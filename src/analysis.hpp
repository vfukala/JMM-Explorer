#ifndef ANALYSIS_HPP
#define ANALYSIS_HPP

#include <cstdint>
#include <functional>
#include <variant>
#include <iostream>

#include "vec.hpp"

namespace JMMExplorer
{

/// A possible result of an execution of the inputted program when no exception occured
typedef vec<vec<int32_t>> RegularExecutionResult;

/// A possible result of an execution of the inputted program that resulted in an (runtime) exception
struct ExceptedExecutionResult
{
	uint32_t ex_thread;
	uint32_t ex_line;

	bool operator==(const ExceptedExecutionResult& other) const;
};

/// A possible result of an execution of the inputted program
struct ExecutionResult
{
	std::variant<RegularExecutionResult, ExceptedExecutionResult> result;

	bool operator==(const ExecutionResult& other) const;

	bool operator!=(const ExecutionResult& other) const;

	/// prints the execution result to the specified output stream in a human-readable format
	/// thread_name_fetcher should return the name of the thread given its (zero-based) index
	void print(std::ostream& os, const std::function<std::string(uint32_t)>& thread_name_fetcher) const;
};

/// Generates all possible execution results (that this program is designed to find) of a program consisting of multiple code snippets
/// Returns true if and only if at least one of the snippets was ill formed (incorrect monitor use)
bool analyze(const vec<std::string>& filenames, const vec<std::istream*>& inputs, vec<ExecutionResult>& results, std::ostream& err_out);


}

#endif // ANALYSIS_HPP
