#include "storm-cmdp/algorithms/algorithms.h"
#include <algorithm>
#include <functional>
#include <optional>
#include "storm/environment/Environment.h"
#include "storm/logic/AtomicLabelFormula.h"
#include "storm/logic/EventuallyFormula.h"
#include "storm/modelchecker/prctl/SparseMdpPrctlModelChecker.h"
#include "storm/modelchecker/results/CheckResult.h"
#include "storm/modelchecker/results/ExplicitQuantitativeCheckResult.h"
#include "storm/solver/OptimizationDirection.h"
#include "storm/storage/BitVector.h"
#include "storm/storage/SparseMatrix.h"
#include "storm/utility/graph.h"

namespace storm {
    namespace cmdp {
        // Helper function: not declared in header.
        // Returns the cost of taking `action` at `state`. Mathematical notation: "C(state, action)".
        storm::utility::ExtendedInteger cost(
            std::shared_ptr<storm::models::sparse::Mdp<double, storm::models::sparse::StandardRewardModel<double>>> cmdp,
            int state,
            int action
        ) {
            auto costs = cmdp->getRewardModel("cost");
            auto choiceIndex = cmdp->getChoiceIndex(storm::storage::StateActionPair(state, action));
            int cost = costs.getStateActionReward(choiceIndex);
            return storm::utility::ExtendedInteger(cost);
        }

        // Helper function: not declared in header.
        // Returns the value `x` such that `compare(getValueForSuccessor(t), x)` is
        // false for all successors `t` of taking `action` at `state`, excluding `excludedSuccessor`.
        // If `compare` implements "<" then never `getValueForSuccessor(t) < x` so `x` is the minimum.
        // If `excludedSuccessor` is the only successor, the return value is an empty optional.
        std::optional<storm::utility::ExtendedInteger> overSuccessors(
            std::shared_ptr<storm::models::sparse::Mdp<double, storm::models::sparse::StandardRewardModel<double>>> cmdp,
            int state,
            int action,
            std::function<bool(storm::utility::ExtendedInteger, storm::utility::ExtendedInteger)> compare,
            std::function<storm::utility::ExtendedInteger(int)> getValueForSuccessor,
            int excludedSuccessor
        ) {
            storm::utility::ExtendedInteger result;
            bool firstSuccessor = true;
            // Probability distribution over the set of states.
            auto successorDistribution = cmdp->getTransitionMatrix().getRow(state, action);

            for (const auto& entry : successorDistribution) {
                size_t successor = entry.getColumn();
                double probability = entry.getValue();
                if (probability > 0 && successor != excludedSuccessor) {
                    // `successor` is actually a successor.
                    auto valueForSuccessor = getValueForSuccessor(successor);
                    if (firstSuccessor) {
                        result = valueForSuccessor;
                        firstSuccessor = false;
                    } else if (compare(valueForSuccessor, result)) {
                        result = valueForSuccessor;
                    }
                }
            }
            if (firstSuccessor) {
                // Haven't found a successor, so `excludedSuccessor` was the only successor.
                return {};
            }
            return {result};
        }

        // Same as the above overload, but doesn't exclude any successors. Hence there is always a value,
        // because with each action there should be at least one successor. (If the input is a proper CMDP.)
        storm::utility::ExtendedInteger overSuccessors(
            std::shared_ptr<storm::models::sparse::Mdp<double, storm::models::sparse::StandardRewardModel<double>>> cmdp,
            int state,
            int action,
            std::function<bool(storm::utility::ExtendedInteger, storm::utility::ExtendedInteger)> compare,
            std::function<storm::utility::ExtendedInteger(int)> getValueForSuccessor
        ) {
            // `-1` will not match any successor state.
            return overSuccessors(cmdp, state, action, compare, getValueForSuccessor, -1).value();
        }

        // Helper function: not declared in header.
        // Returns the maximum energy level `resourceLevels[t]` where `t` is
        // a potential successor if taking `action` at `state`.
        storm::utility::ExtendedInteger maxOverSuccessors(
            std::shared_ptr<storm::models::sparse::Mdp<double, storm::models::sparse::StandardRewardModel<double>>> cmdp,
            int state,
            int action,
            const std::vector<storm::utility::ExtendedInteger>& resourceLevels
        ) {
            return overSuccessors(cmdp, state, action, std::greater{},
                [&resourceLevels](int successor) {
                    return resourceLevels.at(successor);
                }
            );
        }

        // Helper function: not declared in header.
        // See definition of "SPR-Val" in my thesis.
        storm::utility::ExtendedInteger maxOfSos(
            std::shared_ptr<storm::models::sparse::Mdp<double, storm::models::sparse::StandardRewardModel<double>>> cmdp,
            const std::vector<storm::utility::ExtendedInteger>& safe,
            int state,
            int action,
            const std::vector<storm::utility::ExtendedInteger>& resourceLevels,
            int currentSuccessor
        ) {
            auto intermediateMax = overSuccessors(cmdp, state, action, std::greater{},
                [&safe](int successor) {
                    return safe.at(successor);
                },
                currentSuccessor
            );
            auto resourceLvl = resourceLevels.at(currentSuccessor);
            if (!intermediateMax.has_value()) {
                return resourceLvl;
            } else {
                return std::max(resourceLvl, intermediateMax.value());
            }
        }

        // Helper function: not declared in header.
        // Compute "SPR-Val(state, action, resourceLevels)".
        // Pass vector `safe` as argument to avoid recomputing.
        storm::utility::ExtendedInteger computeSprVal(
            std::shared_ptr<storm::models::sparse::Mdp<double, storm::models::sparse::StandardRewardModel<double>>> cmdp,
            const std::vector<storm::utility::ExtendedInteger>& safe,
            int state,
            int action,
            const std::vector<storm::utility::ExtendedInteger>& resourceLevels
        ) {
            auto min = overSuccessors(cmdp, state, action, std::less{},
                [=, &safe, &resourceLevels] (int successor) {
                    return maxOfSos(cmdp, safe, state, action, resourceLevels, successor);
                }
            );
            return cost(cmdp, state, action) + min;
        }

        // Helper function: not declared in header.
        // Returns the `action` for which `computeSprVal(cmdp, safe, state, action, resourceLevels)` is minimal.
        // Pass vector `safe` as argument to avoid recomputing.
        int getActionMinimisingSprVal(
            std::shared_ptr<storm::models::sparse::Mdp<double, storm::models::sparse::StandardRewardModel<double>>> cmdp,
            const std::vector<storm::utility::ExtendedInteger>& safe,
            int state,
            const std::vector<storm::utility::ExtendedInteger>& resourceLevels
        ) {
            const int numberOfActions = cmdp->getNumberOfChoices(0);
            int argMin = 0;  // An action.
            storm::utility::ExtendedInteger minSprVal = computeSprVal(cmdp, safe, state, argMin, resourceLevels);
            for (int a = 1; a < numberOfActions; ++a) {
                auto sprVal = computeSprVal(cmdp, safe, state, a, resourceLevels);
                if (sprVal < minSprVal) {
                    argMin = a;
                    minSprVal = sprVal;
                }
            }
            return argMin;
        }

        // Returns which action the agent should take next according to
        // `counterSelector` if the agent is in `state` with
        // `resourceLevel` units of energy. If the selection rule corresponding
        // to `state` has no value <= `resourceLevel` for which it is "defined"
        // (not bottom) then the default action is zero. (Reason for this
        // choice: there is always at least one action, so the action zero
        // always exists.)
        int getNextAction(
            const storm::cmdp::CounterSelector& counterSelector,
            int state,
            int resourceLevel
        ) {
            auto selectionRule = counterSelector.at(state);
            // Want to find greatest `x` with defined value, so start from top
            // and decrement.
            for (int x = resourceLevel; x >= 0; --x) {
                int nextAction = selectionRule.at(x);
                if (nextAction != storm::cmdp::undefinedAction) {
                    return nextAction;
                }
            }
            // Didn't find x <= `resourceLevel` with defined value, so return
            // default action.
            return 0;
        }

        // Returns the state that corresponds to the
        // pair (`originalState`, `currentResourceLevel`).
        int getStateWithBuiltInResourceLevel(
            int originalState,
            int currentResourceLevel,
            int numberOfResourceLevels
        ) {
            return originalState * numberOfResourceLevels + currentResourceLevel;
        }

        // When creating a CMDP with states that correspond to
        // pairs (state, resource level), this function returns an integer
        // one-past-the-last (normal) state, representing a state where the
        // agent has no resource left.
        int getStateWithZeroResource(
            int numberOfStates,
            int capacity
        ) {
            return getStateWithBuiltInResourceLevel(numberOfStates - 1, capacity + 1, capacity + 1);
        }

        // Returns a transition matrix with states that conceptually correspond
        // to pairs (s, rl) where s is a state from `cmdp` and rl is a resource
        // level with 0 <= rl <= `capacity`. The transitions correspond to what
        // `counterSelector` would choose.
        storm::storage::SparseMatrix<double> getTransitionMatrixAccordingToCounterSelector(
            const storm::cmdp::CounterSelector& counterSelector,
            std::shared_ptr<storm::models::sparse::Mdp<double, storm::models::sparse::StandardRewardModel<double>>> cmdp,
            int capacity
        ) {
            const int numberOfStates = cmdp->getNumberOfStates();
            const int numberOfResoureLevels = capacity + 1;
            const int newNumberOfStates = numberOfStates * numberOfResoureLevels + 1;
            storm::storage::SparseMatrixBuilder<double> matrixBuilder(newNumberOfStates, newNumberOfStates);
            const int stateWithZeroResource = getStateWithZeroResource(numberOfStates, capacity);
            auto reloadStates = cmdp->getStates("reload");

            // States from original CMDP.
            for (int state = 0; state < numberOfStates; ++state) {
                bool leavingReloadState = reloadStates.get(state);
                // Possible resource levels.
                for (int resLvl = 0; resLvl <= capacity; ++resLvl) {
                    const int newState = getStateWithBuiltInResourceLevel(state, resLvl, numberOfResoureLevels);
                    auto action = getNextAction(counterSelector, state, resLvl);
                    auto cost = storm::cmdp::cost(cmdp, state, action).getValue();
                    int nextResourceLevel = leavingReloadState ? (capacity - cost) : (resLvl - cost);

                    if (nextResourceLevel < 0) {
                        matrixBuilder.addNextValue(newState, stateWithZeroResource, 1);
                    } else {
                        // Probability distribution over the set of states.
                        auto successorDistribution = cmdp->getTransitionMatrix().getRow(state, action);

                        // Successor states from original CMDP.
                        for (const auto& entry : successorDistribution) {
                            size_t successor = entry.getColumn();
                            double probability = entry.getValue();
                            if (probability > 0) {
                                // `successor` is actually a successor.

                                auto newSuccessor = getStateWithBuiltInResourceLevel(successor, nextResourceLevel, numberOfResoureLevels);
                                matrixBuilder.addNextValue(newState, newSuccessor, probability);
                            }
                        }
                    }
                }
            }
            // When no resource left, it stays that way forever.
            matrixBuilder.addNextValue(stateWithZeroResource, stateWithZeroResource, 1);
            // TODO maybe call `makeRowGroupingTrivial()`.
            return matrixBuilder.build();
        }

        // Returns a `StateLabeling` indicating target states for an MDP with
        // states representing pairs (s, rl) where s is a state from `cmdp` and
        // rl a resource level. The returned `StateLabeling` doesn't label
        // reload states, because the recharging mechanics are built into the
        // transitions of the new MDP.
        storm::models::sparse::StateLabeling getStateLabellingForBuiltInResourceLevels(
            std::shared_ptr<storm::models::sparse::Mdp<double, storm::models::sparse::StandardRewardModel<double>>> cmdp,
            int capacity
        ) {
            const int numberOfStates = cmdp->getNumberOfStates();
            const int numberOfResoureLevels = capacity + 1;
            const int newNumberOfStates = numberOfStates * numberOfResoureLevels + 1;
            storm::models::sparse::StateLabeling stateLabelling(newNumberOfStates);
            const std::string targetLabel = "target";
            auto targetStates = cmdp->getStates(targetLabel);
            stateLabelling.addLabel(targetLabel);

            // States from original CMDP.
            for (int state = 0; state < numberOfStates; ++state) {
                // Possible resource levels.
                for (int resLvl = 0; resLvl <= capacity; ++resLvl) {
                    if (targetStates.get(state)) {
                        const int newState = getStateWithBuiltInResourceLevel(state, resLvl, numberOfResoureLevels);
                        stateLabelling.addLabelToState(targetLabel, newState);
                    }
                }
            }
            return stateLabelling;
        }

        // Transform `cmdp` into an MDP with states that conceptually correspond
        // to pairs (s, rl) where s is a state from `cmdp` and rl is a resource
        // level with 0 <= rl <= `capacity`. The transitions correspond to what
        // `counterSelector` would choose.
        storm::models::sparse::Mdp<double, storm::models::sparse::StandardRewardModel<double>>
        getMdpWithResourceLevelsBuiltIntoStates(
            const storm::cmdp::CounterSelector& counterSelector,
            std::shared_ptr<storm::models::sparse::Mdp<double, storm::models::sparse::StandardRewardModel<double>>> cmdp,
            const int capacity
        ) {
            auto transitionMatrix = getTransitionMatrixAccordingToCounterSelector(counterSelector, cmdp, capacity);
            auto stateLabelling = getStateLabellingForBuiltInResourceLevels(cmdp, capacity);
            return {transitionMatrix, stateLabelling};
        }

        // Returns an object with for each state the probability of reaching a
        // state labelled "target".
        // `transformedMdp` should be the output of
        // `storm::cmdp::getMdpWithResourceLevelsBuiltIntoStates()`.
        storm::modelchecker::ExplicitQuantitativeCheckResult<double> getProbabilitiesForReachingTargetState(
            const storm::models::sparse::Mdp<double, storm::models::sparse::StandardRewardModel<double>>& transformedMdp
        ) {
            storm::modelchecker::SparseMdpPrctlModelChecker checker(transformedMdp);
            auto isTarget = std::make_shared<storm::logic::AtomicLabelFormula>("target");
            storm::logic::EventuallyFormula formulaForTarget(isTarget);
            storm::modelchecker::CheckTask<storm::logic::EventuallyFormula> checkReachTarget(formulaForTarget);
            // Shouldn't make a difference because there are no choices. There is only
            // one action in each state. But some function below requires this.
            checkReachTarget.setOptimizationDirection(storm::solver::OptimizationDirection::Maximize);

            auto resultReachTarget = checker.computeReachabilityProbabilities(storm::Environment{}, checkReachTarget);
            if (!resultReachTarget->isExplicitQuantitativeCheckResult()) {
                throw storm::exceptions::BaseException("Expected ExplicitQuantitativeCheckResult.");
            }
            return resultReachTarget->asExplicitQuantitativeCheckResult<double>();
        }

        // Returns a bit vector with bits set exactly for the states s such that
        // if the agent starts in s, it never reaches `undesiredState`.
        storm::storage::BitVector getStatesFromWhichNeverReach(
            const storm::models::sparse::Mdp<double, storm::models::sparse::StandardRewardModel<double>>& mdp,
            const int undesiredState
        ) {
            using BitVec = storm::storage::BitVector;
            const int numberOfStates = mdp.getNumberOfStates();
            BitVec statesWithProbZero(numberOfStates);
            BitVec statesWithProbOne(numberOfStates);
            BitVec allStatesTrue(numberOfStates, true);
            BitVec badStates(numberOfStates, false);
            badStates.set(undesiredState);
            std::tie(statesWithProbZero, statesWithProbOne) = storm::utility::graph::performProb01(
                mdp.getBackwardTransitions(),
                allStatesTrue,
                badStates
            );
            return statesWithProbZero;
        }
    }  // namespace cmdp
}  // namespace storm

std::vector<storm::utility::ExtendedInteger> storm::cmdp::computeMinInitCons(
    std::shared_ptr<storm::models::sparse::Mdp<double, storm::models::sparse::StandardRewardModel<double>>> cmdp
) {
    return computeMinInitCons(cmdp, cmdp->getStates("reload"));
}

std::vector<storm::utility::ExtendedInteger> storm::cmdp::computeMinInitCons(
    std::shared_ptr<storm::models::sparse::Mdp<double, storm::models::sparse::StandardRewardModel<double>>> cmdp,
    storm::storage::BitVector newReloadStates
) {
    using ExtInt = storm::utility::ExtendedInteger;

    const int numberOfStates = cmdp->getNumberOfStates();
    const int numberOfActions = cmdp->getNumberOfChoices(0);
    auto reloadStates = newReloadStates;

    std::vector<ExtInt> minInitConsApprox(numberOfStates, ExtInt::infinity());
    auto minInitConsOldApprox = minInitConsApprox;
    do {
        minInitConsOldApprox = minInitConsApprox;
        // Loop over states.
        for (int s = 0; s < numberOfStates; ++s) {
            // Minimum amount of fuel to guarantee reaching a reload state, where
            // the minimum is taken over all actions.
            auto costUntilReload = ExtInt::infinity();
            // Loop over actions.
            for (int a = 0; a < numberOfActions; ++a) {
                ExtInt costForThisStep = cost(cmdp, s, a);

                // Apply truncation operator to `minInitConsOldApprox`.
                auto truncatedMinInitConsOldApprox = minInitConsOldApprox;
                for (int s = 0; s < numberOfStates; ++s) {
                    if (reloadStates.get(s)) {
                        truncatedMinInitConsOldApprox.at(s) = ExtInt(0);
                    }
                }
                // Amount of fuel needed to guarantee reaching a reload state *after*
                // taking action `a`. (Cost for action `a` excluded, hence "remaining".)
                auto remainingCostUntilReload = maxOverSuccessors(cmdp, s, a, truncatedMinInitConsOldApprox);

                if (costForThisStep + remainingCostUntilReload < costUntilReload) {
                    costUntilReload = costForThisStep + remainingCostUntilReload;
                }
            }
            if (costUntilReload < minInitConsApprox.at(s)) {
                minInitConsApprox.at(s) = costUntilReload;
            }
        }
    } while (minInitConsApprox != minInitConsOldApprox);

    return minInitConsApprox;
}

std::vector<storm::utility::ExtendedInteger> storm::cmdp::computeSafe(
    std::shared_ptr<storm::models::sparse::Mdp<double, storm::models::sparse::StandardRewardModel<double>>> cmdp,
    int capacity
) {
    using ExtInt = storm::utility::ExtendedInteger;
    // Initially the set of reload states, but certain reload states will be "removed".
    auto rel = cmdp->getStates("reload");
    const int numberOfStates = cmdp->getNumberOfStates();
    std::vector<ExtInt> minInitCons(numberOfStates);

    bool madeChange = false;
    do {
        madeChange = false;
        minInitCons = computeMinInitCons(cmdp, rel);
        for (int s = 0; s < numberOfStates; ++s) {
            if (rel.get(s)) {
                // `s` is in `rel`.
                if (minInitCons.at(s) > capacity) {
                    rel.set(s, false);  // Remove `s` from `rel`.
                    madeChange = true;
                }
            }
        }
    } while (madeChange);

    auto out = minInitCons;
    for (int s = 0; s < numberOfStates; ++s) {
        if (rel.get(s)) {
            out.at(s) = 0;
        } else if (out.at(s) > capacity) {
            out.at(s) = ExtInt::infinity();
        }
    }
    return out;
}

std::vector<int> storm::cmdp::getSafeActions(
    std::shared_ptr<storm::models::sparse::Mdp<double, storm::models::sparse::StandardRewardModel<double>>> cmdp,
    const std::vector<storm::utility::ExtendedInteger>& safe,
    int capacity
) {
    using ExtInt = storm::utility::ExtendedInteger;
    const int numberOfStates = cmdp->getNumberOfStates();
    const int numberOfActions = cmdp->getNumberOfChoices(0);
    auto reloadStates = cmdp->getStates("reload");
    std::vector<int> safeActions(numberOfStates);

    for (int s = 0; s < numberOfStates; ++s) {
        ExtInt maxCost(reloadStates.get(s) ? capacity : safe.at(s));
        bool foundSafeAction = false;
        // Check all actions; stop when find a safe action.
        for (int a = 0; a < numberOfActions && !foundSafeAction; ++a) {
            if (cost(cmdp, s, a) + maxOverSuccessors(cmdp, s, a, safe) <= maxCost) {
                foundSafeAction = true;
                safeActions.at(s) = a;
            }
        }
    }
    return safeActions;
}

std::pair<std::vector<storm::utility::ExtendedInteger>, storm::cmdp::CounterSelector> storm::cmdp::computeSafePr(
    std::shared_ptr<storm::models::sparse::Mdp<double, storm::models::sparse::StandardRewardModel<double>>> cmdp,
    int capacity
) {
    using ExtInt = storm::utility::ExtendedInteger;

    const int numberOfStates = cmdp->getNumberOfStates();
    const int numberOfActions = cmdp->getNumberOfChoices(0);
    auto reloadStates = cmdp->getStates("reload");
    auto targetStates = cmdp->getStates("target");
    std::vector<ExtInt> safePrApprox(numberOfStates, ExtInt::infinity());
    auto safe = computeSafe(cmdp, capacity);
    auto safeActions = getSafeActions(cmdp, safe, capacity);

    SelectionRule emptySelectionRule(capacity+1, undefinedAction);
    CounterSelector counterSelector(numberOfStates, emptySelectionRule);
    for (int s = 0; s < numberOfStates; ++s) {
        if (safe.at(s) < ExtInt::infinity()) {
            counterSelector.at(s).at(safe.at(s).getValue()) = safeActions.at(s);
        }
    }
    for (int s = 0; s < numberOfStates; ++s) {
        if (targetStates.get(s)) {
            // `s` is target state.
            safePrApprox.at(s) = safe.at(s);
        }
    }
    // Needs to be remembered between iterations of the do-while loop.
    auto safePrOldApprox = safePrApprox;
    do {
        safePrOldApprox = safePrApprox;
        std::vector<int> a(numberOfStates);
        for (int s = 0; s < numberOfStates; ++s) {
            if (!targetStates.get(s)) {
                // `s` in the set S \ T.

                a.at(s) = getActionMinimisingSprVal(cmdp, safe, s, safePrOldApprox);

                // Take minimum of "SPR-Val" over all actions.
                // TODO put this in separate function.
                auto minSprVal = ExtInt::infinity();
                for (int a = 0; a < numberOfActions; ++a) {
                    auto potentialMinSprVal = computeSprVal(cmdp, safe, s, a, safePrOldApprox);
                    if (potentialMinSprVal < minSprVal) {
                        minSprVal = potentialMinSprVal;
                    }
                }
                safePrApprox.at(s) = minSprVal;
            }
        }
        // Apply two-sided truncation operator to `safePrApprox`.
        // TODO put this in separate function.
        for (int s = 0; s < numberOfStates; ++s) {
            if (safePrApprox.at(s) > ExtInt(capacity)) {
                safePrApprox.at(s) = ExtInt::infinity();
            } else if (reloadStates.get(s)) {
                // `s` is a reload state.
                safePrApprox.at(s) = ExtInt(0);
            }
        }
        for (int s = 0; s < numberOfStates; ++s) {
            if (!targetStates.get(s) && safePrApprox.at(s) < safePrOldApprox.at(s)) {
                counterSelector.at(s).at(safePrApprox.at(s).getValue()) = a.at(s);
            }
        }
    } while (safePrOldApprox != safePrApprox);

    return {safePrApprox, counterSelector};
}

bool storm::cmdp::validateCounterSelector(
    const storm::cmdp::CounterSelector& counterSelector,
    std::shared_ptr<storm::models::sparse::Mdp<double, storm::models::sparse::StandardRewardModel<double>>> cmdp,
    const std::vector<storm::utility::ExtendedInteger>& safePr,
    const int capacity
) {
    const int numberOfStates = cmdp->getNumberOfStates();
    const int numberOfResourceLevels = capacity + 1;

    auto transformedMdp = getMdpWithResourceLevelsBuiltIntoStates(counterSelector, cmdp, capacity);
    auto resultReachTarget = getProbabilitiesForReachingTargetState(transformedMdp);

    auto stateWithZeroResource = getStateWithZeroResource(numberOfStates, capacity);
    // States for which the probability that the agent runs out of energy, is zero.
    auto safeStates = getStatesFromWhichNeverReach(transformedMdp, stateWithZeroResource);

    // These two variables indicate whether the counter selector ensures that
    // for each state s with "SafePR(s)" <= `capacity`, ...
    // ... the probability of reaching target state is not zero. Assume true and
    // look for counter-example.
    bool countSelEnsuresTarget = true;
    // ... the agent never runs out of energy. Assume true like above.
    bool countSelEnsuresResource = true;

    bool counterExampleForBoth;
    auto infinity = storm::utility::ExtendedInteger::infinity();
    // Loop over states from original CMDP. Stop when we have a counter-example
    // for resource and target.
    for (int s = 0; s < numberOfStates && !counterExampleForBoth; ++s) {
        // If value is infinity, we don't need to check anything.
        if (safePr.at(s) < infinity) {
            // Need to know that the counter selector satisfies the requirements
            // if agent starts with enough energy. Enough energy for s is
            // "SafePR(s)".
            auto transformedState = getStateWithBuiltInResourceLevel(s, safePr.at(s).getValue(), numberOfResourceLevels);
            auto targetProbability = resultReachTarget[transformedState];
            if (targetProbability <= 0) {
                countSelEnsuresTarget = false;
            }
            if (!safeStates.get(transformedState)) {
                countSelEnsuresResource = false;
            }
        }
        counterExampleForBoth = !countSelEnsuresTarget && !countSelEnsuresResource;
    }
    return countSelEnsuresTarget && countSelEnsuresResource;
}
