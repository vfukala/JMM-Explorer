#include <cstdlib>

#include "jmme-scanner.hpp"
#include "parser.hpp"
#include "snippet.hpp"

namespace JMMExplorer
{

void run()
{
	JMMEScanner scn(&std::cin);
	Snippet snippet;
	JMMEParser prs(scn, snippet);
	prs();
	snippet.print(std::cout);
}

}

int main()
{
	JMMExplorer::run();
	return EXIT_SUCCESS;
}

