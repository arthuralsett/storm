#include <vector>

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
    }  // namespace cmdp
}  // namespace storm
