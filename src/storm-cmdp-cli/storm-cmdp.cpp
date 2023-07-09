#include "storm-cli-utilities/cli.h"
#include "storm-cli-utilities/model-handling.h"

#include "storm/api/storm.h"

#include "storm/exceptions/BaseException.h"
#include "storm/exceptions/InvalidSettingsException.h"
#include "storm/exceptions/NotSupportedException.h"

#include "storm/models/ModelBase.h"

#include "storm/settings/SettingsManager.h"

#include "storm/io/file.h"
#include "storm/utility/initialize.h"
#include "storm/utility/Stopwatch.h"
#include "storm/utility/macros.h"
#include "storm/utility/Engine.h"

#include "storm/settings/modules/GeneralSettings.h"
#include "storm/settings/modules/CoreSettings.h"
#include "storm/settings/modules/IOSettings.h"
#include "storm/settings/modules/BisimulationSettings.h"
#include "storm/settings/modules/TransformationSettings.h"

#include "storm-cmdp/settings/CmdpSettings.h"

#include <fstream>
#include <ostream>
#include <tuple>
#include <vector>
#include "storm-cmdp/algorithms/algorithms.h"
#include "storm-cmdp/TeeStream.h"
#include "storm-cmdp/state-permutation/state-permutation.h"

using ExtInt = storm::utility::ExtendedInteger;

namespace storm {
    namespace cmdp {
        typedef storm::cli::SymbolicInput SymbolicInput;

        // Returns the capacity of the input CMDP.
        int getCapacity(const storm::prism::Program& inputProgramme) {
            if (!inputProgramme.hasConstant("capacity")) {
                throw storm::exceptions::BaseException("Missing constant `capacity` in input file.");
            }
            auto constantCap = inputProgramme.getConstant("capacity");
            if (!constantCap.isDefined()) {
                throw storm::exceptions::BaseException("Constant `capacity` in input file is not defined.");
            }
            auto expr = constantCap.getExpression();
            if (!expr.hasIntegerType()) {
                throw storm::exceptions::BaseException("Constant `capacity` in input file is not an integer.");
            }
            return expr.evaluateAsInt();
        }

        // Returns object representing the input (referred to as a "programme").
        storm::prism::Program getInputProgramme() {
            auto ioSettings = storm::settings::getModule<storm::settings::modules::IOSettings>();
            if (!ioSettings.isPrismInputSet()) {
                throw storm::exceptions::BaseException("No PRISM input specified.");
            }
            auto modelFileName = ioSettings.getPrismInputFilename();
            return storm::api::parseProgram(modelFileName, false, false);
        }

        // Returns the input CMDP (excluding the capacity).
        std::shared_ptr<storm::models::sparse::Mdp<double, storm::models::sparse::StandardRewardModel<double>>>
        getInputCmdp(const storm::prism::Program& inputProgramme) {
            storm::builder::BuilderOptions buildOptions;
            buildOptions.setBuildStateValuations();
            buildOptions.setBuildChoiceLabels();
            buildOptions.setBuildAllLabels();
            buildOptions.setBuildAllRewardModels();

            auto generator = std::make_shared<storm::generator::PrismNextStateGenerator<double, uint32_t>>(inputProgramme, buildOptions);
            storm::builder::ExplicitModelBuilder<double> mdpBuilder(generator);
            auto model = mdpBuilder.build();
            return model->template as<storm::models::sparse::Mdp<double>>();
        }

        // Prints the following to `out` on separate lines:
        // - `name`;
        // - the elements of `vec`, space-separated;
        // - "<seconds>.<milliseconds>s"; and
        // - number of nanoseconds.
        // (The latter two according to `stopwatch`.)
        template<typename T>
        void showResult(std::ostream& out, const std::string& name, const std::vector<T>& vec, storm::utility::Stopwatch stopwatch) {
            out << name << '\n';
            if (vec.size() > 0) {
                out << vec.at(0);
                for (int i = 1; i < vec.size(); ++i) {
                    out << ' ' << vec.at(i);
                }
            }
            out << '\n';
            out << stopwatch << '\n';
            out << stopwatch.getTimeInNanoseconds() << '\n';
        }

        void processInput(SymbolicInput &input, storm::cli::ModelProcessingInformation& mpi) {
            auto inputProgramme = getInputProgramme();
            const int capacity = getCapacity(inputProgramme);
            auto cmdp = getInputCmdp(inputProgramme);

            // TODO get filename from commandline arguments.
            std::ofstream outfile("storm-cmdp-output.txt");
            // "S" for "standard", "f" for "file".
            storm::utility::TeeStream sfout(std::cout, outfile);

            storm::utility::Stopwatch minInitConsTimer(true);
            auto minInitConsWrongOrder = computeMinInitCons(cmdp);
            minInitConsTimer.stop();
            auto minInitCons = storm::utility::undoStatePermutation(minInitConsWrongOrder, cmdp);

            storm::utility::Stopwatch safeTimer(true);
            auto safeWrongOrder = computeSafe(cmdp, capacity);
            safeTimer.stop();
            auto safe = storm::utility::undoStatePermutation(safeWrongOrder, cmdp);

            std::vector<ExtInt> safePrWrongOrder;
            storm::cmdp::CounterSelector counterSelector;
            storm::utility::Stopwatch safePrTimer(true);
            std::tie(safePrWrongOrder, counterSelector) = computeSafePr(cmdp, capacity);
            safePrTimer.stop();
            auto safePr = storm::utility::undoStatePermutation(safePrWrongOrder, cmdp);

            std::cout << "capacity = " << capacity << '\n';

            showResult(sfout, "MinInitCons", minInitCons, minInitConsTimer);
            showResult(sfout, "Safe", safe, safeTimer);
            showResult(sfout, "SafePR", safePr, safePrTimer);

            // Probably wrong order:
            std::cout << "counterSelector =\n";
            printCounterSelector(std::cout, counterSelector, cmdp, capacity);

            bool counterSelectorGood = validateCounterSelector(counterSelector, cmdp, safePrWrongOrder, capacity);
            sfout << "Counter selector satisfies requirements:\n"
                << std::boolalpha << counterSelectorGood << '\n';
        }

        void processOptions() {
            // Start by setting some urgent options (log levels, resources, etc.)
            storm::cli::setUrgentOptions();
            
            auto coreSettings = storm::settings::getModule<storm::settings::modules::CoreSettings>();
            
            // Parse and preprocess symbolic input (PRISM, JANI, properties, etc.)
            auto symbolicInput = storm::cli::parseSymbolicInput();
            storm::cli::ModelProcessingInformation mpi;
            std::tie(symbolicInput, mpi) = storm::cli::preprocessSymbolicInput(symbolicInput);

            processInput(symbolicInput, mpi);
        }

    }
}


/*!
 * Main entry point of the executable storm-cmdp.
 */
int main(const int argc, const char** argv) {

    try {
        storm::utility::setUp();
        storm::cli::printHeader("Storm-cmdp", argc, argv);
        storm::settings::initializeCmdpSettings("Storm-cmdp", "storm-cmdp");

        storm::utility::Stopwatch totalTimer(true);
        if (!storm::cli::parseOptions(argc, argv)) {
            return -1;
        }

        storm::cmdp::processOptions();

        totalTimer.stop();
        if (storm::settings::getModule<storm::settings::modules::ResourceSettings>().isPrintTimeAndMemorySet()) {
            storm::cli::printTimeAndMemoryStatistics(totalTimer.getTimeInMilliseconds());
        }

        storm::utility::cleanUp();
        return 0;
    } catch (storm::exceptions::BaseException const& exception) {
        STORM_LOG_ERROR("An exception caused Storm-cmdp to terminate. The message of the exception is: " << exception.what());
        return 1;
    } catch (std::exception const& exception) {
        STORM_LOG_ERROR("An unexpected exception occurred and caused Storm-cmdp to terminate. The message of this exception is: " << exception.what());
        return 2;
    }
}
