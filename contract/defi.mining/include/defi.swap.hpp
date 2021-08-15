#include <utils.hpp>
#include <math.h>

class [[eosio::contract("defi.swap")]] swap : public contract
{
public:
   using contract::contract;

   swap(name receiver, name code, datastream<const char *> ds)
       : contract(receiver, code, ds),
         _markets(_self, _self.value),
         _orders(_self, _self.value),
         _globals(_self, _self.value) {}

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

   struct [[eosio::table]] liquidity
   {
      name owner;
      uint64_t token;

      uint64_t primary_key() const { return owner.value; }
      EOSLIB_SERIALIZE(liquidity, (owner)(token))
   };

   typedef multi_index<"markets"_n, market> markets;
   markets _markets;
   typedef multi_index<"liquidity"_n, liquidity> liquidity_index;
};