#include "lexer-types.hpp"

#include <cassert>

namespace JMMExplorer
{

ArithmeticOpType to_arithmetic_type(const AdditiveOpType op_type)
{
	return op_type == AdditiveOpType::Add ? ArithmeticOpType::Add : ArithmeticOpType::Subtract;
}

ArithmeticOpType to_arithmetic_type(const MultiplicativeOpType op_type)
{
	switch (op_type)
	{
		case MultiplicativeOpType::Multiply:
			return ArithmeticOpType::Multiply;
		case MultiplicativeOpType::Divide:
			return ArithmeticOpType::Divide;
		case MultiplicativeOpType::Modulo:
			return ArithmeticOpType::Modulo;
	}
	assert(false);
}

ArithmeticOpType to_arithmetic_type(const IncdecOpType op_type)
{
	return op_type == IncdecOpType::Increment ? ArithmeticOpType::Add : ArithmeticOpType::Subtract;
}

}
