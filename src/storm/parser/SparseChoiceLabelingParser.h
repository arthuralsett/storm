#ifndef STORM_PARSER_SPARSECHOICELABELINGPARSER_H_
#define	STORM_PARSER_SPARSECHOICELABELINGPARSER_H_

#include <boost/container/flat_set.hpp>
#include <vector>
#include <string>

namespace storm {
    namespace parser {
        /*!
         * A class providing the functionality to parse a choice labeling.
         */
        class SparseChoiceLabelingParser {
        public:
            /*!
             * Parses the given file and returns the resulting choice labeling.
             * 
             * @param nondeterministicChoiceIndices The indices at which the choices
             * of the states begin.
             * @param filename The name of the file to parse.
             * @return The resulting choice labeling.
             */
            static std::vector<boost::container::flat_set<uint_fast64_t>> parseChoiceLabeling(std::vector<uint_fast64_t> const& nondeterministicChoiceIndices, std::string const& filename);
        };
    }
}

#endif	/* STORM_PARSER_SPARSECHOICELABELINGPARSER_H_ */

