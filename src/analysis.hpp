#ifndef ANALYSIS_HPP
#define ANALYSIS_HPP

#include <cstdint>
#include <functional>
#include <variant>
#include <iostream>

#include "vec.hpp"

namespace JMMExplorer
{

typedef vec<vec<int32_t>> RegularExecutionResult;

struct ExceptedExecutionResult
{
	uint32_t ex_thread;
	uint32_t ex_line;

	bool operator==(const ExceptedExecutionResult& other) const;
};

struct ExecutionResult
{
	std::variant<RegularExecutionResult, ExceptedExecutionResult> result;

	bool operator==(const ExecutionResult& other) const;

	bool operator!=(const ExecutionResult& other) const;

	void print(std::ostream& os, const std::function<std::string(uint32_t)>& thread_name_fetcher) const;
};

bool analyze(const vec<std::string>& filenames, const vec<std::istream*>& inputs, vec<ExecutionResult>& results, std::ostream& err_out);


}

#endif // ANALYSIS_HPP
