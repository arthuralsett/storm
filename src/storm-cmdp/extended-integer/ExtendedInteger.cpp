#include "ExtendedInteger.h"
#include <stdexcept>

storm::utility::ExtendedInteger::ExtendedInteger(int value) :mIsInfinite{false}, value{value} {}

bool storm::utility::ExtendedInteger::operator< (const ExtendedInteger& rhs) const {
    // For performance check this first, since most often finite values will be
    // compared.
    if (this->isFinite() && rhs.isFinite()) {
        return this->value < rhs.value;
    } else if ((this->isInfinite() && this->value > 0) ||
               (rhs.isInfinite() && rhs.value < 0)) {
        // LHS is +infinity or RHS is -infinity.
        return false;
    } else {
        return true;
    }
}

bool storm::utility::ExtendedInteger::operator> (const ExtendedInteger& rhs) const {
    return rhs < *this;
}

bool storm::utility::ExtendedInteger::operator<= (const ExtendedInteger& rhs) const {
    return !(*this > rhs);
}

bool storm::utility::ExtendedInteger::operator>= (const ExtendedInteger& rhs) const {
    return !(*this < rhs);
}

storm::utility::ExtendedInteger storm::utility::ExtendedInteger::operator- () const {
    auto result = *this;
    result.value *= -1;
    return result;
}

bool storm::utility::ExtendedInteger::isFinite() const { return !this->mIsInfinite; }

bool storm::utility::ExtendedInteger::isInfinite() const { return this->mIsInfinite; }

int storm::utility::ExtendedInteger::getValue() const {
    if (this->mIsInfinite) {
        throw std::range_error("Cannot represent infinite value by an integer.");
    }
    return this->value;
}

int storm::utility::ExtendedInteger::sign() const {
    if (this->value > 0) {
        return 1;
    } else if (this->value < 0) {
        return -1;
    } else {
        return 0;
    }
}

storm::utility::ExtendedInteger storm::utility::ExtendedInteger::infinity() { return storm::utility::ExtendedInteger(true); }

storm::utility::ExtendedInteger::ExtendedInteger(bool isInfinite) :mIsInfinite{isInfinite}, value{1} {}

std::ostream& operator<< (std::ostream& out, const storm::utility::ExtendedInteger& x) {
    if (x.isInfinite()) {
        return out << (x.sign() < 0 ? "-" : "") << "infinity";
    } else {
        return out << x.getValue();
    }
}

storm::utility::ExtendedInteger operator+ (const storm::utility::ExtendedInteger& lhs, const storm::utility::ExtendedInteger& rhs) {
    if (lhs.isFinite() && rhs.isFinite()) {
        return lhs.getValue() + rhs.getValue();
    }
    if (lhs.isInfinite() && rhs.isInfinite() && lhs.sign() != rhs.sign()) {
        throw std::invalid_argument("Mathematically undefined operation: adding infinite numbers with opposite sign.");
    }
    if (lhs.isInfinite()) {
        // `rhs` is finite or has same sign as `lhs`.
        return lhs;
    } else {
        // `lhs` is finite.
        return rhs;
    }
}

bool operator== (const storm::utility::ExtendedInteger& lhs, const storm::utility::ExtendedInteger& rhs) {
    if (lhs.isFinite()) {
        return rhs.isFinite() && lhs.getValue() == rhs.getValue();
    } else {
        return rhs.isInfinite() && lhs.sign() == rhs.sign();
    }
}
