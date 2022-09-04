#ifndef LEXER_TYPES_HPP
#define LEXER_TYPES_HPP

#include <string>

#include "jmme-language.hpp"

namespace JMMExplorer
{

/// The specific type (addition or subtraction) of an additive-type operation
enum class AdditiveOpType
{
	Add, Subtract
};

/// Converts the additive operation type to the general arithmetic operation type
ArithmeticOpType to_arithmetic_type(AdditiveOpType op_type);

/// The specific type (multiplication, division or remainder) of a multiplicative-type operation
enum class MultiplicativeOpType
{
	Multiply, Divide, Remainder
};

/// Converts the multiplicative operation type to the general arithmetic operation type
ArithmeticOpType to_arithmetic_type(MultiplicativeOpType op_type);

/// The specific type (increment or decrement) of an increment/decrement operation
enum class IncdecOpType
{
	Increment, Decrement
};

/// Converts the increment/decrement operation type to the corresponding arithmetic operation (addition or subtraction)
ArithmeticOpType to_arithmetic_type(IncdecOpType op_type);

}


#endif // LEXER_TYPES_HPP
