#include "analysis.hpp"

#include <algorithm>
#include <cassert>
#include <limits>

#include "jmme-scanner.hpp"
#include "parser.hpp"
#include "snippet.hpp"

namespace JMMExplorer
{

using std::pair;

bool ExceptedExecutionResult::operator==(const ExceptedExecutionResult& other) const
{
	return ex_thread == other.ex_thread && ex_line == other.ex_line;
}

bool ExecutionResult::operator==(const ExecutionResult& other) const
{
	return result.index() == other.result.index()
		&& (result.index() == 0 ? std::get<RegularExecutionResult>(result) == std::get<RegularExecutionResult>(other.result)
								: std::get<ExceptedExecutionResult>(result) == std::get<ExceptedExecutionResult>(other.result));
}

bool ExecutionResult::operator!=(const ExecutionResult& other) const
{
	return !(*this == other);
}

void ExecutionResult::print(std::ostream& os, const std::function<std::string(uint32_t)>& thread_name_fetcher) const
{
	if (std::holds_alternative<RegularExecutionResult>(result))
	{
		bool frst = true;
		for (const vec<int32_t>& snpout : std::get<RegularExecutionResult>(result))
		{
			if (!frst)
				os << "|";
			frst = false;
			os << " ";
			for (const int32_t val : snpout)
				os << val << " ";
		}
	}
	else
	{
		const ExceptedExecutionResult& eres = std::get<ExceptedExecutionResult>(result);
		os << "division by zero exception in thread " << eres.ex_thread << " (" << thread_name_fetcher(eres.ex_thread) << ") at line " << eres.ex_line;
	}
}

/// Returns true and prints error messages to err_out if and only if at least one of the snippets doesn't have monitor locks and unlocks correctly paired
/// (given a particular snippet, the locks and unlocks are correctly paired if and only if for every monitor, the number of already performed locks is
/// greater than or equal to the number of already performed unlocks at all times and these two numbers are equal at the end of the snippet)
static bool check_monitor_use(const vec<Snippet>& snps, std::ostream& err_out)
{
	bool ret = false;
	for (const Snippet& snp : snps)
	{
		std::unordered_map<Ident, uint32_t> locked;
		for (uint32_t i = 0; i < snp.action_count(); i++)
		{
			const Instruction& action = snp.get_action(i);
			if (action.is_lock())
				locked[action.get_monitor_name()]++;
			else if (action.is_unlock())
			{
				const Ident mname = action.get_monitor_name();
				if (locked[mname] == 0)
				{
					err_out << "Error: Unlocking monitor " << mname << " in " << snp.get_name() << " at " << action.location << std::endl;
					ret = true;
				}
				else
					locked[mname]--;
			}
		}
	}
	return ret;
}

/// Simulates the execution of the program given a particular write-seen function (it either produces the corresponding output or,
/// if the write-seen function forms a dependency cycle, returns without producing any output)
static void analyze_fixed_write_seen(vec<Snippet>& snps, const uint32_t globc, const vec<vec<uint32_t>>& to_glob_action, const vec<std::pair<uint32_t, uint32_t>>& to_thread_action, const std::function<const Instruction&(uint32_t)>& get_action, const std::unordered_map<uint32_t,uint32_t>& gintr_to_rix, const vec<int32_t>& write_seen, const vec<uint32_t>& reads, vec<ExecutionResult>& results)
{
	for (Snippet& snp : snps)
		snp.prepare_execution();

	// value at index i is the number of reads on which read i depends that have not yet been evaluated
	vec<uint32_t> outstanding(reads.size());

	// index i stores all reads that depend on read i
	vec<vec<uint32_t>> used_by(reads.size());

	uint32_t nr = 0;
	for (uint32_t i = 0; i < globc; i++)
	{
		const Instruction& action = get_action(i);
		if (action.is_read())
		{
			if (write_seen[nr] != -1)
			{
				const pair<uint32_t, uint32_t> wthreada = to_thread_action[write_seen[nr]];
				const vec<uint32_t>& deps = snps[wthreada.first].get_write_dependencies(wthreada.second);
				outstanding[nr] = deps.size();
				for (const uint32_t dep : deps)
					used_by[gintr_to_rix.at(to_glob_action[wthreada.first][dep])].push_back(nr);
			}
			else
				outstanding[nr] = 0;
			nr++;
		}
	}

	// whether a division by zero exception has happened
	bool excepted = false;

	// thread and line number where the zero exception has happened (assuming it has)
	uint32_t excepted_thread, excepted_line;

	// all reasd that can be evaluated (because all their dependencies have already been evaluated)
	vec<uint32_t> ready;

	for (uint32_t i = 0; i < reads.size(); i++)
		if (outstanding[i] == 0)
			ready.push_back(i);

	// number of already evaluated reads
	uint32_t reads_done = 0;

	// keep evaluating reads as long as there are some that can be evaluated
	while (!ready.empty())
	{
		const uint32_t cur = ready.back();
		ready.pop_back();
		reads_done++;
		const pair<uint32_t, uint32_t> readti = to_thread_action[reads[cur]];
		const int32_t val = write_seen[cur] != -1 ? [&]()
		{ 
			const pair<uint32_t, uint32_t> writeti = to_thread_action[write_seen[cur]];
			const int32_t value = snps[writeti.first].read_write(writeti.second);
			if (snps[writeti.first].is_zerodiv_excepted())
			{
				excepted = true;
				excepted_thread = writeti.first;
				excepted_line = snps[writeti.first].get_excepted_line();
			}
			return value;
		}() : 0;
		if (excepted)
			break;
		snps[readti.first].supply_read_value(readti.second, val);
		for (const uint32_t dependent : used_by[cur])
		{
			outstanding[dependent]--;
			if (outstanding[dependent] == 0)
				ready.push_back(dependent);

		}
	}

	// reads_done == reads.size() means that the program was successfully executed with this write-seen function,
	// i.e., there were no dependency cycles

	if (!excepted && reads_done == reads.size())
	{
		vec<vec<int32_t>> newout;
		for (uint32_t i = 0; i < snps.size(); i++)
		{
			Snippet& snp = snps[i];
			newout.push_back(snp.get_execution_results());
			if (snp.is_zerodiv_excepted())
			{
				excepted = true;
				excepted_thread = i;
				excepted_line = snp.get_excepted_line();
				break;
			}
		}
		if (!excepted)
		{
			const ExecutionResult res{ newout };
			if (std::all_of(results.begin(), results.end(), [&res](const ExecutionResult& thisout){ return thisout != res; }))
				results.push_back(res);
		}
	}
	if (excepted)
	{
		const ExecutionResult res{ ExceptedExecutionResult{ excepted_thread, excepted_line } };
		if (std::all_of(results.begin(), results.end(), [&res](const ExecutionResult& thisout){ return thisout != res; }))
			results.push_back(res);
	}
}

/// Iterates through and tries possible executions given a particular synchronization order
static void analyze_fixed_so(vec<Snippet>& snps, const uint32_t globc, const vec<vec<uint32_t>>& to_glob_action, const vec<std::pair<uint32_t, uint32_t>>& to_thread_action, const vec<uint32_t>& so, const uint32_t synaction_count, vec<ExecutionResult>& results)
{
	// check that monitors are paired correctly
	std::unordered_map<Ident, uint32_t> holding_thread;
	std::unordered_map<Ident, uint32_t> hold_count;

	const auto get_action = [&](const uint32_t globi) -> const Instruction&
	{
		const pair<uint32_t, uint32_t> thread_action = to_thread_action[globi];
		return snps[thread_action.first].get_action(thread_action.second);
	};

	for (uint32_t i = 0; i < synaction_count; i++)
	{
		const Instruction& action = get_action(so[i]);
		if (action.is_lock())
		{
			const str& mname = action.get_monitor_name();
			const uint32_t this_thread = to_thread_action[so[i]].first;
			if (hold_count[mname] && holding_thread[mname] != this_thread)
				return;
			hold_count[mname]++;
			holding_thread[mname] = this_thread;
		}
		else if (action.is_unlock())
		{
			const str& mname = action.get_monitor_name();
			const uint32_t this_thread = to_thread_action[so[i]].first;
			assert(hold_count[mname]);
			assert(holding_thread[mname] == this_thread);
			hold_count[mname]--;
		}
	}

	vec<vec<bool>> hb(globc, vec<bool>(globc, false));

	// add reflexivity to HB
	for (uint32_t i = 0; i < globc; i++)
		hb[i][i] = true;

	// add (a transitive skeleton) of all the program orders to HB
	for (uint32_t i = 0; i < snps.size(); i++)
		for (uint32_t j = 0; j + 1 < snps[i].action_count(); j++)
			hb[to_glob_action[i][j]][to_glob_action[i][j + 1]] = true;

	// add the synchronizes-with edges to HB
	for (uint32_t i = 0; i < synaction_count; i++)
	{
		const Instruction& action = get_action(so[i]);
		if (action.is_unlock())
			for (uint32_t j = i + 1; j < synaction_count; j++)
			{
				const Instruction& a2 = get_action(so[j]);
				if (a2.is_lock() && a2.get_monitor_name() == action.get_monitor_name())
					hb[so[i]][so[j]] = true;
			}
		else if (action.is_volatile_write())
			for (uint32_t j = i + 1; j < synaction_count; j++)
			{
				const Instruction& a2 = get_action(so[j]);
				if (a2.is_volatile_read() && a2.get_volatile_name() == action.get_volatile_name())
					hb[so[i]][so[j]] = true;
			}
	}

	// completes HB by calculating the transitive closure
	for (uint32_t i = 0; i < globc; i++)
		for (uint32_t j = 0; j < globc; j++)
			for (uint32_t k = 0; k < globc; k++)
				if (hb[j][i] && hb[i][k])
					hb[j][k] = true;

	// assert that HB is a partial order
	for (uint32_t i = 0; i < globc; i++)
		for (uint32_t j = 0; j < globc; j++)
			assert(i == j || !hb[i][j] || !hb[j][i]);

	// indices of all shared reads
	vec<uint32_t> shared_reads;

	// for each shared read, the set of all writes that can be seen by it if compliant with HB
	vec<vec<int32_t>> pss_write_seen;

	for (uint32_t i = 0; i < globc; i++)
	{
		const Instruction& action = get_action(i);
		if (action.is_shared_read())
		{
			shared_reads.push_back(i);
			vec<int32_t> seeable;
			vec<uint32_t> preceding_writes;
			for (uint32_t j = 0; j < globc; j++)
			{
				const Instruction& a2 = get_action(j);
				if (a2.is_shared_write() && action.get_shared_name() == a2.get_shared_name())
				{
					if (hb[j][i])
						preceding_writes.push_back(j);
					else if (!hb[i][j])
						seeable.push_back(j);
				}
			}
			for (const uint32_t p0 : preceding_writes)
				if (std::all_of(preceding_writes.begin(), preceding_writes.end(), [&hb, p0](const uint32_t p1){ return p0 == p1 || !hb[p0][p1]; }))
					seeable.push_back(p0);
			if (preceding_writes.empty())
				seeable.push_back(-1);
			pss_write_seen.push_back(seeable);
		}
	}

	// map: global action index of a read action -> index of the read
	std::unordered_map<uint32_t, uint32_t> gintr_to_rix;

	// global action indices of all read actions
	vec<uint32_t> reads;

	for (uint32_t i = 0; i < globc; i++)
		if (get_action(i).is_read())
		{
			gintr_to_rix[i] = reads.size();
			reads.push_back(i);
		}

	vec<uint32_t> write_seen_i(shared_reads.size(), 0);
	while (true)
	{
		// construct write seen from index array

		vec<int32_t> write_seen(reads.size());
		uint32_t nr = 0, nshr = 0;
		for (uint32_t i = 0; i < globc; i++)
		{
			const Instruction& action = get_action(i);
			if (action.is_volatile_read())
			{
				// go up the synchronization order to find the matching write
				int32_t latest_write = -1;
				for (uint32_t j = 0; j < synaction_count; j++)
				{
					const Instruction& waction = get_action(so[j]);
					if (waction.is_volatile_write() && waction.get_volatile_name() == action.get_volatile_name())
						latest_write = so[j];
					else if (so[j] == i)
						break;
				}
				write_seen[nr] = latest_write;
				nr++;
			}
			else if (action.is_shared_read())
			{
				write_seen[nr] = pss_write_seen[nshr][write_seen_i[nshr]];
				nshr++;
				nr++;
			}
		}

		// use write seen
		analyze_fixed_write_seen(snps, globc, to_glob_action, to_thread_action, get_action, gintr_to_rix, write_seen, reads, results);

		// update write seen index array
		if (write_seen_i.empty())
			break;
		write_seen_i[0]++;
		uint32_t poi = 0;
		while (poi < shared_reads.size() && write_seen_i[poi] == pss_write_seen[poi].size())
		{
			write_seen_i[poi] = 0;
			poi++;
			if (poi < shared_reads.size())
				write_seen_i[poi]++;
		}
		if (poi == shared_reads.size())
			break;
	}
}

bool analyze(const vec<std::string>& filenames, const vec<std::istream*>& inputs, vec<ExecutionResult>& results, std::ostream& err_out)
{
	// parse the source code
	vec<Snippet> snps;
	snps.reserve(inputs.size());
	for (uint32_t i = 0; i < inputs.size(); i++)
	{
		snps.push_back(Snippet(filenames[i]));
		JMMEScanner scn(inputs[i]);
		JMMEParser prs(scn, snps.back());
		prs();
	}
	
	// check that the monitors are used correctly in each file
	if (check_monitor_use(snps, err_out))
	{
		err_out << "Terminating due to invalid monitor use." << std::endl;
		return true;
	}

	// converts from the global indexing of actions to the per-thread indexing
	vec<pair<uint32_t, uint32_t>> to_thread_action;

	// converts from the per-thread indexing of actions to the global indexing
	vec<vec<uint32_t>> to_glob_action(snps.size());

	// total number of actions
	uint32_t globc = 0;

	// populate to_thread_action and to_glob_action
	for (uint32_t i = 0; i < snps.size(); i++)
	{
		const uint32_t action_count = snps[i].action_count();
		for (uint32_t j = 0; j < action_count; j++)
		{
			to_thread_action.push_back({ i, j });
			to_glob_action[i].push_back(globc++);
		}
	}

	// for each thread, holds the indices of all synchronziation actions
	vec<vec<uint32_t>> synactions(snps.size());

	// total number of synchronization actions
	uint32_t synaction_count = 0;

	for (uint32_t i = 0; i < snps.size(); i++)
	{
		synactions[i] = snps[i].get_synchronization_actions();
		synaction_count += synactions[i].size();
	}

	// the current synchronization order allocation -- for every place, indicates an syn. action from which thread should be there
	vec<uint32_t> so_thread_alloc(synaction_count);
	{
		uint32_t nxt = 0;
		for (uint32_t i = 0; i < snps.size(); i++)
			for (uint32_t j = 0; j < synactions[i].size(); j++)
				so_thread_alloc[nxt++] = i;
	}

	for (Snippet& snp : snps)
		snp.run_preexecution_analysis();

	while (true)
	{
		// index i holds the global index of the syn. action that comes i-th in the syn. order
		vec<uint32_t> so(synaction_count);
		{
			vec<uint32_t> nxts(snps.size(), 0);
			for (uint32_t i = 0; i < synaction_count; i++)
			{
				const uint32_t threadi = so_thread_alloc[i];
				so[i] = to_glob_action[threadi][synactions[threadi][nxts[threadi]++]];
			}
		}

		analyze_fixed_so(snps, globc, to_glob_action, to_thread_action, so, synaction_count, results);
		
		// update so_thread_alloc

		// temporary placeholder value for a free spot in the algorithm that generates all possible synchronization orders
		const uint32_t free_slot = std::numeric_limits<uint32_t>::max();

		bool updated = false;
		for (int32_t i = snps.size() - 2; i >= 0; i--)
		{
			// in each iteration, try to advance the places allocated for thread i forward by one (in a particular order of subsets of a given size)
			// without moving any allocations for threads with lower indices
			
			// lowest visited index that is occupied by a thread with an index greater than i
			int32_t next_free = -1; 

			// number of visited indices occupied by thread i
			uint32_t self_seen = 0;

			// iterate through the indices of the synchronization order back to front
			for (int32_t j = synaction_count - 1; j >= 0; j--)
			{
				if (so_thread_alloc[j] > i)
					next_free = j;
				else if (so_thread_alloc[j] == i)
				{
					if (next_free != -1)
					{
						so_thread_alloc[j] = free_slot;
						so_thread_alloc[next_free] = i;
						for (uint32_t k = next_free + 1; self_seen; k++)
						{
							if (so_thread_alloc[k] > i)
							{
								so_thread_alloc[k] = i;
								self_seen--;
							}
						}
						updated = true;
						break;
					}
					else
					{
						so_thread_alloc[j] = free_slot;
						self_seen++;
					}
				}
			}

			// after updating the subset of slots allocated for thread i, change the configuration of the threads with indices > i to the minimum
			
			if (updated)
			{
				uint32_t nxt = 0;
				for (uint32_t j = i + 1; j < snps.size(); j++)
				{
					uint32_t left = synactions[j].size();
					while (left)
					{
						if (so_thread_alloc[nxt] > i)
						{
							so_thread_alloc[nxt] = j;
							left--;
						}
						nxt++;
					}
				}
				break;
			}
		}
		if (!updated)
			break;
	}
	return false;
}

}
