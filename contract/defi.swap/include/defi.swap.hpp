#include "utils.hpp"
#include <math.h>

class [[eosio::contract("defi.swap")]] swap : public contract
{
public:
   using contract::contract;

   static constexpr uint64_t TRADE_FEE = 100; // 1%
   static constexpr uint64_t MINIMUM_LIQUIDITY = 10000;// 最小交易额
   static constexpr uint64_t PRICE_BASE = 10000;     //最小价格
   static constexpr uint64_t DAY_SECONDS = 24 * 60 * 60; //一天有多少秒
   static constexpr uint64_t START_STAT_DATE = 1607385600; //2020-12-08 08:00:00

   static constexpr name LOG_ACCOUNT{"log.defi"_n};
   static constexpr name MINE_ACCOUNT{""_n};     //挖矿用户
   static constexpr uint64_t PROTOCOL_FEE = 10; // 0.1%
   static constexpr name PROTOCOL_FEE_ACCOUNT{""_n};

   swap(name receiver, name code, datastream<const char *> ds)
       : contract(receiver, code, ds),
         _markets(_self, _self.value),
         _orders(_self, _self.value),
         _globals(_self, _self.value) {}

   /**
    * newmarket action.
    *
    * @details Allows any user to create a token exchange market .
 
    * @param contract0 - tokenA contract address, 代币1合约
    * @param contract1 - tokenB contract address, 代币2合约
    * @param sym0 - tokenA symbol,  代币1
    * @param sym1 - tokenB symbol.  代币2
    *
    * @pre contract0 has to be valid,
    * @pre contract1  has to be valid,
    * @pre sym0 has to be valid,
    * @pre sym1 has to be valid.
    *
    * If validation is successful a new exchange market for tokenA and tokenB gets created.
    */
   [[eosio::action]]
   void newmarket(name creator, name contract0, name contract1, symbol sym0, symbol sym1);

   [[eosio::action]]
   void rmmarket(uint64_t mid);
   [[eosio::action]]
   void deposit(name user, uint64_t mid);
   [[eosio::action]]
   void cancle(name user);
   [[eosio::action]]
   void withdraw(name user, uint64_t mid, uint64_t amount);
   [[eosio::on_notify("*::transfer")]]
   void handle_transfer(name from, name to, asset quantity, std::string memo);

   [[eosio::action]]
   void test(uint64_t amount_in, uint64_t reserve_in, uint64_t reserve_out);

   [[eosio::action]]
   void donate(name user, uint64_t mid, uint64_t amount, name to);
private:

   struct [[eosio::table]] market
   {
      uint64_t mid;

      name contract0;
      name contract1;
      symbol sym0;
      symbol sym1;
      asset reserve0;
      asset reserve1;
      uint64_t liquidity_token;
      double price0_last; // the last price is easy to controll, just for kline display, don't use in other dapp directly
      double price1_last;
      uint64_t price0_cumulative_last;
      uint64_t price1_cumulative_last;
      time_point_sec last_update;

      uint64_t primary_key() const { return mid; }
      EOSLIB_SERIALIZE(market, (mid)(contract0)(contract1)(sym0)(sym1)(reserve0)(reserve1)(liquidity_token)(price0_last)(price1_last)(price0_cumulative_last)(price1_cumulative_last)(last_update))
   };

   struct [[eosio::table]] order
   {
      name owner;
      uint64_t mid;
      asset quantity0;
      asset quantity1;

      uint64_t primary_key() const { return owner.value; }
      EOSLIB_SERIALIZE(order, (owner)(mid)(quantity0)(quantity1))
   };

   struct [[eosio::table]] liquidity
   {
      name owner;
      uint64_t token;

      uint64_t primary_key() const { return owner.value; }
      EOSLIB_SERIALIZE(liquidity, (owner)(token))
   };

   struct [[eosio::table]] globals
   {
      uint64_t market_id;
      EOSLIB_SERIALIZE(globals, (market_id))
   };

	struct [[eosio::table]] swap_stat
	{
		uint64_t day;

		asset total_amount_0;
		asset total_amount_1;

		asset total_fee_0;
		asset total_fee_1;

		uint32_t total_count_0;
		uint32_t total_count_1;

		time_point_sec last_update;

		uint64_t primary_key() const { return day; }
		EOSLIB_SERIALIZE(swap_stat, (day)(total_amount_0)(total_amount_1)(total_fee_0)(total_fee_1)(total_count_0)(total_count_1)(last_update))
	};

   typedef multi_index<"markets"_n, market> markets;
   markets _markets;
   typedef multi_index<"orders"_n, order> orders;
   orders _orders;
   typedef eosio::singleton<"globals"_n, globals> globals_index;
   globals_index _globals;
   typedef multi_index<"liquidity"_n, liquidity> liquidity_index;
   typedef multi_index<"swapstat"_n, swap_stat> swap_stats;

   void do_deposit(name from, asset quantity, name code);
   void add_liquidity(name user);
   void mint_liquidity_token(uint64_t mid, name to, uint64_t amount);
   void burn_liquidity_token(uint64_t mid, name to, uint64_t amount);
   extended_asset do_swap(uint64_t mid, name from, asset quantity, name code, std::string memo);
   void update(uint64_t mid, uint64_t balance0, uint64_t balance1, uint64_t reserve0, uint64_t reserve1);
   uint64_t get_mid();
   uint64_t quote(uint64_t amount0, uint64_t reserve0, uint64_t reserve1);
   uint64_t get_amount_out(uint64_t amount_in, uint64_t reserve_in, uint64_t reserve_out);
   void swap_statistics(const uint64_t mid, const asset& token0, const asset& token1);
};