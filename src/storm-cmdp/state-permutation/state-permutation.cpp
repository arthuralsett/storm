#include "storm-cmdp/state-permutation/state-permutation.h"
#include <sstream>

int storm::utility::originalState(const std::string& valuation) {
    const int charsToIgnore = 3;  // Length of "[s=".
    std::istringstream iss(valuation);
    iss.ignore(charsToIgnore);
    int i;
    iss >> i;
    return i;
}
