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

/// Represents a value during the execution of a snippet that is either a literal constant or the value of a local variable
struct LocalValue
{
	/// Index of the local variable or the literal value
	std::variant<size_t, uint32_t> value;

	/// Constructs a LocalValue that has the value of constant literal
	static LocalValue from_literal(uint32_t literal);

	LocalValue() = default;
private:

	/// Construct a LocalValue that has the value of a local variable
	static LocalValue from_local(size_t local_id);

	/// Returns true iff this is a value of constant literal
	bool is_literal() const;
	/// Assuming this holds a constant literal value, returns the value of that literal
	uint32_t get_literal() const;
	/// Assuming this refers to the value of local variable, returns the index of that local variable
	size_t get_local_id() const;

	friend Snippet;
};

/// Instruction that locks a monitor
struct LockInstruction
{
	Ident monitor_name;
};

/// Instruction that unlocks a monitor
struct UnlockInstruction
{
	Ident monitor_name;
};

/// Instruction that performs a binary arithmetic operattion
struct ArithmeticInstruction
{
	/// Index of the target local variable
	size_t target;

	/// First operand
	LocalValue op0;
	
	/// Operation type
	ArithmeticOpType op_type;

	/// Second operand
	LocalValue op1;
};

/// Instruction that reads a non-volatile shared variable into a local variable
struct SharedReadInstruction
{
	/// Index of the target local variable
	size_t target;

	/// Name of the variable to be read
	Ident shared_source;
};

/// Instruction that writes a non-volatile shared variable
struct SharedWriteInstruction
{
	/// Name of the variable to write to
	Ident shared_target;

	/// Local data to write
	LocalValue data;
};

/// Instruction that reads a volatile shared variable into a local variable
struct VolatileReadInstruction
{
	/// Index of the target local variable
	size_t target;

	/// Name of the variable to be read
	Ident volatile_source;
};

/// Instruction that writes a volatile shared variable
struct VolatileWriteInstruction
{
	/// Name of the variable to write to
	Ident volatile_target;

	/// Local data to write
	LocalValue data;
};

/// Instruction that write a local value into a local variable
struct MoveInstruction
{
	/// Index of the target local variable
	size_t local_id;

	/// Local data to write
	LocalValue data;
};

/// Instruction used for communicating the program output
struct PrintInstruction
{
	LocalValue arg;
};

/// One instruction in our transformed representation of the inputted program
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
	/// Consumes a parsed method call of method_name on object_name with an empty argument list (to be used by the parser)
	void emit_method_call(const Ident& object_name, const Ident& method_name, ILoc loc);
	/// Consumes a parsed call of static function function_name with a single argument given by arg (to be used by the parser)
	void emit_static_call(const Ident& function_name, LocalValue arg, ILoc loc);
	/// Returns a LocalValue that represent the current value refered to by var_name in the snippet
	LocalValue emit_read(const Ident& var_name, ILoc loc);
	/// Consumes a parsed simple assignment of data to target_name
	void emit_write(const Ident& target_name, LocalValue data, ILoc loc);
	/// Consumes a parsed compund assignement of op to target_name of type op_type
	void emit_op_write(const Ident& target_name, LocalValue op, ArithmeticOpType op_type, ILoc loc);
	/// Returns a LocalValue that represents the result of applying operation of type op_type on values op0 and op1
	LocalValue emit_arithmetic(LocalValue op0, LocalValue op1, ArithmeticOpType op_type, ILoc loc);

	/// Prints the instructions of this snippet using a set of predefined mnemonics; one instruction per line
	void print(std::ostream& os) const;

	/// Returns the number of instructions in this snippet which are JMM actions
	size_t action_count() const;
	/// Returns the index-th action of this snippet (starting at 0)
	const Instruction& get_action(uint32_t index) const;
	/// Returns the indices of all actions which are also synchronization actions
	vec<uint32_t> get_synchronization_actions() const;

	Snippet() = default;
	Snippet(const str& name);

	const str& get_name() const;

	/// Should be run exactly once after the parse has finished emitting instructions and before any execution starts or dependencies are requested (computes dependencies between instructions etc.)
	void run_preexecution_analysis();
	/// Should be run (at least) once before the start of every new execution (clears the program state)
	void prepare_execution();
	/// Assuming the value for all reads has already been supplied, returns the value printed out by every print call. The order of the values is the same as of the print statements in the source code
	vec<int32_t> get_execution_results();
	/// Assuming the value for all reads it depends on has already been supplied, returns the value written by the write which is the action_index-th (zero-based) action
	int32_t read_write(uint32_t action_index);
	/// Returns a vector of action indices which are of the reads that the write with action index action_index depends on
	const vec<uint32_t>& get_write_dependencies(uint32_t action_index) const;
	/// Saves what the value to be read by the read at action index action_index should be
	void supply_read_value(uint32_t action_index, int32_t value);
	/// Returns true if and only if some instruction in the current execution caused a division by zero exception; if that was the case, the execution shouldn't be continued
	bool is_zerodiv_excepted() const;
	/// If is_zero_div_excepted returns true, this function return the line number where the exception occurred
	uint32_t get_excepted_line() const;

private:
	// name of this snippet (often the file name)
	str name;

	// number of the next compiler temporary variable (used for its name)
	size_t next_temporary = 0;

	// names of all the local variables
	vec<Ident> locals;

	// maps local variable name to local variable index
	std::unordered_map<Ident, size_t> local_to_id;

	// creates a new compiler temporary variable and returns its index as a local variable
	size_t allocate_temporary();

	// should be called whenever the last instruction is a JMM action and so it should be added to the list of actions
	void pushed_action();
	
	// snippet's instructions
	vec<Instruction> instructions;

	// indices in the instructions array of all the instructions that are JMM actions
	vec<uint32_t> actions;

	// for each instruction, for each LocalValue input that is a local variable (and not a constant literal), stores the index of the instruction that produces the value that should be read there -- if the default zero initialization should be read, stores a -1
	vec<vec<int32_t>> argument_deps;

	// for each instruction, stores the list of all read instruction indices that the instruction (even transitively) depends on
	vec<vec<uint32_t>> trans_read_deps;

	// for each instruction, stores true iff the instruction has already been evaluated
	vec<bool> instr_evaluated;

	// for each instruction, stores the value that it produced assuming that it has already be evaluated
	vec<int32_t> instr_value;

	// evaluates instruction instri assuming that all instructions it depends on have already been evaluated
	void exec_eval(uint32_t isntri);

	// recursively evaluates instruction instri and instructions it depends on assuming that all reads it transitively depends on have been supplied with a value
	void request_eval(uint32_t instri);

	// true iff a division by zero has occurred in the current execution
	bool zerodiv_excepted;
	
	// if zerodiv_excepted is true, this variable stores the line number where the exception occurred
	uint32_t excepted_at_line;
};

}

#endif // SNIPPET_HPP
