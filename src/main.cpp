#include <cstdlib>
#include <fstream>
#include <memory>
#include <variant>

#include "analysis.hpp"
#include "testing.hpp"

namespace JMMExplorer
{

/// Runs the primary application with the given command-line arguments
static void run(const int argc, const char *const *const argv)
{
	bool nonexisting_file = false;
	vec<std::string> filenames;
	vec<std::unique_ptr<std::ifstream>> uq_inputs;
	vec<std::istream*> inputs;
	for (int i = 1; i < argc; i++)
	{
		filenames.push_back(argv[i]);
		uq_inputs.push_back(std::make_unique<std::ifstream>());
		inputs.push_back(uq_inputs.back().get());
		uq_inputs.back()->open(argv[i]);
		if (!*uq_inputs.back())
		{
			std::cerr << "Error: Source file " << argv[i] << " doesn't exist." << std::endl;
			nonexisting_file = true;
			continue;
		}
	}
	if (nonexisting_file)
	{
		std::cout << "Terminating due to a non-existing source file." << std::endl;
		return;
	}
	vec<ExecutionResult> results;
	if (analyze(filenames, inputs, results, std::cerr))
		return;
	for (const ExecutionResult& res : results)
	{
		res.print(std::cout, [&filenames](const uint32_t threadi){ return filenames[threadi]; });
		std::cout << '\n';
	}
}

}

int main(const int argc, const char *const *const argv)
{
#ifndef TESTING
	JMMExplorer::run(argc, argv);
#else
	JMMExplorer::run_all_tests();
#endif
	return EXIT_SUCCESS;
}

