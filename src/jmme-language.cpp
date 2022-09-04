#include "jmme-language.hpp"

#include <cassert>

namespace JMMExplorer
{

str get_mnemonic(ArithmeticOpType op_type)
{
	switch (op_type)
	{
		case ArithmeticOpType::Add:
			return "add";
		case ArithmeticOpType::Subtract:
			return "sub";
		case ArithmeticOpType::Multiply:
			return "mul";
		case ArithmeticOpType::Divide:
			return "div";
		case ArithmeticOpType::Remainder:
			return "rem";
		case ArithmeticOpType::Or:
			return "or";
		case ArithmeticOpType::Xor:
			return "xor";
		case ArithmeticOpType::And:
			return "and";
		default:
			assert(false);
	}
}

}
