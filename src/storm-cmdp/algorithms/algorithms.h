#include <vector>
#include <memory>
#include "storm/models/sparse/Mdp.h"

#include "storm-cmdp/extended-integer/ExtendedInteger.h"

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
    }  // namespace cmdp
}  // namespace storm
