#include <ostream>

namespace storm {
    namespace utility {
        class ExtendedInteger {
            public:
                ExtendedInteger(int value);

                bool operator< (const ExtendedInteger& rhs) const;
                bool operator> (const ExtendedInteger& rhs) const;
                bool operator<= (const ExtendedInteger& rhs) const;
                bool operator>= (const ExtendedInteger& rhs) const;

                ExtendedInteger operator- () const;

                bool isFinite() const;

                bool isInfinite() const;

                // Throws std::range_error if called on infinite number.
                int getValue() const;

                // Returns
                // -1 if this  < 0
                //  0 if this == 0
                //  1 if this  > 1
                int sign() const;

                // Returns +infinity.
                static ExtendedInteger infinity();

            private:
                bool mIsInfinite;
                int value;

                ExtendedInteger(bool isInfinite);
        };
    }  // namespace utility
}  // namespace storm

// +infinity is printed as "infinity" and -infinity as "-infinity".
std::ostream& operator<< (std::ostream& out, const storm::utility::ExtendedInteger& x);

storm::utility::ExtendedInteger operator+ (const storm::utility::ExtendedInteger& lhs, const storm::utility::ExtendedInteger& rhs);

namespace std {
    bool operator== (const storm::utility::ExtendedInteger& lhs, const storm::utility::ExtendedInteger& rhs);

    bool operator!= (const storm::utility::ExtendedInteger& lhs, const storm::utility::ExtendedInteger& rhs);
}  // namespace std
