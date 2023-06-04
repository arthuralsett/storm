#include <utility>
#include <vector>
#include <memory>
#include "storm/models/sparse/Mdp.h"

#include "storm-cmdp/extended-integer/ExtendedInteger.h"
#include "storm-cmdp/CounterSelector.h"

namespace storm {
    namespace cmdp {
        // Compute MinInitCons for `cmdp`.
        std::vector<storm::utility::ExtendedInteger> computeMinInitCons(
            std::shared_ptr<storm::models::sparse::Mdp<double, storm::models::sparse::StandardRewardModel<double>>> cmdp
        );

        // Compute MinInitCons for `cmdp` where its set of reload states is replaced by `newReloadStates`.
        std::vector<storm::utility::ExtendedInteger> computeMinInitCons(
            std::shared_ptr<storm::models::sparse::Mdp<double, storm::models::sparse::StandardRewardModel<double>>> cmdp,
            storm::storage::BitVector newReloadStates
        );

        // Compute Safe for `cmdp`.
        std::vector<storm::utility::ExtendedInteger> computeSafe(
            std::shared_ptr<storm::models::sparse::Mdp<double, storm::models::sparse::StandardRewardModel<double>>> cmdp,
            int capacity
        );

        // Returns a vector with for each state a safe action (represented by an integer).
        std::vector<int> getSafeActions(
            std::shared_ptr<storm::models::sparse::Mdp<double, storm::models::sparse::StandardRewardModel<double>>> cmdp,
            const std::vector<storm::utility::ExtendedInteger>& safe,
            int capacity
        );

        // Compute SafePR for `cmdp` and a corresponding counter selector.
        std::pair<std::vector<storm::utility::ExtendedInteger>, storm::cmdp::CounterSelector> computeSafePr(
            std::shared_ptr<storm::models::sparse::Mdp<double, storm::models::sparse::StandardRewardModel<double>>> cmdp,
            int capacity,
            storm::storage::BitVector targetStates
        );
    }  // namespace cmdp
}  // namespace storm
