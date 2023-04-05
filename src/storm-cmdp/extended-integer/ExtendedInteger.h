#include <ostream>

namespace util {
    class ExtendedInteger {
        public:
            ExtendedInteger(int value);

            bool operator< (const util::ExtendedInteger& rhs) const;
            bool operator> (const util::ExtendedInteger& rhs) const;
            bool operator<= (const util::ExtendedInteger& rhs) const;
            bool operator>= (const util::ExtendedInteger& rhs) const;

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
}  // namespace util

// +infinity is printed as "infinity" and -infinity as "-infinity".
std::ostream& operator<< (std::ostream& out, const util::ExtendedInteger& x);

util::ExtendedInteger operator+ (const util::ExtendedInteger& lhs, const util::ExtendedInteger& rhs);

bool operator== (const util::ExtendedInteger& lhs, const util::ExtendedInteger& rhs);
