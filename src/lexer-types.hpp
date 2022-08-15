#ifndef LEXER_TYPES_HPP
#define LEXER_TYPES_HPP

#include <string>

#include "jmme-language.hpp"

namespace JMMExplorer
{

enum class AdditiveOpType
{
	Add, Subtract
};

ArithmeticOpType to_arithmetic_type(AdditiveOpType op_type);

enum class MultiplicativeOpType
{
	Multiply, Divide, Modulo
};

ArithmeticOpType to_arithmetic_type(MultiplicativeOpType op_type);

enum class IncdecOpType
{
	Increment, Decrement
};

ArithmeticOpType to_arithmetic_type(IncdecOpType op_type);

}


#endif // LEXER_TYPES_HPP
