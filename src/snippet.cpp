#include "snippet.hpp"
#include "jmme-language.hpp"

#include <cassert>
#include <variant>

namespace JMMExplorer
{

Snippet::LocalValue Snippet::LocalValue::from_literal(const uint32_t literal)
{
	return { std::variant<size_t, uint32_t>{ std::in_place_index<1>, literal } };
}

Snippet::LocalValue Snippet::LocalValue::from_local(const size_t local_id)
{
	return { std::variant<size_t, uint32_t>{ std::in_place_index<0>, local_id } };
}

bool Snippet::LocalValue::is_literal() const
{
	return std::holds_alternative<uint32_t>(value);
}

uint32_t Snippet::LocalValue::get_literal() const
{
	return std::get<uint32_t>(value);
}

size_t Snippet::LocalValue::get_local_id() const
{
	return std::get<size_t>(value);
}

void Snippet::emit_method_call(const Ident& object_name, const Ident& method_name, const ILoc loc)
{
	assert(!object_name.empty() && object_name[0] == 'm');
	assert(method_name == "lock" || method_name == "unlock");
	used_monitors.insert(object_name);
	if (method_name == "lock")
		instructions.push_back({ LockInstruction{ object_name }, loc });
	else
		instructions.push_back({ UnlockInstruction{ object_name }, loc });
}

void Snippet::emit_static_call(const Ident& function_name, const LocalValue arg, const ILoc loc)
{
	assert(function_name == "print");
	instructions.push_back({ PrintInstruction{ arg }, loc });
}

size_t Snippet::allocate_temporary()
{
	const Ident temporary = "ct" + std::to_string(next_temporary++);
	const size_t local_id = locals.size();
	local_to_id[temporary] = local_id;
	locals.push_back(temporary);
	return local_id;
}

Snippet::LocalValue Snippet::emit_read(const Ident& var_name, const ILoc loc)
{
	const char ch0 = var_name[0];
	assert(!var_name.empty() && (ch0 == 'l' || ch0 == 's' || ch0 == 'v'));
	if (ch0 == 'l')
	{
		if (local_to_id.find(var_name) == local_to_id.end())
		{
			local_to_id[var_name] = locals.size();
			locals.push_back(var_name);
		}
		return LocalValue::from_local(local_to_id[var_name]);
	}
	else
	{
		(ch0 == 's' ? used_shareds : used_volatiles).insert(var_name);
		const size_t local_id = allocate_temporary();
		instructions.push_back(ch0 == 's' ? Instruction{ SharedReadInstruction{ local_id, var_name }, loc }  : Instruction{ VolatileReadInstruction{ local_id, var_name }, loc });
		return LocalValue::from_local(local_id);
	}
}

void Snippet::emit_write(const Ident& target_name, const LocalValue data, const ILoc loc)
{
	const char ch0 = target_name[0];
	assert(!target_name.empty() && (ch0 == 'l' || ch0 == 's' || ch0 == 'v'));
	if (ch0 == 'l')
	{
		if (local_to_id.find(target_name) == local_to_id.end())
		{
			local_to_id[target_name] = locals.size();
			locals.push_back(target_name);
		}
		instructions.push_back({ MoveInstruction{ local_to_id[target_name], data }, loc });
	}
	else
	{
		(ch0 == 's' ? used_shareds : used_volatiles).insert(target_name);
		instructions.push_back(ch0 == 's' ? Instruction{ SharedWriteInstruction{ target_name, data }, loc } : Instruction{ VolatileWriteInstruction{ target_name, data }, loc });
	}
}

void Snippet::emit_op_write(const Ident& target_name, const LocalValue op, const ArithmeticOpType op_type, const ILoc loc)
{
	const LocalValue original = emit_read(target_name, loc);
	const LocalValue new_value = emit_arithmetic(original, op, op_type, loc);
	emit_write(target_name, new_value, loc);
}

Snippet::LocalValue Snippet::emit_arithmetic(const LocalValue op0, const LocalValue op1, const ArithmeticOpType op_type, const ILoc loc)
{
	const size_t temp = allocate_temporary();
	instructions.push_back({ ArithmeticInstruction{ temp, op0, op_type, op1 }, loc });
	return LocalValue::from_local(temp);
}

void Snippet::print(std::ostream& os) const
{
	const auto val_to_str = [this](const LocalValue val)
	{
		return val.is_literal() ? std::to_string(val.get_literal()) : locals[val.get_local_id()];
	};
	for (const Instruction& instr : instructions)
	{
		const auto& vari = instr.instr;
		if (std::holds_alternative<LockInstruction>(vari))
			os << "lock " << std::get<LockInstruction>(vari).monitor_name << '\n';
		else if (std::holds_alternative<UnlockInstruction>(vari))
			os << "unlock " << std::get<UnlockInstruction>(vari).monitor_name << '\n';
		else if (std::holds_alternative<ArithmeticInstruction>(vari))
		{
			const auto& ari = std::get<ArithmeticInstruction>(vari);
			os << get_mnemonic(ari.op_type) << " " << locals[ari.target] << ", " << val_to_str(ari.op0) << ", " << val_to_str(ari.op1) << '\n';
		}
		else if (std::holds_alternative<SharedReadInstruction>(vari))
		{
			const auto& sre = std::get<SharedReadInstruction>(vari);
			os << "sre " << locals[sre.target] << ", " << sre.shared_source << '\n';
		}
		else if (std::holds_alternative<SharedWriteInstruction>(vari))
		{
			const auto& swr = std::get<SharedWriteInstruction>(vari);
			os << "swr " << val_to_str(swr.data) << ", " << swr.shared_target << '\n';
		}
		else if (std::holds_alternative<VolatileReadInstruction>(vari))
		{
			const auto& vre = std::get<VolatileReadInstruction>(vari);
			os << "vre " << locals[vre.target] << ", " << vre.volatile_source << '\n';
		}
		else if (std::holds_alternative<VolatileWriteInstruction>(vari))
		{
			const auto& vwr = std::get<VolatileWriteInstruction>(vari);
			os << "vwr " << val_to_str(vwr.data) << ", " << vwr.volatile_target << '\n';
		}
		else if (std::holds_alternative<MoveInstruction>(vari))
		{
			const auto& mov = std::get<MoveInstruction>(vari);
			os << "mov " << locals[mov.local_id] << ", " << val_to_str(mov.data) << '\n';
		}
		else if (std::holds_alternative<PrintInstruction>(vari))
		{
			os << "print " << val_to_str(std::get<PrintInstruction>(vari).arg) << '\n';
		}
		else
			assert(false);
	}
}

}
