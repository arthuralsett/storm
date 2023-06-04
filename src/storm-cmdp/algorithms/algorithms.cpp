#include "storm-cmdp/algorithms/algorithms.h"
#include <algorithm>
#include <functional>
#include <optional>

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
            int argMin;  // An action.
            storm::utility::ExtendedInteger minSprVal;
            for (int a = 0; a < numberOfActions; ++a) {
                auto sprVal = computeSprVal(cmdp, safe, state, a, resourceLevels);
                if (sprVal < minSprVal) {
                    argMin = a;
                    minSprVal = sprVal;
                }
            }
            return argMin;
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
    int capacity,
    storm::storage::BitVector targetStates
) {
    using ExtInt = storm::utility::ExtendedInteger;

    const int numberOfStates = cmdp->getNumberOfStates();
    const int numberOfActions = cmdp->getNumberOfChoices(0);
    auto reloadStates = cmdp->getStates("reload");
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
