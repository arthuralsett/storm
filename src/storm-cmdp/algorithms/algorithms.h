#include <vector>
#include <memory>
#include "storm/models/sparse/Mdp.h"

#include "storm-cmdp/extended-integer/ExtendedInteger.h"

namespace storm {
    namespace cmdp {
        std::vector<storm::utility::ExtendedInteger> computeMinInitCons(std::shared_ptr<storm::models::sparse::Mdp<double, storm::models::sparse::StandardRewardModel<double>>> cmdp);
    }  // namespace cmdp
}  // namespace storm
