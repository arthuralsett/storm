#include <vector>
#include <memory>
#include <algorithm>

#include "storm/models/sparse/Mdp.h"

namespace storm {
    namespace utility {
        // Precondition: `valuation` is of the form "[s=INT]" with "INT" some integer >= 0.
        // Returns "INT" converted to an integer, which is the original state of
        // `s` if this function was called with
        // `auto valuation = valuations.toString(s)`.
        int originalState(const std::string& valuation);

        // Returns `in` with elements sorted based on actual state (from input model, e.g. PRISM).
        template<typename T>
        std::vector<T> undoStatePermutation(const std::vector<T>& in, std::shared_ptr<storm::models::sparse::Mdp<double, storm::models::sparse::StandardRewardModel<double>>> mdp) {
            if (!mdp->hasStateValuations()) {
                return in;  // Nothing to do.
            }
            auto valuations = mdp->getStateValuations();
            // Pair value with original state.
            std::vector<std::pair<T,int>> paired;
            for (int s = 0; s < in.size(); ++s) {
                int origState = originalState(valuations.toString(s));
                paired.push_back({in.at(s), origState});
            }
            // Sort on second element of the pairs.
            std::sort(paired.begin(), paired.end(),
                [](std::pair<T,int> a, std::pair<T,int> b) { return a.second < b.second; }
            );
            // Discard state, keep value.
            std::vector<T> out(in.size());
            for (int s = 0; s < out.size(); ++s) {
                out.at(s) = paired.at(s).first;
            }
            return out;
        }
    }  // namespace utility
}  // namespace storm
