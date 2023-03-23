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

namespace storm {
    namespace cmdp {
        typedef storm::cli::SymbolicInput SymbolicInput;

        void processInput(SymbolicInput &input, storm::cli::ModelProcessingInformation& mpi) {
            std::cout << "Hello, world.\n";
            auto model = storm::cli::buildPreprocessExportModelWithValueTypeAndDdlib<storm::dd::DdType::CUDD, double>(input, mpi);
            auto cmdp = model->template as<storm::models::sparse::Mdp<double>>();
            std::cout << "cmdp->getNumberOfStates() = " << cmdp->getNumberOfStates() << '\n';
            std::cout << "cmdp->getNumberOfTransitions() = " << cmdp->getNumberOfTransitions() << '\n';
            std::cout << "cmdp->getNumberOfChoices() = " << cmdp->getNumberOfChoices() << '\n';
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
