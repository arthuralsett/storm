#include "storm-cmdp/algorithms/algorithms.h"

std::vector<storm::utility::ExtendedInteger> storm::cmdp::computeMinInitCons(std::shared_ptr<storm::models::sparse::Mdp<double, storm::models::sparse::StandardRewardModel<double>>> cmdp) {
    using ExtInt = storm::utility::ExtendedInteger;

    std::vector<ExtInt> minInitConsApprox(cmdp->getNumberOfStates(), ExtInt::infinity());

    const int numberOfActions = cmdp->getNumberOfChoices(0);
    const auto transitionMatrix = cmdp->getTransitionMatrix();
    auto costs = cmdp->getRewardModel("cost");
    auto reloadStates = cmdp->getStates("reload");

    auto minInitConsOldApprox = minInitConsApprox;
    do {
        minInitConsOldApprox = minInitConsApprox;
        // Loop over states.
        for (int s = 0; s < minInitConsApprox.size(); ++s) {
            // Minimum amount of fuel to guarantee reaching a reload state, where
            // the minimum is taken over all actions.
            auto costUntilReload = ExtInt::infinity();
            // Loop over actions.
            for (int a = 0; a < numberOfActions; ++a) {
                auto choiceIndex = cmdp->getChoiceIndex(storm::storage::StateActionPair(s, a));
                // Cost of taking action `a`. Mathematical notation: "C(s,a)".
                ExtInt costForThisStep(static_cast<int>(costs.getStateActionReward(choiceIndex)));

                // Probability distribution over the set of states.
                auto successorDistribution = transitionMatrix.getRow(s, a);

                // Amount of fuel needed to guarantee reaching a reload state *after*
                // taking action `a`. (Cost for action `a` excluded, hence "remaining".)
                ExtInt remainingCostUntilReload(0);
                // Loop over successors. `remainingCostUntilReload` becomes maximum.
                for (const auto& entry : successorDistribution) {
                    size_t successor = entry.getColumn();
                    double probability = entry.getValue();
                    // First condition means `successor` is actually a successor.
                    // Second condition means `successor` is not a reload state.
                    if (probability > 0 && !reloadStates.get(successor) && minInitConsOldApprox.at(successor) > remainingCostUntilReload) {
                        remainingCostUntilReload = minInitConsOldApprox.at(successor);
                    }
                }
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
