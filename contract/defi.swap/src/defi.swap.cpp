#include "../include/defi.swap.hpp"

void swap::newmarket(name creator, name contract0, name contract1, symbol sym0, symbol sym1)
{
	require_auth(creator);
	require_auth(get_self());
    check((contract0 != contract1) || (sym0 != sym1), "invalid pair");
    auto supply0 = utils::get_supply(contract0, sym0.code());
    check(supply0.amount > 0, "invalid token0");
    check(supply0.symbol == sym0, "invalid symbol0");
    auto supply1 = utils::get_supply(contract1, sym1.code());
    check(supply1.amount > 0, "invalid token1");
    check(supply1.symbol == sym1, "invalid symbol1");

    auto itr = _markets.begin();
    bool pair_exists = false;
    while (itr != _markets.end())
    {
        if (itr->contract0 == contract0 && itr->sym0 == sym0 && itr->contract1 == contract1 && itr->sym1 == sym1)
        {
            pair_exists = true;
            break;
        }
        if (itr->contract0 == contract1 && itr->sym0 == sym1 && itr->contract1 == contract0 && itr->sym1 == sym0)
        {
            pair_exists = true;
            break;
        }
        itr++;
    }
    check(!pair_exists, "market already exists");

    _markets.emplace(creator, [&](auto &a) {
        a.mid = get_mid();
        a.contract0 = contract0;
        a.contract1 = contract1;
        a.sym0 = sym0;
        a.sym1 = sym1;
        a.reserve0.symbol = sym0;
        a.reserve1.symbol = sym1;
        a.last_update = current_time_point();
    });
}

void swap::rmmarket(uint64_t mid)
{
    auto itr = _markets.require_find(mid, "Market does not exist.");
    check(itr->reserve0.amount == 0 && itr->reserve1.amount == 0 && itr->liquidity_token == 0, "Unable to remove active market.");
    _markets.erase(itr);
}

void swap::deposit(name user, uint64_t mid)
{
    require_auth(user);
    auto itr = _orders.find(user.value);
    check(itr == _orders.end(), "You have a pending order.");
    auto m_itr = _markets.require_find(mid, "Market does not exist.");
    _orders.emplace(user, [&](auto &a) {
        a.owner = user;
        a.mid = mid;
        a.quantity0.symbol = m_itr->sym0;
        a.quantity1.symbol = m_itr->sym1;
    });
}

void swap::cancle(name user)
{
    require_auth(user);
    auto itr = _orders.require_find(user.value, "You don't have any order.");
    auto m_itr = _markets.require_find(itr->mid, "Market does not exist.");
    if (itr->quantity0.amount > 0)
        utils::inline_transfer(m_itr->contract0, get_self(), user, itr->quantity0, std::string("cancle order refund"));
    if (itr->quantity1.amount > 0)
        utils::inline_transfer(m_itr->contract1, get_self(), user, itr->quantity1, std::string("cancle order refund"));
    _orders.erase(itr);
}

void swap::withdraw(name user, uint64_t mid, uint64_t amount)
{
    require_auth(user);
    auto m_itr = _markets.require_find(mid, "Market does not exist.");
    uint64_t reserve0 = m_itr->reserve0.amount;
    uint64_t reserve1 = m_itr->reserve1.amount;
    uint64_t amount0 = 1.0 * amount / m_itr->liquidity_token * reserve0;
    uint64_t amount1 = 1.0 * amount / m_itr->liquidity_token * reserve1;
    check(amount0 > 0 && amount1 > 0, "INSUFFICIENT_LIQUIDITY_BURNED");
    burn_liquidity_token(mid, user, amount);
    utils::inline_transfer(m_itr->contract0, get_self(), user, asset(amount0, m_itr->sym0), std::string("withdraw token0 liquidity"));
    utils::inline_transfer(m_itr->contract1, get_self(), user, asset(amount1, m_itr->sym1), std::string("withdraw token1 liquidity"));
    update(mid, reserve0 - amount0, reserve1 - amount1, reserve0, reserve1);
    // event log
    action(permission_level{LOG_ACCOUNT, "active"_n}, LOG_ACCOUNT, name("withdrawlog"),
           std::make_tuple(user, mid, amount, asset(amount0, m_itr->sym0), asset(amount1, m_itr->sym1)))
        .send();
}

void swap::donate(name user, uint64_t mid, uint64_t amount, name to)
{
	check(has_auth(user) || has_auth(get_self()), "no permission");

	auto m_itr = _markets.require_find(mid, "Market does not exist.");
	liquidity_index liqtable(get_self(), mid);

	//sub
	auto liq_itr = liqtable.require_find(user.value, "User liquidity does not exist.");
	check(liq_itr->token >= amount, "Invalid token amount.");
	check(liq_itr->token > 0, "Liquidity token is zero.");
	liqtable.modify(liq_itr, same_payer, [&](auto &a) {
		a.token -= amount;
	});
	if (liq_itr->token == 0)
	{
		liqtable.erase(liq_itr);
	}

	//add
	auto liq_to_itr = liqtable.find(to.value);
	if (liq_to_itr == liqtable.end())
	{
		liqtable.emplace(get_self(), [&](auto &a) {
			a.owner = to;
			a.token = amount;
		});
	}
	else
	{
		liqtable.modify(liq_to_itr, same_payer, [&](auto &a) {
			a.token += amount;
		});
	}

	// event log
	action(permission_level{LOG_ACCOUNT, "active"_n}, LOG_ACCOUNT, name("donatelog"),
			std::make_tuple(user, mid, amount, to))
			.send();
}

void swap::handle_transfer(name from, name to, asset quantity, std::string memo)
{
    if (from == _self || to != _self)
        return;
    std::vector<std::string> strs = utils::split(memo, ":");
    std::string act = strs[0];
    name code = get_first_receiver();

    if (act == "deposit")
    {
        do_deposit(from, quantity, code);
    }
    else if (act == "swap")
    {
        std::vector<std::string> paths = utils::split(strs[1], "-");
        uint64_t min_out = std::stoull(strs[2].c_str());
        extended_asset quantity_out_ex = extended_asset(quantity, code);
        for (int i = 0; i < paths.size(); i++)
        {
            uint64_t mid = std::stoull(paths[i].c_str());
            quantity_out_ex = do_swap(mid, from, quantity_out_ex.quantity, quantity_out_ex.contract, memo);
        }
        check(quantity_out_ex.quantity.amount >= min_out, "INSUFFICIENT_OUTPUT_AMOUNT");
        utils::inline_transfer(quantity_out_ex.contract, get_self(), from, quantity_out_ex.quantity, std::string("swap success"));
    }
    else
    {
        check(false, "invalid memo");
    }
}

void swap::do_deposit(name from, asset quantity, name code)
{
    auto itr = _orders.require_find(from.value, "You don't have any order.");
    auto m_itr = _markets.require_find(itr->mid, "Market does not exist.");
    check((code == m_itr->contract0 && quantity.symbol == m_itr->sym0) ||
              (code == m_itr->contract1 && quantity.symbol == m_itr->sym1),
          "Invalid deposit.");
    _orders.modify(itr, same_payer, [&](auto &s) {
        if (code == m_itr->contract0 && quantity.symbol == m_itr->sym0)
            s.quantity0 += quantity;
        else if (code == m_itr->contract1 && quantity.symbol == m_itr->sym1)
            s.quantity1 += quantity;
    });
    if (itr->quantity0.amount > 0 && itr->quantity1.amount > 0)
        add_liquidity(itr->owner);
}

void swap::add_liquidity(name user)
{
    auto itr = _orders.require_find(user.value, "You don't have any order.");
    auto m_itr = _markets.require_find(itr->mid, "Market does not exist.");

    // step1: get amount0 and amount1
    uint64_t amount0_desired = itr->quantity0.amount;
    uint64_t amount1_desired = itr->quantity1.amount;
    uint64_t amount0 = 0;
    uint64_t amount1 = 0;
    uint64_t reserve0 = m_itr->reserve0.amount;
    uint64_t reserve1 = m_itr->reserve1.amount;
    uint64_t refund_amount0 = 0;
    uint64_t refund_amount1 = 0;
    if (reserve0 == 0 && reserve1 == 0)
    {
        amount0 = amount0_desired;
        amount1 = amount1_desired;
    }
    else
    {
        uint64_t amount1_optimal = quote(amount0_desired, reserve0, reserve1);
        if (amount1_optimal <= amount1_desired)
        {
            amount0 = amount0_desired;
            amount1 = amount1_optimal;
            refund_amount1 = amount1_desired - amount1_optimal;
        }
        else
        {
            uint64_t amount0_optimal = quote(amount1_desired, reserve1, reserve0);
            check(amount0_optimal <= amount0_desired, "math error");
            amount0 = amount0_optimal;
            amount1 = amount1_desired;
            refund_amount0 = amount0_desired - amount0_optimal;
        }
    }

    // step2: refund
    if (refund_amount0 > 0)
        utils::inline_transfer(m_itr->contract0, get_self(), user, asset(refund_amount0, m_itr->sym0), std::string("extra deposit refund"));
    if (refund_amount1 > 0)
        utils::inline_transfer(m_itr->contract1, get_self(), user, asset(refund_amount1, m_itr->sym1), std::string("extra deposit refund"));

    //  step3: mint liquidity token
    uint64_t token_mint = 0;
    uint64_t total_liquidity_token = m_itr->liquidity_token;
    if (total_liquidity_token == 0)
    {
        token_mint = sqrt(amount0 * amount1) - MINIMUM_LIQUIDITY;
        mint_liquidity_token(itr->mid, get_self(), MINIMUM_LIQUIDITY); // permanently lock the first MINIMUM_LIQUIDITY tokens
    }
    else
    {
        token_mint = std::min(1.0 * amount0 / reserve0 * total_liquidity_token, 1.0 * amount1 / reserve1 * total_liquidity_token);
    }
    check(token_mint > 0, "INSUFFICIENT_LIQUIDITY_MINTED");
    // step4: update user liquidity token
    mint_liquidity_token(itr->mid, user, token_mint);
    // step5: update market
    update(itr->mid, reserve0 + amount0, reserve1 + amount1, reserve0, reserve1);
    // step6: finish deposit, remove order
    _orders.erase(itr);

    // event log
    action(permission_level{LOG_ACCOUNT, "active"_n}, LOG_ACCOUNT, name("depositlog"),
           std::make_tuple(user, m_itr->mid, asset(amount0, m_itr->sym0), asset(amount1, m_itr->sym1)))
        .send();
}

void swap::mint_liquidity_token(uint64_t mid, name to, uint64_t amount)
{
    liquidity_index liqtable(get_self(), mid);
    auto liq_itr = liqtable.find(to.value);
    if (liq_itr == liqtable.end())
    {
        liqtable.emplace(get_self(), [&](auto &a) {
            a.owner = to;
            a.token = amount;
        });
    }
    else
    {
        liqtable.modify(liq_itr, same_payer, [&](auto &a) {
            a.token += amount;
        });
    }
    auto m_itr = _markets.require_find(mid, "Market does not exist.");
    _markets.modify(m_itr, same_payer, [&](auto &a) {
        a.liquidity_token += amount;
    });
}

void swap::burn_liquidity_token(uint64_t mid, name to, uint64_t amount)
{
    auto m_itr = _markets.require_find(mid, "Market does not exist.");
    liquidity_index liqtable(get_self(), mid);
    auto liq_itr = liqtable.require_find(to.value, "User liquidity does not exist.");
    check(liq_itr->token >= amount, "Invalid token amount.");
    check(liq_itr->token > 0, "Liquidity token is zero.");
    liqtable.modify(liq_itr, same_payer, [&](auto &a) {
        a.token -= amount;
    });
    if (liq_itr->token == 0)
    {
        liqtable.erase(liq_itr);
    }
    _markets.modify(m_itr, same_payer, [&](auto &a) {
        a.liquidity_token -= amount;
    });
}

extended_asset swap::do_swap(uint64_t mid, name from, asset quantity, name code, std::string memo)
{
    auto m_itr = _markets.require_find(mid, "Market does not exist.");
    check((code == m_itr->contract0 || code == m_itr->contract1), "contract error");
    check((quantity.symbol == m_itr->sym0 || quantity.symbol == m_itr->sym1), "symbol error");
    uint64_t amount_in = quantity.amount;

    //TODO protocol fee
//    uint64_t protocol_fee = amount_in * PROTOCOL_FEE / 10000;
//    if (protocol_fee > 0)
//    {
//        amount_in -= protocol_fee;
//        utils::inline_transfer(code, get_self(), PROTOCOL_FEE_ACCOUNT, asset(protocol_fee, quantity.symbol), std::string("swap protocol fee"));
//    }

    uint64_t reserve0 = m_itr->reserve0.amount;
    uint64_t reserve1 = m_itr->reserve1.amount;
    uint64_t amount_out = 0;
    asset quantity_out;
    extended_asset quantity_out_ex;
    if (code == m_itr->contract0 && quantity.symbol == m_itr->sym0)
    {
        amount_out = get_amount_out(amount_in, reserve0, reserve1);
        check(amount_out < reserve1, "invalid amount_out1");
        quantity_out = asset(amount_out, m_itr->sym1);
        quantity_out_ex = extended_asset(quantity_out, m_itr->contract1);
        update(mid, reserve0 + amount_in, reserve1 - amount_out, reserve0, reserve1);
		swap_statistics(mid, asset(amount_in, m_itr->sym0), asset(0, m_itr->sym1));
    }
    else
    {
        amount_out = get_amount_out(amount_in, reserve1, reserve0);
        check(amount_out < reserve0, "invalid amount_out0");
        quantity_out = asset(amount_out, m_itr->sym0);
        quantity_out_ex = extended_asset(quantity_out, m_itr->contract0);
        update(mid, reserve0 - amount_out, reserve1 + amount_in, reserve0, reserve1);
		swap_statistics(mid, asset(0, m_itr->sym0), asset(amount_in, m_itr->sym1));
    }

    // TODO mining
    // asset eos_trade = asset(0, symbol("EOS", 4));
    // if (quantity_out_ex.contract == name("eosio.token") && quantity_out_ex.quantity.symbol == symbol("EOS", 4))
    // {
    //     eos_trade.amount = quantity_out_ex.quantity.amount;
    // }
    // if (code == name("eosio.token") && quantity.symbol == symbol("EOS", 4))
    // {
    //     eos_trade.amount = quantity.amount;
    // }
    // if (eos_trade.amount >= 10000)
    // {
    //     action(permission_level{MINE_ACCOUNT, "active"_n}, MINE_ACCOUNT, name("mine"), std::make_tuple(from, eos_trade)).send();
    // }

    // event log
    action(permission_level{LOG_ACCOUNT, "active"_n}, LOG_ACCOUNT, name("swaplog"),
           std::make_tuple(from, mid, quantity, quantity_out, memo))
        .send();

    return quantity_out_ex;
}

void swap::update(uint64_t mid, uint64_t balance0, uint64_t balance1, uint64_t reserve0, uint64_t reserve1)
{
    check(balance0 > 0, "Update balance0 error");
    check(balance1 > 0, "Update balance1 error");
    auto m_itr = _markets.require_find(mid, "Market does not exist.");
    auto last_sec = m_itr->last_update.sec_since_epoch();
    uint64_t time_elapsed = 1;
    if (last_sec > 0)
        time_elapsed = current_time_point().sec_since_epoch() - last_sec;
    _markets.modify(m_itr, same_payer, [&](auto &a) {
        a.reserve0.amount = balance0;
        a.reserve1.amount = balance1;
        if (time_elapsed > 0 && reserve0 != 0 && reserve1 != 0)
        {
            double price0 = 1.0 * PRICE_BASE / reserve0 * reserve1;
            double price1 = 1.0 * PRICE_BASE / reserve1 * reserve0;
            a.price0_cumulative_last += price0 * time_elapsed;
            a.price1_cumulative_last += price1 * time_elapsed;
            a.price0_last = (double)price0 / PRICE_BASE;
            a.price1_last = (double)price1 / PRICE_BASE;
        }
        a.last_update = current_time_point();
    });
}

uint64_t swap::get_mid()
{
    globals glb = _globals.get_or_default(globals{});
    glb.market_id += 1;
    _globals.set(glb, _self);
    return glb.market_id;
}

// given some amount of an asset and pair reserves, returns an equivalent amount of the other asset
uint64_t swap::quote(uint64_t amount0, uint64_t reserve0, uint64_t reserve1)
{
    check(amount0 > 0, "INSUFFICIENT_AMOUNT0");
    check(reserve0 > 0 && reserve1 > 0, "INSUFFICIENT_LIQUIDITY");
    uint64_t result = 1.0 * amount0 / reserve0 * reserve1;
    return result;
}

uint64_t swap::get_amount_out(uint64_t amount_in, uint64_t reserve_in, uint64_t reserve_out)
{
    check(amount_in > 0, "invalid input amount");
    check(reserve_in > 0 && reserve_out > 0, "insufficient liquidity");
    uint64_t amount_in_with_fee = amount_in * (10000 - TRADE_FEE);
//    uint64_t numerator = amount_in_with_fee * reserve_out;
    uint64_t denominator = reserve_in * 10000 + amount_in_with_fee;
    uint64_t amount_out = 1.0 * reserve_out / denominator * amount_in_with_fee;
    check(amount_out > 0, "invalid output amount");
    return amount_out;
}

void swap::test(uint64_t amount_in, uint64_t reserve_in, uint64_t reserve_out)
{
	auto amount_out = get_amount_out(amount_in, reserve_in, reserve_out);
	std::string msg = std::to_string(amount_out);
	check(false, msg.c_str());
}

void swap::swap_statistics(const uint64_t mid, const asset& token0, const asset& token1)
{
	//statistics
	auto now = current_time_point().sec_since_epoch();
	auto day = (now - START_STAT_DATE) / DAY_SECONDS;
	asset token0_fee = token0 * TRADE_FEE / 10000;
	asset token1_fee = token1 * TRADE_FEE / 10000;

	swap_stats stattable(get_self(), mid);
	auto stat_itr = stattable.find(day);
	if(stat_itr == stattable.end())
	{
		stattable.emplace(get_self(), [&](auto &s) {
			s.day = day;
			s.last_update = current_time_point();
			s.total_amount_0 = token0;
			s.total_amount_1 = token1;
			s.total_fee_0 = token0_fee;
			s.total_fee_1 = token1_fee;

			if(token0.amount>0)
			{
				s.total_count_0 = 1;
			}
			else
			{
				s.total_count_1 = 1;
			}
		});
	}
	else
	{
		stattable.modify(stat_itr, same_payer, [&](auto &s) {
			s.last_update = current_time_point();
			s.total_amount_0 += token0;
			s.total_amount_1 += token1;
			s.total_fee_0 += token0_fee;
			s.total_fee_1 += token1_fee;

			if(token0.amount>0)
			{
				s.total_count_0 += 1;
			}
			else
			{
				s.total_count_1 += 1;
			}
		});
	}
}