#ifndef SNIPPET_HPP
#define SNIPPET_HPP

#include <unordered_map>
#include <unordered_set>
#include <variant>

#include "jmme-language.hpp"
#include "vec.hpp"

namespace JMMExplorer
{

class Snippet
{
public:

struct LocalValue
{
	std::variant<size_t, uint32_t> value;

	static LocalValue from_literal(uint32_t literal);

	LocalValue() = default;
private:

	static LocalValue from_local(size_t local_id);

	bool is_literal() const;
	uint32_t get_literal() const;
	size_t get_local_id() const;

	friend Snippet;
};

	void emit_method_call(const Ident& object_name, const Ident& method_name, ILoc loc);
	void emit_static_call(const Ident& function_name, LocalValue arg, ILoc loc);
	LocalValue emit_read(const Ident& var_name, ILoc loc);
	void emit_write(const Ident& target_name, LocalValue data, ILoc loc);
	void emit_op_write(const Ident& target_name, LocalValue op, ArithmeticOpType op_type, ILoc loc);
	LocalValue emit_arithmetic(LocalValue op0, LocalValue op1, ArithmeticOpType op_type, ILoc loc);

	void print(std::ostream& os) const;

private:
	size_t next_temporary = 0;
	vec<Ident> locals;
	std::unordered_map<Ident, size_t> local_to_id;
	std::unordered_set<Ident> used_monitors, used_shareds, used_volatiles;

	size_t allocate_temporary();

	struct LockInstruction
	{
		Ident monitor_name;
	};
	
	struct UnlockInstruction
	{
		Ident monitor_name;
	};
	
	struct ArithmeticInstruction
	{
		size_t target;
		LocalValue op0;
		ArithmeticOpType op_type;
		LocalValue op1;
	};
	
	struct SharedReadInstruction
	{
		size_t target;
		Ident shared_source;
	};
	
	struct SharedWriteInstruction
	{
		Ident shared_target;
		LocalValue data;
	};
	
	struct VolatileReadInstruction
	{
		size_t target;
		Ident volatile_source;
	};
	
	struct VolatileWriteInstruction
	{
		Ident volatile_target;
		LocalValue data;
	};
	
	struct MoveInstruction
	{
		size_t local_id;
		LocalValue data;
	};
	
	struct PrintInstruction
	{
		LocalValue arg;
	};

	struct Instruction
	{
		std::variant<
			LockInstruction,
			UnlockInstruction,
			ArithmeticInstruction,
			SharedReadInstruction,
			SharedWriteInstruction,
			VolatileReadInstruction,
			VolatileWriteInstruction,
			MoveInstruction,
			PrintInstruction
			> instr;
		ILoc location;
	};
	
	vec<Instruction> instructions;
};

}

#endif // SNIPPET_HPP
