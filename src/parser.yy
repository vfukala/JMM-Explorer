%skeleton "lalr1.cc"
%require "3.2"
%language "c++"
%define api.namespace {JMMExplorer}
%define api.parser.class {JMMEParser}

%define api.value.type variant
%define parse.error detailed

%code requires
{

namespace JMMExplorer
{

class JMMEScanner;

}

#include "lexer-types.hpp"
#include "snippet.hpp"

}

%parse-param { JMMEScanner& scanner }
%parse-param { Snippet& ctx }

%code
{

#include <iostream>
#include <string>
#include <vector>

#include "jmme-scanner.hpp"
#include "snippet.hpp"

#undef yylex
#define yylex scanner.yylex
}

%token SEMIC
%token<Ident> IDENT
%token<uint32_t> INTLIT
%token<AdditiveOpType> ADDITIVE_OP
%token<MultiplicativeOpType> MULTIPLICATIVE_OP
%token<IncdecOpType> INCDEC_OP
%token LPAREN
%token RPAREN
%token DOT
%token ASSIGN
%token<ArithmeticOpType> ASSIGN_OP

%nterm<Snippet::LocalValue> expression unary_expression multiplicative_expression

%nterm statement static_call method_call assignment_statement assignment_op_statement incdec_statement

%nterm program

%start program

%locations

%%

program:
	%empty
	| program statement
;

statement:
	static_call
	| method_call
	| assignment_statement
	| assignment_op_statement
	| incdec_statement
;

static_call:
	IDENT LPAREN expression RPAREN SEMIC { ctx.emit_static_call($1, $3, @1); }
;

method_call:
	IDENT DOT IDENT LPAREN RPAREN SEMIC { ctx.emit_method_call($1, $3, @1); }
;

assignment_statement:
	IDENT ASSIGN expression SEMIC { ctx.emit_write($1, $3, @1); }
;

assignment_op_statement:
	IDENT ASSIGN_OP expression SEMIC { ctx.emit_op_write($1, $3, $2, @1); }
;

incdec_statement:
	IDENT INCDEC_OP SEMIC { ctx.emit_op_write($1, Snippet::LocalValue::from_literal(1), to_arithmetic_type($2), @1); }
;

expression:
	multiplicative_expression
	| multiplicative_expression ADDITIVE_OP expression { $$ = ctx.emit_arithmetic($1, $3, to_arithmetic_type($2), @1); }
;

multiplicative_expression:
	unary_expression
	| multiplicative_expression MULTIPLICATIVE_OP unary_expression { $$ = ctx.emit_arithmetic($1, $3, to_arithmetic_type($2), @1); }
;

unary_expression:
	INTLIT { $$ = Snippet::LocalValue::from_literal($1); }
	| IDENT { $$ = ctx.emit_read($1, @1); }
	| LPAREN expression RPAREN { $$ = $2; }
;
	
	
%%

void JMMExplorer::JMMEParser::error(const location_type& loc, const std::string& msg)
{
	std::cerr << "Parser error: " << msg << " at " << loc << '\n';
}
