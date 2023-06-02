#include "storm-cmdp/algorithms/algorithms.h"

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
        // Returns the maximum energy level `resourceLevels[t]` where `t` is
        // a potential successor if taking `action` at `state`.
        storm::utility::ExtendedInteger maxOverSuccessors(
            std::shared_ptr<storm::models::sparse::Mdp<double, storm::models::sparse::StandardRewardModel<double>>> cmdp,
            int state,
            int action,
            const std::vector<storm::utility::ExtendedInteger>& resourceLevels
        ) {
            storm::utility::ExtendedInteger max(0);
            // Probability distribution over the set of states.
            auto successorDistribution = cmdp->getTransitionMatrix().getRow(state, action);
            for (const auto& entry : successorDistribution) {
                size_t successor = entry.getColumn();
                double probability = entry.getValue();
                // First condition means `successor` is actually a successor.
                if (probability > 0 && resourceLevels.at(successor) > max) {
                    max = resourceLevels.at(successor);
                }
            }
            return max;
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
