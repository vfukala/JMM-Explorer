#include "snippet.hpp"
#include "jmme-language.hpp"

#include <cassert>
#include <functional>
#include <stack>
#include <variant>

namespace JMMExplorer
{

LocalValue LocalValue::from_literal(const uint32_t literal)
{
	return { std::variant<size_t, uint32_t>{ std::in_place_index<1>, literal } };
}

LocalValue LocalValue::from_local(const size_t local_id)
{
	return { std::variant<size_t, uint32_t>{ std::in_place_index<0>, local_id } };
}

bool LocalValue::is_literal() const
{
	return std::holds_alternative<uint32_t>(value);
}

uint32_t LocalValue::get_literal() const
{
	return std::get<uint32_t>(value);
}

size_t LocalValue::get_local_id() const
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
	pushed_action();
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

LocalValue Snippet::emit_read(const Ident& var_name, const ILoc loc)
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
		pushed_action();
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
		pushed_action();
	}
}

void Snippet::emit_op_write(const Ident& target_name, const LocalValue op, const ArithmeticOpType op_type, const ILoc loc)
{
	const LocalValue original = emit_read(target_name, loc);
	const LocalValue new_value = emit_arithmetic(original, op, op_type, loc);
	emit_write(target_name, new_value, loc);
}

LocalValue Snippet::emit_arithmetic(const LocalValue op0, const LocalValue op1, const ArithmeticOpType op_type, const ILoc loc)
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

bool Instruction::is_synchronization() const
{
	return std::holds_alternative<LockInstruction>(instr)
		|| std::holds_alternative<UnlockInstruction>(instr)
		|| std::holds_alternative<VolatileReadInstruction>(instr)
		|| std::holds_alternative<VolatileWriteInstruction>(instr);
}

bool Instruction::is_action() const
{
	return is_synchronization() || is_shared_read() || is_shared_write();
}

bool Instruction::is_lock() const
{
	return std::holds_alternative<LockInstruction>(instr);
}

bool Instruction::is_unlock() const
{
	return std::holds_alternative<UnlockInstruction>(instr);
}

Ident Instruction::get_monitor_name() const
{
	return is_lock() ? std::get<LockInstruction>(instr).monitor_name : std::get<UnlockInstruction>(instr).monitor_name;
}

bool Instruction::is_volatile_read() const
{
	return std::holds_alternative<VolatileReadInstruction>(instr);
}

bool Instruction::is_volatile_write() const
{
	return std::holds_alternative<VolatileWriteInstruction>(instr);
}

Ident Instruction::get_volatile_name() const
{
	return is_volatile_read() ? std::get<VolatileReadInstruction>(instr).volatile_source : std::get<VolatileWriteInstruction>(instr).volatile_target;
}

bool Instruction::is_shared_read() const
{
	return std::holds_alternative<SharedReadInstruction>(instr);
}

bool Instruction::is_shared_write() const
{
	return std::holds_alternative<SharedWriteInstruction>(instr);
}

Ident Instruction::get_shared_name() const
{
	return is_shared_read() ? std::get<SharedReadInstruction>(instr).shared_source : std::get<SharedWriteInstruction>(instr).shared_target;
}

bool Instruction::is_arithmetic() const
{
	return std::holds_alternative<ArithmeticInstruction>(instr);
}

bool Instruction::is_move() const
{
	return std::holds_alternative<MoveInstruction>(instr);
}

bool Instruction::is_print() const
{
	return std::holds_alternative<PrintInstruction>(instr);
}

bool Instruction::is_read() const
{
	return std::holds_alternative<SharedReadInstruction>(instr) || std::holds_alternative<VolatileReadInstruction>(instr);
}

bool Instruction::is_write() const
{
	return std::holds_alternative<SharedWriteInstruction>(instr) || std::holds_alternative<VolatileWriteInstruction>(instr);
}

const LocalValue& Instruction::get_write_data() const
{
	return is_volatile_write() ? std::get<VolatileWriteInstruction>(instr).data : std::get<SharedWriteInstruction>(instr).data;
}

size_t Instruction::get_read_target() const
{
	return is_volatile_read() ? std::get<VolatileReadInstruction>(instr).target : std::get<SharedReadInstruction>(instr).target;
}

const ArithmeticInstruction& Instruction::as_arithmetic() const
{
	return std::get<ArithmeticInstruction>(instr);
}

const MoveInstruction& Instruction::as_move() const
{
	return std::get<MoveInstruction>(instr);
}

const LocalValue& Instruction::get_print_arg() const
{
	return std::get<PrintInstruction>(instr).arg;
}

void Snippet::pushed_action()
{
	actions.push_back(instructions.size() - 1);
}

size_t Snippet::action_count() const
{
	return actions.size();
}

const Instruction& Snippet::get_action(const uint32_t index) const
{
	return instructions[actions[index]];
}

vec<uint32_t> Snippet::get_synchronization_actions() const
{
	vec<uint32_t> res;
	for (size_t i = 0; i < actions.size(); i++)
		if (instructions[actions[i]].is_synchronization())
			res.push_back(i);
	return res;
}

Snippet::Snippet(const str& name)
	: name(name)
{
}

const str& Snippet::get_name() const
{
	return name;
}

/*
const vec<int32_t>& Snippet::get_execution_results() const
{
	return output;
}

void Snippet::start_execution()
{
	local_state = vec<int64_t>(locals.size(), 0);
	output.clear();
	visibly_written.clear();
	ipointer = 0;
	actions_executed = 0;

	keep_executing();
}

void Snippet::keep_executing()
{

	while (ipointer < instructions.size() && !instructions[ipointer].is_shared_read() && !instructions[ipointer].is_volatile_read())
	{
		const Instruction& instr = instructions[ipointer];
		if (instr.is_arithmetic())
	}
}
*/

void Snippet::run_preexecution_analysis()
{
	// initialize argument_deps and trans_read_deps
	argument_deps = vec<vec<int32_t>>(instructions.size());
	trans_read_deps = vec<vec<uint32_t>>(instructions.size());
	uint32_t nact = 0;
	for (uint32_t i = 0; i < instructions.size(); i++)
	{
		if (nact < actions.size() && actions[nact] < i)
			nact++;
		if (instructions[i].is_read())
			trans_read_deps[i].push_back(nact);
	}

	vec<int32_t> local_written_at(locals.size(), -1);
	for (uint32_t i = 0; i < instructions.size(); i++)
	{
		const Instruction& instr = instructions[i];
		if (instr.is_arithmetic())
		{
			const ArithmeticInstruction& ari = instr.as_arithmetic();
			if (!ari.op0.is_literal())
			{
				argument_deps[i].push_back(local_written_at[ari.op0.get_local_id()]);
				if (argument_deps[i].back() != -1)
					trans_read_deps[i] = trans_read_deps[argument_deps[i].back()];
			}
			if (!ari.op1.is_literal())
			{
				argument_deps[i].push_back(local_written_at[ari.op1.get_local_id()]);
				if (argument_deps[i].back() != -1)
				{
					vec<uint32_t> nwtransdp;
					uint32_t p0 = 0, p1 = 0;
					vec<uint32_t> &d0 = trans_read_deps[i], &d1 = trans_read_deps[argument_deps[i].back()];
					while (true)
					{
						if (p0 != d0.size() && p1 != d1.size())
						{
							if (d0[p0] == d1[p1])
							{
								nwtransdp.push_back(d0[p0++]);
								p1++;
							}
							else if (d0[p0] < d1[p1])
								nwtransdp.push_back(d0[p0++]);
							else
								nwtransdp.push_back(d1[p1++]);
						}
						else if (p0 != d0.size())
							nwtransdp.push_back(d0[p0++]);
						else if (p1 != d1.size())
							nwtransdp.push_back(d1[p1++]);
						else
							break;
					}
					swap(trans_read_deps[i], nwtransdp);
				}
			}
			local_written_at[ari.target] = i;
		}
		else if (instr.is_write() || instr.is_move() || instr.is_print())
		{
			const LocalValue& data = [&instr]()
			{
				if (instr.is_write())
					return instr.get_write_data();
				if (instr.is_move())
					return instr.as_move().data;
				assert(instr.is_print());
				return instr.get_print_arg();
			}();
			if (!data.is_literal())
			{
				const uint32_t written_from = local_written_at[data.get_local_id()];
				argument_deps[i].push_back(written_from);
				trans_read_deps[i] = trans_read_deps[written_from];
			}
			if (instr.is_move())
				local_written_at[instr.as_move().local_id] = i;
		}
		else if (instr.is_read())
			local_written_at[instr.get_read_target()] = i;
	}

	// initialized instr_evaluated and instr_value
	instr_evaluated = vec<bool>(instructions.size());
	instr_value = vec<int32_t>(instructions.size());
}

void Snippet::exec_eval(const uint32_t instri)
{
	const Instruction& instr = instructions[instri];
	if (instr.is_arithmetic())
	{
		const ArithmeticInstruction& ari = instr.as_arithmetic();
		uint32_t nex = 0;
		const int32_t v0 = ari.op0.is_literal() ? ari.op0.get_literal() : instr_value[argument_deps[instri][nex++]];
		const int32_t v1 = ari.op1.is_literal() ? ari.op1.get_literal() : instr_value[argument_deps[instri][nex]];
		int32_t res;
		if (ari.op_type == ArithmeticOpType::Add)
			res = static_cast<uint32_t>(v0) + static_cast<uint32_t>(v1);
		else if (ari.op_type == ArithmeticOpType::Subtract)
			res = static_cast<uint32_t>(v0) - static_cast<uint32_t>(v1);
		else if (ari.op_type == ArithmeticOpType::Multiply)
			res = static_cast<uint32_t>(v0) * static_cast<uint32_t>(v1);
		else if (ari.op_type == ArithmeticOpType::Divide)
		{
			if (v1 == 0)
			{
				zerodiv_excepted = true;
				return;
			}
			res = static_cast<int64_t>(v0) / v1;
		}
		else if (ari.op_type == ArithmeticOpType::Modulo)
		{
			if (v1 == 0)
			{
				zerodiv_excepted = true;
				return;
			}
			res = static_cast<int64_t>(v0) % v1;
		}
		else
			assert(false);
		instr_value[instri] = res;
	}
	else if (instr.is_write() || instr.is_move() || instr.is_print())
	{
		const LocalValue& data = [&instr]()
		{
			if (instr.is_write())
				return instr.get_write_data();
			if (instr.is_move())
				return instr.as_move().data;
			return instr.get_print_arg();
		}();
		instr_value[instri] = data.is_literal() ? data.get_literal() : instr_value[argument_deps[instri][0]];
	}
	instr_evaluated[instri] = true;
}

void Snippet::request_eval(const uint32_t instri)
{
	if (instr_evaluated[instri])
		return;
	std::stack<std::pair<uint32_t, uint32_t>> st;
	st.push({ instri, 0 });
	while (!st.empty())
	{
		auto& cur = st.top();
		if (cur.second == argument_deps[cur.first].size())
		{
			exec_eval(cur.first);
			st.pop();
		}
		else if (!instr_evaluated[argument_deps[cur.first][cur.second++]])
			st.push({ argument_deps[cur.first][cur.second - 1], 0 });
	}
}

void Snippet::prepare_execution()
{
	fill(instr_evaluated.begin(), instr_evaluated.end(), false);
	zerodiv_excepted = false;
}

vec<int32_t> Snippet::get_execution_results()
{
	vec<int32_t> ress;
	for (uint32_t i = 0; i < instructions.size(); i++)
		if (instructions[i].is_print())
		{
			request_eval(i);
			ress.push_back(instr_value[i]);
		}
	return ress;
}

int32_t Snippet::read_write(const uint32_t action_index)
{
	const uint32_t instri = actions[action_index];
	assert(instructions[instri].is_write());
	request_eval(instri);
	return instr_value[instri];
}

const vec<uint32_t>& Snippet::get_write_dependencies(const uint32_t action_index) const
{
	return trans_read_deps[actions[action_index]];
}

void Snippet::supply_read_value(const uint32_t action_index, const int32_t value)
{
	const uint32_t instri = actions[action_index];
	assert(instructions[instri].is_read());
	instr_evaluated[instri] = true;
	instr_value[instri] = value;
}

}
