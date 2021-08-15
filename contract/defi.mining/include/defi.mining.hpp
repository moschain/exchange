#pragma once

#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>
#include <eosio/system.hpp>

#include <string>
#include <cmath>

using std::string;
using namespace eosio;

class [[eosio::contract("defi.mining")]] mining : public contract
{
public:
   using contract::contract;

   [[eosio::action]]
   void start(const uint64_t &date);

   [[eosio::action]] 
   void update(const uint64_t &date);

private:
   struct [[eosio::table]] account
   {
      asset balance;

      uint64_t primary_key() const { return balance.symbol.code().raw(); }
   };

   struct [[eosio::table]] currency_stats
   {
      asset supply;
      asset max_supply;
      name issuer;

      uint64_t primary_key() const { return supply.symbol.code().raw(); }
   };

   typedef eosio::multi_index<"accounts"_n, account> accounts;
   typedef eosio::multi_index<"stat"_n, currency_stats> stats;
};