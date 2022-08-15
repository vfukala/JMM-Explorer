#ifndef JMME_SCANNER_HPP
#define JMME_SCANNER_HPP

#if ! defined(yyFlexLexerOnce)
#include <FlexLexer.h>
#endif

#include "parser.hpp"

namespace JMMExplorer
{

class JMMEScanner : public yyFlexLexer
{
public:
	JMMEScanner(std::istream *in) : yyFlexLexer(in)
	{
		loc = new JMMExplorer::JMMEParser::location_type();
	}

	using FlexLexer::yylex;

	virtual int yylex(JMMEParser::semantic_type *const lval, JMMEParser::location_type *location);

private:
	JMMEParser::semantic_type *yylval = nullptr;
	JMMEParser::location_type *loc = nullptr;
};

}


#endif // JMME_SCANNER_HPP
