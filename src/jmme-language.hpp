#ifndef JMME_LANGUAGE_HPP
#define JMME_LANGUAGE_HPP

#include <string>

#include "location.hh"
#include "str.hpp"

namespace JMMExplorer
{

typedef location ILoc;
typedef std::string Ident;

/// Represents the type of a binary operation that can be performed by the arithmetic instruction
enum class ArithmeticOpType
{
	Add, Subtract, Multiply, Divide, Remainder, Or, Xor, And
};

/// Returns the short mnemonic of the given arithmetic instruction used when the generated instructions need to be printed out
str get_mnemonic(ArithmeticOpType op_type);

}

#endif // JMME_LANGUAGE_HPP
