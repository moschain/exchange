#include "defi.mining.hpp"

void mining::deferred_check()
{
	auto now = current_time_point().elapsed.count() / 1000;
	auto next_issue_time = start_date + (issued_days + 1) * milliseconds_per_day;
	auto delay_sec = next_issue_time < now ? 10 : (next_issue_time - now) / 1000;

	eosio::transaction out;
	out.actions.emplace_back(permission_level{get_self(), active_permission},
			get_self(), "dailyissue"_n,
			std::make_tuple(quantity));
	out.delay_sec = delay_sec;
	eosio::cancel_deferred(id);
	out.send(id, get_self(), true);
}