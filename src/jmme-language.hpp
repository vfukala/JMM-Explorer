#ifndef JMME_LANGUAGE_HPP
#define JMME_LANGUAGE_HPP

#include <string>

#include "location.hh"
#include "str.hpp"

namespace JMMExplorer
{

typedef location ILoc;
typedef std::string Ident;

enum class ArithmeticOpType
{
	Add, Subtract, Multiply, Divide, Modulo, Or, Xor, And
};

str get_mnemonic(ArithmeticOpType op_type);

}

#endif // JMME_LANGUAGE_HPP
