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

#include <tuple>
#include <vector>
#include "storm-cmdp/algorithms/algorithms.h"
#include "storm-cmdp/state-permutation/state-permutation.h"

using ExtInt = storm::utility::ExtendedInteger;

namespace storm {
    namespace cmdp {
        typedef storm::cli::SymbolicInput SymbolicInput;

        // Returns the capacity of the input CMDP.
        int getCapacity(const storm::prism::Program& inputProgramme) {
            if (!inputProgramme.hasConstant("capacity")) {
                // TODO improve error handling.
                std::exit(EXIT_FAILURE);
            }
            auto constantCap = inputProgramme.getConstant("capacity");
            if (!constantCap.isDefined()) {
                // TODO improve error handling.
                std::exit(EXIT_FAILURE);
            }
            auto expr = constantCap.getExpression();
            if (!expr.hasIntegerType()) {
                // TODO improve error handling.
                std::exit(EXIT_FAILURE);
            }
            return expr.evaluateAsInt();
        }

        // Returns object representing the input (referred to as a "programme").
        storm::prism::Program getInputProgramme() {
            auto ioSettings = storm::settings::getModule<storm::settings::modules::IOSettings>();
            if (!ioSettings.isPrismInputSet()) {
                // TODO improve error handling.
                std::exit(EXIT_FAILURE);
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

        void processInput(SymbolicInput &input, storm::cli::ModelProcessingInformation& mpi) {
            auto inputProgramme = getInputProgramme();
            const int capacity = getCapacity(inputProgramme);
            auto cmdp = getInputCmdp(inputProgramme);

            auto minInitConsWrongOrder = computeMinInitCons(cmdp);
            auto minInitCons = storm::utility::undoStatePermutation(minInitConsWrongOrder, cmdp);

            auto safeWrongOrder = computeSafe(cmdp, capacity);
            auto safe = storm::utility::undoStatePermutation(safeWrongOrder, cmdp);

            std::vector<ExtInt> safePrWrongOrder;
            storm::cmdp::CounterSelector counterSelector;
            std::tie(safePrWrongOrder, counterSelector) = computeSafePr(cmdp, capacity);
            auto safePr = storm::utility::undoStatePermutation(safePrWrongOrder, cmdp);

            auto print_vec = [](const std::vector<ExtInt>& v) {
                if (v.size() == 0) {
                    std::cout << "{}\n";
                    return;
                }
                std::cout << '{' << v.at(0);
                for (int i = 1; i < v.size(); ++i) {
                    std::cout << ' ' << v.at(i);
                }
                std::cout << "}\n";
            };

            std::cout << "capacity = " << capacity << '\n';
            std::cout << "minInitCons = ";
            print_vec(minInitCons);
            std::cout << "safe = ";
            print_vec(safe);
            std::cout << "safePr = ";
            print_vec(safePr);

            // Probably wrong order:
            std::cout << "counterSelector =\n";
            printCounterSelector(std::cout, counterSelector, cmdp, capacity);
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
