#include <memory>
#include <ostream>
#include <vector>
#include "storm/models/sparse/Mdp.h"

namespace storm {
    namespace cmdp {
        const int undefinedAction = -1;

        // If the value is supposed to be undefined (in my thesis: bottom),
        // set the value to `undefinedAction`.
        using SelectionRule = std::vector<int>;

        // Mathematically, a function from set of states to set of selection
        // rules. States are represented by integers, so this can be implemented
        // by a vector.
        using CounterSelector = std::vector<SelectionRule>;

        // Prints a table/matrix representing `counterSelector`.
        // Rows correspond to states, columns to resource levels, and elements (roughly)
        // to which action the agent should take if in that state with that resource level.
        // Prints newline at the end.
        void printCounterSelector(
            std::ostream& out,
            storm::cmdp::CounterSelector counterSelector,
            std::shared_ptr<storm::models::sparse::Mdp<double, storm::models::sparse::StandardRewardModel<double>>> cmdp,
            int capacity
        );
    }  // namespace cmdp
}  // namespace storm
