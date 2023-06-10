// Source: https://wordaligned.org/articles/cpp-streambufs

#ifndef STORM_CMDP_TEE_STREAM_H
#define STORM_CMDP_TEE_STREAM_H

#include <ostream>

namespace storm {
    namespace utility {
        template <typename char_type, typename traits = std::char_traits<char_type>>
        class BasicTeebuf : public std::basic_streambuf<char_type, traits> {
          public:
            typedef typename traits::int_type int_type;

            BasicTeebuf(
                std::basic_streambuf<char_type, traits>* sb1,
                std::basic_streambuf<char_type, traits>* sb2
            ) : sb1(sb1), sb2(sb2) {}

          private:
            virtual int sync() {
                int const r1 = sb1->pubsync();
                int const r2 = sb2->pubsync();
                return r1 == 0 && r2 == 0 ? 0 : -1;
            }

            virtual int_type overflow(int_type c) {
                int_type const eof = traits::eof();

                if (traits::eq_int_type(c, eof)) {
                    return traits::not_eof(c);
                } else {
                    char_type const ch = traits::to_char_type(c);
                    int_type const r1 = sb1->sputc(ch);
                    int_type const r2 = sb2->sputc(ch);

                    return
                        traits::eq_int_type(r1, eof) ||
                        traits::eq_int_type(r2, eof) ? eof : c;
                }
            }

            std::basic_streambuf<char_type, traits>* sb1;
            std::basic_streambuf<char_type, traits>* sb2;
        };

        typedef BasicTeebuf<char> Teebuf;

        class TeeStream : public std::ostream {
          public:
            // Construct an ostream which tees output to the supplied
            // ostreams.
            TeeStream(std::ostream & o1, std::ostream & o2);

          private:
            Teebuf tbuf;
        };
    }  // namespace utility
}  // namespace storm

storm::utility::TeeStream::TeeStream(std::ostream & o1, std::ostream & o2)
    : std::ostream(&tbuf)
    , tbuf(o1.rdbuf(), o2.rdbuf())
{}

#endif
