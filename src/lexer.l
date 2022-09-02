%{
#include "jmme-scanner.hpp"
#include "lexer-types.hpp"
#undef YY_DECL
#define YY_DECL int JMMExplorer::JMMEScanner::yylex(JMMExplorer::JMMEParser::semantic_type *const lval, JMMExplorer::JMMEParser::location_type *location)

using token = JMMExplorer::JMMEParser::token;
%}

%option debug
%option nodefault
%option yyclass="JMMExplorer::JMMEScanner"
%option noyywrap
%option c++
%option yylineno

%%
%{
				yylval = lval;
				location->begin.line = location->end.line = yylineno;
%}

;				{ return token::SEMIC; }
\+\+			{ yylval->build<IncdecOpType>(IncdecOpType::Increment); return token::INCDEC_OP; }
\-\-			{ yylval->build<IncdecOpType>(IncdecOpType::Decrement); return token::INCDEC_OP; }
\+				{ yylval->build<AdditiveOpType>(AdditiveOpType::Add); return token::ADDITIVE_OP; }
\-				{ yylval->build<AdditiveOpType>(AdditiveOpType::Subtract); return token::ADDITIVE_OP; }
\*				{ yylval->build<MultiplicativeOpType>(MultiplicativeOpType::Multiply); return token::MULTIPLICATIVE_OP; }
\/				{ yylval->build<MultiplicativeOpType>(MultiplicativeOpType::Divide); return token::MULTIPLICATIVE_OP; }
%				{ yylval->build<MultiplicativeOpType>(MultiplicativeOpType::Modulo); return token::MULTIPLICATIVE_OP; }
[a-zA-Z\$]|([a-zA-Z\$_][a-zA-Z\$_0-9]*)		{ yylval->build<Ident>(YYText()); return token::IDENT; }
[0-9]+			{
				const char *const tok = YYText();
				int64_t val;
				if (strlen(tok) > 10 || (val = strtoll(tok, nullptr, 10)) > (1LL << 31) - 1)
					std::cerr << "Integer literal " << tok << " out of range." << std::endl;
				else
					yylval->build<uint32_t>(val); return token::INTLIT;
				}
&				{ return token::AMPERSAND; }
\^				{ return token::CARET; }
\|				{ return token::PIPE; }
\(				{ return token::LPAREN; }
\)				{ return token::RPAREN; }
\.				{ return token::DOT; }
=				{ return token::ASSIGN; }
\+=				{ yylval->build<ArithmeticOpType>(ArithmeticOpType::Add); return token::ASSIGN_OP; }
\-=				{ yylval->build<ArithmeticOpType>(ArithmeticOpType::Subtract); return token::ASSIGN_OP; }
\*=				{ yylval->build<ArithmeticOpType>(ArithmeticOpType::Multiply); return token::ASSIGN_OP; }
\/=				{ yylval->build<ArithmeticOpType>(ArithmeticOpType::Divide); return token::ASSIGN_OP; }
%=				{ yylval->build<ArithmeticOpType>(ArithmeticOpType::Modulo); return token::ASSIGN_OP; }
&=				{ yylval->build<ArithmeticOpType>(ArithmeticOpType::And); return token::ASSIGN_OP; }
\^=				{ yylval->build<ArithmeticOpType>(ArithmeticOpType::Xor); return token::ASSIGN_OP; }
\|=				{ yylval->build<ArithmeticOpType>(ArithmeticOpType::Or); return token::ASSIGN_OP; }
[ \t\v\r\n]		{ }
.				{ std::cerr << "Unrecognized character \'" << YYText() << "\'." << std::endl; }
%%
