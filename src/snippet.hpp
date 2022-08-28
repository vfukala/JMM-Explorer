#ifndef SNIPPET_HPP
#define SNIPPET_HPP

#include <unordered_map>
#include <unordered_set>
#include <variant>

#include "jmme-language.hpp"
#include "vec.hpp"

namespace JMMExplorer
{

class Snippet;

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

	bool is_action() const;
	bool is_synchronization() const;
	bool is_lock() const;
	bool is_unlock() const;
	Ident get_monitor_name() const;
	bool is_volatile_read() const;
	bool is_volatile_write() const;
	Ident get_volatile_name() const;
	bool is_shared_read() const;
	bool is_shared_write() const;
	Ident get_shared_name() const;
	bool is_arithmetic() const;
	bool is_move() const;
	bool is_print() const;
	bool is_read() const;
	bool is_write() const;
	const LocalValue& get_write_data() const;
	size_t get_read_target() const;
	const ArithmeticInstruction& as_arithmetic() const;
	const MoveInstruction& as_move() const;
	const LocalValue& get_print_arg() const;
};

class Snippet
{
public:
	void emit_method_call(const Ident& object_name, const Ident& method_name, ILoc loc);
	void emit_static_call(const Ident& function_name, LocalValue arg, ILoc loc);
	LocalValue emit_read(const Ident& var_name, ILoc loc);
	void emit_write(const Ident& target_name, LocalValue data, ILoc loc);
	void emit_op_write(const Ident& target_name, LocalValue op, ArithmeticOpType op_type, ILoc loc);
	LocalValue emit_arithmetic(LocalValue op0, LocalValue op1, ArithmeticOpType op_type, ILoc loc);

	void print(std::ostream& os) const;

	size_t action_count() const;
	const Instruction& get_action(uint32_t index) const;
	vec<uint32_t> get_synchronization_actions() const;

	Snippet() = default;
	Snippet(const str& name);

	const str& get_name() const;

	void run_preexecution_analysis();
	void prepare_execution();
	vec<int32_t> get_execution_results();
	int32_t read_write(uint32_t action_index);
	const vec<uint32_t>& get_write_dependencies(uint32_t action_index) const;
	void supply_read_value(uint32_t action_index, int32_t value);

private:
	str name;
	size_t next_temporary = 0;
	vec<Ident> locals;
	std::unordered_map<Ident, size_t> local_to_id;
	std::unordered_set<Ident> used_monitors, used_shareds, used_volatiles;

	size_t allocate_temporary();
	void pushed_action();
	
	vec<Instruction> instructions;
	vec<uint32_t> actions;

	vec<vec<int32_t>> argument_deps;
	vec<vec<uint32_t>> trans_read_deps;

	vec<bool> instr_evaluated;
	vec<int32_t> instr_value;

	void exec_eval(uint32_t isntri);
	void request_eval(uint32_t instri);

	bool zerodiv_excepted;
};

}

#endif // SNIPPET_HPP
