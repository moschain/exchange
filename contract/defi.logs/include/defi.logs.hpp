#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>
using namespace eosio;
using namespace std;

class [[eosio::contract("defi.logs")]] logs : public contract
{
public:
   using contract::contract;

   [[eosio::action]]
   void swaplog(name user, uint64_t mid, asset amountIn, asset amountOut, string memo);
   [[eosio::action]]
   void depositlog(name user, uint64_t mid, asset quantity0, asset quantity1);
   [[eosio::action]]
   void withdrawlog(name user, uint64_t mid, uint64_t amount, asset quantity0, asset quantity1);
   [[eosio::action]]
   void donatelog(name user, uint64_t mid, uint64_t amount, name to);
};