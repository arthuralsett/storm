#include "storm/storage/memorystructure/SparseModelMemoryProduct.h"

#include <boost/optional.hpp>

#include "storm/models/sparse/Dtmc.h"
#include "storm/models/sparse/Mdp.h"
#include "storm/models/sparse/Ctmc.h"
#include "storm/models/sparse/MarkovAutomaton.h"
#include "storm/modelchecker/propositional/SparsePropositionalModelChecker.h"
#include "storm/modelchecker/results/ExplicitQualitativeCheckResult.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"
#include "storm/utility/builder.h"

#include "storm/exceptions/InvalidOperationException.h"

namespace storm {
    namespace storage {

        template <typename ValueType>
        SparseModelMemoryProduct<ValueType>::SparseModelMemoryProduct(storm::models::sparse::Model<ValueType> const& sparseModel, storm::storage::MemoryStructure const& memoryStructure) : model(sparseModel), memory(memoryStructure) {
            // intentionally left empty
        }
            
        template <typename ValueType>
        std::shared_ptr<storm::models::sparse::Model<ValueType>> SparseModelMemoryProduct<ValueType>::build() {
            uint64_t modelStateCount = model.getNumberOfStates();
            uint64_t memoryStateCount = memory.getNumberOfStates();
            
            std::vector<uint64_t> memorySuccessors = computeMemorySuccessors();
            
            // Get the initial states and reachable states. A stateIndex s corresponds to the model state (s / memoryStateCount) and memory state (s % memoryStateCount)
            storm::storage::BitVector initialStates(modelStateCount * memoryStateCount, false);
            auto memoryInitIt = memory.getInitialMemoryStates().begin();
            for (auto const& modelInit : model.getInitialStates()) {
                initialStates.set(modelInit * memoryStateCount + *memoryInitIt, true);
                ++memoryInitIt;
            }
            STORM_LOG_ASSERT(memoryInitIt == memory.getInitialMemoryStates().end(), "Unexpected number of initial states.");
            
            storm::storage::BitVector reachableStates = computeReachableStates(memorySuccessors, initialStates);
            
            // Compute the mapping to the states of the result
            uint64_t reachableStateCount = 0;
            toResultStateMapping = std::vector<uint64_t> (model.getNumberOfStates() * memory.getNumberOfStates(), std::numeric_limits<uint64_t>::max());
            for (auto const& reachableState : reachableStates) {
                toResultStateMapping[reachableState] = reachableStateCount;
                ++reachableStateCount;
            }
                
            // Build the model components
            storm::storage::SparseMatrix<ValueType> transitionMatrix = model.getTransitionMatrix().hasTrivialRowGrouping() ?
                                                                       buildDeterministicTransitionMatrix(reachableStates, memorySuccessors) :
                                                                       buildNondeterministicTransitionMatrix(reachableStates, memorySuccessors);
            storm::models::sparse::StateLabeling labeling = buildStateLabeling(transitionMatrix);
            std::unordered_map<std::string, storm::models::sparse::StandardRewardModel<ValueType>> rewardModels = buildRewardModels(transitionMatrix, memorySuccessors);

            // Add the label for the initial states. We need to translate the state indices w.r.t. the set of reachable states.
            labeling.addLabel("init", initialStates % reachableStates);
            
            
            return buildResult(std::move(transitionMatrix), std::move(labeling), std::move(rewardModels));

        }
            
        template <typename ValueType>
        uint64_t const& SparseModelMemoryProduct<ValueType>::getResultState(uint64_t const& modelState, uint64_t const& memoryState) const {
                return toResultStateMapping[modelState * memory.getNumberOfStates() + memoryState];
        }
            
        template <typename ValueType>
        std::vector<uint64_t> SparseModelMemoryProduct<ValueType>::computeMemorySuccessors() const {
            uint64_t modelTransitionCount = model.getTransitionMatrix().getEntryCount();
            uint64_t memoryStateCount = memory.getNumberOfStates();
            std::vector<uint64_t> result(modelTransitionCount * memoryStateCount, std::numeric_limits<uint64_t>::max());
            
            for (uint64_t memoryState = 0; memoryState < memoryStateCount; ++memoryState) {
                for (uint64_t transitionGoal = 0; transitionGoal < memoryStateCount; ++transitionGoal) {
                    auto const& memoryTransition = memory.getTransitionMatrix()[memoryState][transitionGoal];
                    if (memoryTransition) {
                        for (auto const& modelTransitionIndex : memoryTransition.get()) {
                            result[modelTransitionIndex * memoryStateCount + memoryState] = transitionGoal;
                        }
                    }
                }
            }
            return result;
        }
            
        template <typename ValueType>
        storm::storage::BitVector SparseModelMemoryProduct<ValueType>::computeReachableStates(std::vector<uint64_t> const& memorySuccessors, storm::storage::BitVector const& initialStates) const {
            uint64_t memoryStateCount = memory.getNumberOfStates();
            // Explore the reachable states via DFS.
            // A state s on the stack corresponds to the model state (s / memoryStateCount) and memory state (s % memoryStateCount)
            storm::storage::BitVector reachableStates = initialStates;
            std::vector<uint64_t> stack(reachableStates.begin(), reachableStates.end());
            while (!stack.empty()) {
                uint64_t stateIndex = stack.back();
                stack.pop_back();
                uint64_t modelState = stateIndex / memoryStateCount;
                uint64_t memoryState = stateIndex % memoryStateCount;
                
                auto const& rowGroup = model.getTransitionMatrix().getRowGroup(modelState);
                for (auto modelTransitionIt = rowGroup.begin(); modelTransitionIt != rowGroup.end(); ++modelTransitionIt) {
                    if (!storm::utility::isZero(modelTransitionIt->getValue())) {
                        uint64_t successorModelState = modelTransitionIt->getColumn();
                        uint64_t modelTransitionId = modelTransitionIt - model.getTransitionMatrix().begin();
                        uint64_t successorMemoryState = memorySuccessors[modelTransitionId * memoryStateCount + memoryState];
                        uint64_t successorStateIndex = successorModelState * memoryStateCount + successorMemoryState;
                        if (!reachableStates.get(successorStateIndex)) {
                            reachableStates.set(successorStateIndex, true);
                            stack.push_back(successorStateIndex);
                        }
                    }
                }
            }
            return reachableStates;
        }
            
        template <typename ValueType>
        storm::storage::SparseMatrix<ValueType> SparseModelMemoryProduct<ValueType>::buildDeterministicTransitionMatrix(storm::storage::BitVector const& reachableStates, std::vector<uint64_t> const& memorySuccessors) const {
            uint64_t memoryStateCount = memory.getNumberOfStates();
            uint64_t numResStates = reachableStates.getNumberOfSetBits();
            uint64_t numResTransitions = 0;
            for (auto const& stateIndex : reachableStates) {
                numResTransitions += model.getTransitionMatrix().getRow(stateIndex / memoryStateCount).getNumberOfEntries();
            }
            
            storm::storage::SparseMatrixBuilder<ValueType> builder(numResStates, numResStates, numResTransitions, true);
            uint64_t currentRow = 0;
            for (auto const& stateIndex : reachableStates) {
                uint64_t modelState = stateIndex / memoryStateCount;
                uint64_t memoryState = stateIndex % memoryStateCount;
                auto const& modelRow = model.getTransitionMatrix().getRow(modelState);
                for (auto entryIt = modelRow.begin(); entryIt != modelRow.end(); ++entryIt) {
                    uint64_t transitionId = entryIt - model.getTransitionMatrix().begin();
                    uint64_t const& successorMemoryState = memorySuccessors[transitionId * memoryStateCount + memoryState];
                    builder.addNextValue(currentRow, getResultState(entryIt->getColumn(), successorMemoryState), entryIt->getValue());
                }
                ++currentRow;
            }
            
            return builder.build();
        }
        
        template <typename ValueType>
        storm::storage::SparseMatrix<ValueType> SparseModelMemoryProduct<ValueType>::buildNondeterministicTransitionMatrix(storm::storage::BitVector const& reachableStates, std::vector<uint64_t> const& memorySuccessors) const {
            uint64_t memoryStateCount = memory.getNumberOfStates();
            uint64_t numResStates = reachableStates.getNumberOfSetBits();
            uint64_t numResChoices = 0;
            uint64_t numResTransitions = 0;
            for (auto const& stateIndex : reachableStates) {
                uint64_t modelState = stateIndex / memoryStateCount;
                for (uint64_t modelRow = model.getTransitionMatrix().getRowGroupIndices()[modelState]; modelRow < model.getTransitionMatrix().getRowGroupIndices()[modelState + 1]; ++modelRow) {
                    ++numResChoices;
                    numResTransitions += model.getTransitionMatrix().getRow(modelRow).getNumberOfEntries();
                }
            }
            
            storm::storage::SparseMatrixBuilder<ValueType> builder(numResChoices, numResStates, numResTransitions, true, true, numResStates);
            uint64_t currentRow = 0;
            for (auto const& stateIndex : reachableStates) {
                uint64_t modelState = stateIndex / memoryStateCount;
                uint64_t memoryState = stateIndex % memoryStateCount;
                builder.newRowGroup(currentRow);
                for (uint64_t modelRowIndex = model.getTransitionMatrix().getRowGroupIndices()[modelState]; modelRowIndex < model.getTransitionMatrix().getRowGroupIndices()[modelState + 1]; ++modelRowIndex) {
                    auto const& modelRow = model.getTransitionMatrix().getRow(modelRowIndex);
                    for (auto entryIt = modelRow.begin(); entryIt != modelRow.end(); ++entryIt) {
                        uint64_t transitionId = entryIt - model.getTransitionMatrix().begin();
                        uint64_t const& successorMemoryState = memorySuccessors[transitionId * memoryStateCount + memoryState];
                        builder.addNextValue(currentRow, getResultState(entryIt->getColumn(), successorMemoryState), entryIt->getValue());
                    }
                    ++currentRow;
                }
            }
            
            return builder.build();
        }
        
        template <typename ValueType>
        storm::models::sparse::StateLabeling SparseModelMemoryProduct<ValueType>::buildStateLabeling(storm::storage::SparseMatrix<ValueType> const& resultTransitionMatrix) const {
            uint64_t modelStateCount = model.getNumberOfStates();
            uint64_t memoryStateCount = memory.getNumberOfStates();
            
            uint64_t numResStates = resultTransitionMatrix.getRowGroupCount();
            storm::models::sparse::StateLabeling resultLabeling(numResStates);
            
            for (std::string modelLabel : model.getStateLabeling().getLabels()) {
                if (modelLabel != "init") {
                    storm::storage::BitVector resLabeledStates(numResStates, false);
                    for (auto const& modelState : model.getStateLabeling().getStates(modelLabel)) {
                        for (uint64_t memoryState = 0; memoryState < memoryStateCount; ++memoryState) {
                            uint64_t const& resState = getResultState(modelState, memoryState);
                            // Check if the state exists in the result (i.e. if it is reachable)
                            if (resState < numResStates) {
                                resLabeledStates.set(resState, true);
                            }
                        }
                    }
                    resultLabeling.addLabel(modelLabel, std::move(resLabeledStates));
                }
            }
            for (std::string memoryLabel : memory.getStateLabeling().getLabels()) {
                STORM_LOG_THROW(!resultLabeling.containsLabel(memoryLabel), storm::exceptions::InvalidOperationException, "Failed to build the product of model and memory structure: State labelings are not disjoint as both structures contain the label " << memoryLabel << ".");
                storm::storage::BitVector resLabeledStates(numResStates, false);
                for (auto const& memoryState : memory.getStateLabeling().getStates(memoryLabel)) {
                    for (uint64_t modelState = 0; modelState < modelStateCount; ++modelState) {
                        uint64_t const& resState = getResultState(modelState, memoryState);
                        // Check if the state exists in the result (i.e. if it is reachable)
                        if (resState < numResStates) {
                            resLabeledStates.set(resState, true);
                        }
                    }
                }
                resultLabeling.addLabel(memoryLabel, std::move(resLabeledStates));
            }
            return resultLabeling;
        }
        
        template <typename ValueType>
        std::unordered_map<std::string, storm::models::sparse::StandardRewardModel<ValueType>> SparseModelMemoryProduct<ValueType>::buildRewardModels(storm::storage::SparseMatrix<ValueType> const& resultTransitionMatrix, std::vector<uint64_t> const& memorySuccessors) const {
            std::unordered_map<std::string, storm::models::sparse::StandardRewardModel<ValueType>> result;
            uint64_t memoryStateCount = memory.getNumberOfStates();
            uint64_t numResStates = resultTransitionMatrix.getRowGroupCount();

            for (auto const& rewardModel : model.getRewardModels()) {
                boost::optional<std::vector<ValueType>> stateRewards;
                if (rewardModel.second.hasStateRewards()) {
                    stateRewards = std::vector<ValueType>(numResStates, storm::utility::zero<ValueType>());
                    uint64_t modelState = 0;
                    for (auto const& modelStateReward : rewardModel.second.getStateRewardVector()) {
                        if (!storm::utility::isZero(modelStateReward)) {
                            for (uint64_t memoryState = 0; memoryState < memoryStateCount; ++memoryState) {
                                uint64_t const& resState = getResultState(modelState, memoryState);
                                // Check if the state exists in the result (i.e. if it is reachable)
                                if (resState < numResStates) {
                                    stateRewards.get()[resState] = modelStateReward;
                                }
                            }
                        }
                        ++modelState;
                    }
                }
                boost::optional<std::vector<ValueType>> stateActionRewards;
                if (rewardModel.second.hasStateActionRewards()) {
                    stateActionRewards = std::vector<ValueType>(resultTransitionMatrix.getRowCount(), storm::utility::zero<ValueType>());
                    uint64_t modelState = 0;
                    uint64_t modelRow = 0;
                    for (auto const& modelStateActionReward : rewardModel.second.getStateActionRewardVector()) {
                        if (!storm::utility::isZero(modelStateActionReward)) {
                            while (modelRow >= model.getTransitionMatrix().getRowGroupIndices()[modelState + 1]) {
                                ++modelState;
                            }
                            uint64_t rowOffset = modelRow - model.getTransitionMatrix().getRowGroupIndices()[modelState];
                            for (uint64_t memoryState = 0; memoryState < memoryStateCount; ++memoryState) {
                                uint64_t const& resState = getResultState(modelState, memoryState);
                                // Check if the state exists in the result (i.e. if it is reachable)
                                if (resState < numResStates) {
                                    stateActionRewards.get()[resultTransitionMatrix.getRowGroupIndices()[resState] + rowOffset] = modelStateActionReward;
                                }
                            }
                        }
                        ++modelRow;
                    }
                }
                boost::optional<storm::storage::SparseMatrix<ValueType>> transitionRewards;
                if (rewardModel.second.hasTransitionRewards()) {
                    storm::storage::SparseMatrixBuilder<ValueType> builder(resultTransitionMatrix.getRowCount(), resultTransitionMatrix.getColumnCount());
                    uint64_t stateIndex = 0;
                    for (auto const& resState : toResultStateMapping) {
                        if (resState < numResStates) {
                            uint64_t modelState = stateIndex / memoryStateCount;
                            uint64_t memoryState = stateIndex % memoryStateCount;
                            uint64_t rowGroupSize = resultTransitionMatrix.getRowGroupSize(resState);
                            for (uint64_t rowOffset = 0; rowOffset < rowGroupSize; ++rowOffset) {
                                uint64_t resRowIndex = resultTransitionMatrix.getRowGroupIndices()[resState] + rowOffset;
                                uint64_t modelRowIndex = model.getTransitionMatrix().getRowGroupIndices()[modelState] + rowOffset;
                                auto const& rewardMatrixRow = rewardModel.second.getTransitionRewardMatrix().getRow(modelRowIndex);
                                for (auto entryIt = rewardMatrixRow.begin(); entryIt != rewardMatrixRow.end(); ++entryIt) {
                                    uint64_t transitionId = entryIt - rewardModel.second.getTransitionRewardMatrix().begin();
                                    uint64_t const& successorMemoryState = memorySuccessors[transitionId * memoryStateCount + memoryState];
                                    builder.addNextValue(resRowIndex, getResultState(entryIt->getColumn(), successorMemoryState), entryIt->getValue());
                                }
                            }
                        }
                        ++stateIndex;
                    }
                    transitionRewards = builder.build();
                }
                result.insert(std::make_pair(rewardModel.first, storm::models::sparse::StandardRewardModel<ValueType>(std::move(stateRewards), std::move(stateActionRewards), std::move(transitionRewards))));
            }
            return result;
        }
            
        template <typename ValueType>
        std::shared_ptr<storm::models::sparse::Model<ValueType>> SparseModelMemoryProduct<ValueType>::buildResult(storm::storage::SparseMatrix<ValueType>&& matrix, storm::models::sparse::StateLabeling&& labeling, std::unordered_map<std::string, storm::models::sparse::StandardRewardModel<ValueType>>&& rewardModels) const {
            storm::storage::sparse::ModelComponents<ValueType, storm::models::sparse::StandardRewardModel<ValueType>> components (std::move(matrix), std::move(labeling), std::move(rewardModels));
            
            if (model.isOfType(storm::models::ModelType::Ctmc)) {
                components.rateTransitions = true;
            } else if (model.isOfType(storm::models::ModelType::MarkovAutomaton)) {
                // We also need to translate the exit rates and the Markovian states
                uint64_t numResStates = components.transitionMatrix.getRowGroupCount();
                uint64_t memoryStateCount = memory.getNumberOfStates();
                std::vector<ValueType> resultExitRates;
                resultExitRates.reserve(components.transitionMatrix.getRowGroupCount());
                storm::storage::BitVector resultMarkovianStates(numResStates, false);
                auto const& modelExitRates = dynamic_cast<storm::models::sparse::MarkovAutomaton<ValueType> const&>(model).getExitRates();
                auto const& modelMarkovianStates = dynamic_cast<storm::models::sparse::MarkovAutomaton<ValueType> const&>(model).getMarkovianStates();
                    
                uint64_t stateIndex = 0;
                for (auto const& resState : toResultStateMapping) {
                    if (resState < numResStates) {
                        assert(resState == resultExitRates.size());
                        uint64_t modelState = stateIndex / memoryStateCount;
                        resultExitRates.push_back(modelExitRates[modelState]);
                        if (modelMarkovianStates.get(modelState)) {
                            resultMarkovianStates.set(resState, true);
                        }
                    }
                    ++stateIndex;
                }
                components.markovianStates = std::move(resultMarkovianStates);
                components.exitRates = std::move(resultExitRates);
            }
            
            return storm::utility::builder::buildModelFromComponents(model.getType(), std::move(components));
        }
        
        template  class SparseModelMemoryProduct<double>;
        template  class SparseModelMemoryProduct<storm::RationalNumber>;
        template  class SparseModelMemoryProduct<storm::RationalFunction>;
            
    }
}


