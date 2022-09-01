#include "analysis.hpp"

#include <algorithm>
#include <limits>

#include "jmme-scanner.hpp"
#include "parser.hpp"
#include "snippet.hpp"

namespace JMMExplorer
{

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
		os << "div by zero exception in thread " << eres.ex_thread << " (" << thread_name_fetcher(eres.ex_thread) << ") at line " << eres.ex_line;
	}
}


bool analyze(const vec<std::string>& filenames, const vec<std::istream*>& inputs, vec<ExecutionResult>& results, std::ostream& err_out)
{
	vec<Snippet> snps;
	snps.reserve(inputs.size());
	for (uint32_t i = 0; i < inputs.size(); i++)
	{
		snps.push_back(Snippet(filenames[i]));
		JMMEScanner scn(inputs[i]);
		JMMEParser prs(scn, snps.back());
		prs();
	}
	bool invalid_monitors = false;
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
					invalid_monitors = true;
				}
				else
					locked[mname]--;
			}
		}
	}
	if (invalid_monitors)
	{
		err_out << "Terminating due to invalid monitor use." << std::endl;
		return true;
	}
	using std::pair;
	vec<pair<uint32_t, uint32_t>> to_thread_action;
	vec<vec<uint32_t>> to_glob_action(snps.size());
	uint32_t globc = 0;
	for (uint32_t i = 0; i < snps.size(); i++)
		for (uint32_t j = 0; j < snps[i].action_count(); j++)
		{
			to_thread_action.push_back({ i, j });
			to_glob_action[i].push_back(globc++);
		}
	vec<vec<uint32_t>> synactions(snps.size());
	uint32_t synaction_count = 0;
	for (uint32_t i = 0; i < snps.size(); i++)
	{
		synactions[i] = snps[i].get_synchronization_actions();
		synaction_count += synactions[i].size();
	}
	const uint32_t free_slot = std::numeric_limits<uint32_t>::max();
	vec<uint32_t> so_thread_alloc(synaction_count, free_slot);
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
		// use SO here
		vec<uint32_t> so(synaction_count);
		{
			vec<uint32_t> nxts(snps.size(), 0);
			for (uint32_t i = 0; i < synaction_count; i++)
			{
				const uint32_t threadi = so_thread_alloc[i];
				so[i] = to_glob_action[threadi][synactions[threadi][nxts[threadi]++]];
			}
		}

		[&]() -> void
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

			vec<uint32_t> shared_reads;
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

			std::unordered_map<uint32_t, uint32_t> gintr_to_rix;
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
							const Instruction& action = get_action(so[j]);
							if (action.is_volatile_write() && action.get_volatile_name() == action.get_volatile_name())
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
				
				for (Snippet& snp : snps)
					snp.prepare_execution();

				vec<uint32_t> outstanding(reads.size());
				vec<vec<uint32_t>> used_by(reads.size());
				nr = 0;
				for (uint32_t i = 0; i < globc; i++)
				{
					const Instruction& action = get_action(i);
					if (action.is_read())
					{
						if (write_seen[nr] != -1)
						{
							pair<uint32_t, uint32_t> wthreada = to_thread_action[write_seen[nr]];
							const vec<uint32_t>& deps = snps[wthreada.first].get_write_dependencies(wthreada.second);
							outstanding[nr] = deps.size();
							for (const uint32_t dep : deps)
								used_by[gintr_to_rix[to_glob_action[wthreada.first][dep]]].push_back(nr);
						}
						else
							outstanding[nr] = 0;
						nr++;
					}
				}

				bool excepted = false;
				uint32_t excepted_thread, excepted_line;

				vec<uint32_t> ready;
				for (uint32_t i = 0; i < reads.size(); i++)
					if (outstanding[i] == 0)
						ready.push_back(i);

				uint32_t reads_done = 0;
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
		}();
		
		// update SO
		bool updated = false;
		for (int i = snps.size() - 2; i >= 0; i--)
		{
			int32_t next_free = -1;
			uint32_t self_seen = 0;
			for (int j = synaction_count - 1; j >= 0; j--)
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
