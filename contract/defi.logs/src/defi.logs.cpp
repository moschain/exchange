#include <defi.logs.hpp>

void logs::swaplog(name user, uint64_t mid, asset amountIn, asset amountOut, string memo)
{
    require_auth(get_self());
}

void logs::depositlog(name user, uint64_t mid, asset quantity0, asset quantity1)
{
    require_auth(get_self());
}

void logs::withdrawlog(name user, uint64_t mid, uint64_t amount, asset quantity0, asset quantity1)
{
    require_auth(get_self());
}

void logs::donatelog(name user, uint64_t mid, uint64_t amount, name to)
{
	require_auth(get_self());
}