#include "storm-cmdp/counter-selector/CounterSelector.h"

#include <algorithm>
#include <iomanip>

void storm::cmdp::printCounterSelector(
    std::ostream& out,
    storm::cmdp::CounterSelector counterSelector,
    std::shared_ptr<storm::models::sparse::Mdp<double, storm::models::sparse::StandardRewardModel<double>>> cmdp,
    int capacity
) {
    auto len = [](int x) { return std::to_string(x).length(); };
    int stateColumnWidth = len(cmdp->getNumberOfStates() - 1);
    int otherColumnWidth = std::max(len(capacity), len(cmdp->getNumberOfChoices(0) - 1));
    std::string fill(stateColumnWidth, ' ');
    // Header.
    std::cout << fill << " resource levels:\n"
        << fill;
    // Print values of the resource levels.
    for (int rl = 0; rl <= capacity; ++rl) {
        std::cout << ' ' << std::setw(otherColumnWidth) << rl;
    }
    std::cout << '\n';
    // Header.
    std::cout << std::setw(stateColumnWidth) << 's' << " actions:\n";
    // Values of states and actions.
    for (int s = 0; s < counterSelector.size(); ++s) {
        std::cout << std::setw(stateColumnWidth) << s;
        for (int rl = 0; rl <= capacity; ++rl) {
            int action = counterSelector.at(s).at(rl);
            std::cout << ' ' << std::setw(otherColumnWidth);
            if (action == storm::cmdp::undefinedAction) {
                std::cout << '-';
            } else {
                std::cout << action;
            }
        }
        std::cout << '\n';
    }
}
