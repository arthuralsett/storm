#ifndef STORM_STORAGE_EXPRESSIONS_EXPRESSIONMANAGER_H_
#define STORM_STORAGE_EXPRESSIONS_EXPRESSIONMANAGER_H_

#include <cstdint>
#include <iterator>
#include <vector>
#include <unordered_map>
#include <unordered_set>

#include "src/storage/expressions/Variable.h"
#include "src/storage/expressions/Expression.h"
#include "src/utility/OsDetection.h"

namespace storm {
    namespace expressions {
        // Forward-declare manager class for iterator class.
        class ExpressionManager;
        
        class VariableIterator : public std::iterator<std::input_iterator_tag, std::pair<storm::expressions::Variable, storm::expressions::Type> const> {
        public:
            enum class VariableSelection { OnlyRegularVariables, OnlyAuxiliaryVariables, AllVariables };
            
            VariableIterator(ExpressionManager const& manager, std::unordered_map<std::string, uint_fast64_t>::const_iterator nameIndexIterator, std::unordered_map<std::string, uint_fast64_t>::const_iterator nameIndexIteratorEnd, VariableSelection const& selection);
            VariableIterator(VariableIterator const& other) = default;
            VariableIterator& operator=(VariableIterator const& other) = default;
            
            // Define the basic input iterator operations.
            bool operator==(VariableIterator const& other);
            bool operator!=(VariableIterator const& other);
            value_type& operator*();
            VariableIterator& operator++(int);
            VariableIterator& operator++();
            
        private:
            /*!
             * Moves the current element to the next selected element or the end iterator if there is no such element.
             *
             * @param atLeastOneStep A flag indicating whether at least one step should be made.
             */
            void moveUntilNextSelectedElement(bool atLeastOneStep = true);
            
            // The manager responsible for the variable to iterate over.
            ExpressionManager const& manager;
            
            // The underlying iterator that ranges over all names and the corresponding indices.
            std::unordered_map<std::string, uint_fast64_t>::const_iterator nameIndexIterator;
            
            // The iterator indicating the end of the underlying iterator range.
            std::unordered_map<std::string, uint_fast64_t>::const_iterator nameIndexIteratorEnd;
            
            // A field indicating which variables we are supposed to iterate over.
            VariableSelection selection;
            
            // The current element that is shown to the outside upon dereferencing.
            std::pair<storm::expressions::Variable, storm::expressions::Type> currentElement;
        };
        
        /*!
         * This class is responsible for managing a set of typed variables and all expressions using these variables.
         */
        class ExpressionManager {
        public:
            friend class VariableIterator;
            
            typedef VariableIterator const_iterator;
            
            /*!
             * Creates a new manager that is unaware of any variables.
             */
            ExpressionManager();
            
            // Explicitly delete copy construction/assignment, since the manager is supposed to be stored as a pointer
            // of some sort. This is because the expression classes store a reference to the manager and it must
            // therefore be guaranteed that they do not become invalid, because the manager has been copied.
            ExpressionManager(ExpressionManager const& other) = delete;
            ExpressionManager& operator=(ExpressionManager const& other) = delete;
#ifndef WINDOWS
            // Create default instantiations for the move construction/assignment.
            ExpressionManager(ExpressionManager&& other) = default;
            ExpressionManager& operator=(ExpressionManager&& other) = default;
#endif

            /*!
             * Creates an expression that characterizes the given boolean literal.
             *
             * @param value The value of the boolean literal.
             * @return The resulting expression.
             */
            Expression boolean(bool value) const;
            
            /*!
             * Creates an expression that characterizes the given integer literal.
             *
             * @param value The value of the integer literal.
             * @return The resulting expression.
             */
            Expression integer(int_fast64_t value) const;

            /*!
             * Creates an expression that characterizes the given rational literal.
             *
             * @param value The value of the rational literal.
             * @return The resulting expression.
             */
            Expression rational(double value) const;
            
            /*!
             * Compares the two expression managers for equality, which holds iff they are the very same object.
             */
            bool operator==(ExpressionManager const& other) const;
            
            /*!
             * Retrieves the boolean type.
             *
             * @return The boolean type.
             */
            Type getBooleanType() const;
            
            /*!
             * Retrieves the integer type.
             *
             * @return The integer type.
             */
            Type getIntegerType() const;
            
            /*!
             * Retrieves the bounded integer type.
             *
             * @param width The bit width of the bounded type.
             * @return The bounded integer type.
             */
            Type getBoundedIntegerType(std::size_t width) const;
            
            /*!
             * Retrieves the rational type.
             *
             * @return The rational type.
             */
            Type getRationalType() const;
            
            /*!
             * Declares a variable with a name that must not yet exist and its corresponding type. Note that the name
             * must not start with two underscores since these variables are reserved for internal use only.
             *
             * @param name The name of the variable.
             * @param variableType The type of the variable.
             * @return The newly declared variable.
             */
            Variable declareVariable(std::string const& name, storm::expressions::Type const& variableType);
            
            /*!
             * Declares an auxiliary variable with a name that must not yet exist and its corresponding type.
             *
             * @param name The name of the variable.
             * @param variableType The type of the variable.
             * @return The newly declared variable.
             */
            Variable declareAuxiliaryVariable(std::string const& name, storm::expressions::Type const& variableType);
            
            /*!
             * Declares a variable with the given name if it does not yet exist.
             *
             * @param name The name of the variable to declare.
             * @param variableType The type of the variable to declare.
             * @return The variable.
             */
            Variable declareOrGetVariable(std::string const& name, storm::expressions::Type const& variableType);

            /*!
             * Declares a variable with the given name if it does not yet exist.
             *
             * @param name The name of the variable to declare.
             * @param variableType The type of the variable to declare.
             * @return The variable.
             */
            Variable declareOrGetAuxiliaryVariable(std::string const& name, storm::expressions::Type const& variableType);
            
            /*!
             * Retrieves the expression that represents the variable with the given name.
             *
             * @param name The name of the variable to retrieve.
             */
            Variable getVariable(std::string const& name) const;
            
            /*!
             * Retrieves an expression that represents the variable with the given name.
             *
             * @param name The name of the variable
             * @return An expression that represents the variable with the given name.
             */
            Expression getVariableExpression(std::string const& name) const;
            
            /*!
             * Declares a variable with the given type whose name is guaranteed to be unique and not yet in use.
             *
             * @param variableType The type of the variable to declare.
             * @return The variable.
             */
            Variable declareFreshVariable(storm::expressions::Type const& variableType);
            
            /*!
             * Declares an auxiliary variable with the given type whose name is guaranteed to be unique and not yet in use.
             *
             * @param variableType The type of the variable to declare.
             * @return The variable.
             */
            Variable declareFreshAuxiliaryVariable(storm::expressions::Type const& variableType);

            /*!
             * Retrieves the number of variables with the given type.
             *
             * @return The number of variables with the given type.
             */
            uint_fast64_t getNumberOfVariables(storm::expressions::Type const& variableType) const;
            
            /*!
             * Retrieves the number of variables.
             *
             * @return The number of variables.
             */
            uint_fast64_t getNumberOfVariables() const;
            
            /*!
             * Retrieves the number of boolean variables.
             *
             * @return The number of boolean variables.
             */
            uint_fast64_t getNumberOfBooleanVariables() const;

            /*!
             * Retrieves the number of integer variables.
             *
             * @return The number of integer variables.
             */
            uint_fast64_t getNumberOfIntegerVariables() const;
            
            /*!
             * Retrieves the number of rational variables.
             *
             * @return The number of rational variables.
             */
            uint_fast64_t getNumberOfRationalVariables() const;

            /*!
             * Retrieves the number of auxiliary variables with the given type.
             *
             * @return The number of auxiliary variables with the given type.
             */
            uint_fast64_t getNumberOfAuxiliaryVariables(storm::expressions::Type const& variableType) const;
            
            /*!
             * Retrieves the number of auxiliary variables.
             *
             * @return The number of auxiliary variables.
             */
            uint_fast64_t getNumberOfAuxiliaryVariables() const;
            
            /*!
             * Retrieves the number of boolean variables.
             *
             * @return The number of boolean variables.
             */
            uint_fast64_t getNumberOfAuxiliaryBooleanVariables() const;
            
            /*!
             * Retrieves the number of integer variables.
             *
             * @return The number of integer variables.
             */
            uint_fast64_t getNumberOfAuxiliaryIntegerVariables() const;
            
            /*!
             * Retrieves the number of rational variables.
             *
             * @return The number of rational variables.
             */
            uint_fast64_t getNumberOfAuxiliaryRationalVariables() const;
            
            /*!
             * Retrieves the name of the variable with the given index.
             *
             * @param index The index of the variable whose name to retrieve.
             * @return The name of the variable.
             */
            std::string const& getVariableName(uint_fast64_t index) const;
            
            /*!
             * Retrieves the type of the variable with the given index.
             *
             * @param index The index of the variable whose name to retrieve.
             * @return The type of the variable.
             */
            Type const& getVariableType(uint_fast64_t index) const;

            /*!
             * Retrieves the offset of the variable with the given index within the group of equally typed variables.
             *
             * @param index The index of the variable.
             * @return The offset of the variable.
             */
            uint_fast64_t getOffset(uint_fast64_t index) const;
            
            /*!
             * Retrieves an iterator to all variables managed by this manager.
             *
             * @return An iterator to all variables managed by this manager.
             */
            const_iterator begin() const;
            
            /*!
             * Retrieves an iterator that points beyond the last variable managed by this manager.
             *
             * @return An iterator that points beyond the last variable managed by this manager.
             */
            const_iterator end() const;
            
        private:
            /*!
             * Checks whether the given variable name is valid.
             *
             * @param name The name to check.
             * @return True iff the variable name is valid.
             */
            static bool isValidVariableName(std::string const& name);
            
            /*!
             * Retrieves whether a variable with the given name exists.
             *
             * @param name The name of the variable to check for.
             * @return True iff a variable with this name exists.
             */
            bool variableExists(std::string const& name) const;
            
            // A mapping from all variable names (auxiliary + normal) to their indices.
            std::unordered_map<std::string, uint_fast64_t> nameToIndexMapping;
            
            // A mapping from all variable indices to their names.
            std::unordered_map<uint_fast64_t, std::string> indexToNameMapping;

            // A mapping from all variable indices to their types.
            std::unordered_map<uint_fast64_t, Type> indexToTypeMapping;
            
            // Store counts for variables.
            std::unordered_map<Type, uint_fast64_t> variableTypeToCountMapping;

            // The number of declared variables.
            uint_fast64_t numberOfVariables;
            
            // Store counts for auxiliary variables.
            std::unordered_map<Type, uint_fast64_t> auxiliaryVariableTypeToCountMapping;

            // The number of declared auxiliary variables.
            uint_fast64_t numberOfAuxiliaryVariables;
            
            // A counter used to create fresh variables.
            uint_fast64_t freshVariableCounter;
            
            // A mask that can be used to query whether a variable is an auxiliary variable.
            static const uint64_t auxiliaryMask = (1 << 60);
            
            // A mask that can be used to project a variable index to its offset (with the group of equally typed variables).
            static const uint64_t offsetMask = (1 << 60) - 1;
        };
    }
}

#endif /* STORM_STORAGE_EXPRESSIONS_EXPRESSIONMANAGER_H_ */