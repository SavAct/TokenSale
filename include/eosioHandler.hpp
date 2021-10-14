#pragma once

#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>
#include <eosio/symbol.hpp>
#include <eosio/crypto.hpp>

using namespace eosio;
    
#define defined_system_contract "eosio"

class EosioHandler 
{
public:
    // Structs from eosio.bios.hpp
    struct key_weight {
        public_key 	key;
        uint16_t 	weight;
        EOSLIB_SERIALIZE( key_weight, (key)(weight) )
    };
    struct wait_weight {
        uint32_t	wait_sec;
        uint16_t	weight;
        EOSLIB_SERIALIZE( wait_weight, (wait_sec)(weight) )
    };
    struct permission_level_weight {
        permission_level  permission;
        uint16_t          weight;
        EOSLIB_SERIALIZE( permission_level_weight, (permission)(weight) )
    };
    struct authority {
        uint32_t                              threshold = 0;
        std::vector<key_weight>               keys;
        std::vector<permission_level_weight>  accounts;
        std::vector<wait_weight>              waits;
        EOSLIB_SERIALIZE( authority, (threshold)(keys)(accounts)(waits) )
    };

    /** Deduced from eosio.system/src/From delegate_bandwidth.cpp function buyrambytes
    *   This function calculate the price for the bytes of RAM
    *   @param bytes    Amount of bytes of the RAM
    *   @returns        Cost of RAM, including the fee
    */
    static int64_t calcRamPrice(int32_t bytes);

    /** Stake NET and CPU for an account
    *	@param self		    Executing account
	*	@param account		Name of the new account
	*	@param stake_net	Public key of the new account
	*/
    static void createAccount(name self, name account, public_key& pubkey);
    
	/** Stake NET and CPU for an account
    *	@param self		    Executing account
	*	@param account		Account which gets the staked resources
	*	@param stake_net	Amount of system token to stake for NET
	*	@param stake_cpu 	Amount of system token to stake for CPU
	*/
	static void delegatebw(name self, name account, asset stake_net, asset stake_cpu);

    /** Transfer funds of system token
	*	@param from		Name of the sender 
    *	@param to		Name of the recipient 
	*	@param funds	Amount of funds
	*	@param memo		Referring memo
	*/
	static void transfer(name from, name to, asset funds, const std::string& memo);

	/** Buy RAM by bytes
	*	@param payer		Account who pays in system token for the RAM 
	*	@param receiver		Receier of the RAM
	*	@param bytes		Amount of bytes which should be bought
	*/
	static void buyrambytes(name payer, name receiver, int32_t bytes);

private:
    // From eosio.system/src/exchange_state.cpp
    static int64_t get_bancor_input(int64_t out_reserve, int64_t inp_reserve, int64_t out);

    // From eosio.system/include/exchange_state.hpp
    /**
    * Uses Bancor math to create a 50/50 relay between two asset types.
    *
    * The state of the bancor exchange is entirely contained within this struct.
    * There are no external side effects associated with using this API.
    */
    struct [[eosio::table, eosio::contract(defined_system_contract)]] exchange_state {
        asset    supply;

        struct connector {
            asset balance;
            double weight = .5;
        };
        uint64_t primary_key()const { return supply.symbol.raw(); }

        connector base;
        connector quote;
    };
    typedef eosio::multi_index< "rammarket"_n, exchange_state > rammarket;

    static constexpr name eosio_system = name(defined_system_contract);
    static constexpr symbol ramcore_symbol = symbol(symbol_code("RAMCORE"), 4);
};


int64_t EosioHandler::calcRamPrice(int32_t bytes) {
    rammarket _rammarket(eosio_system, eosio_system.value);
    auto itr = _rammarket.find(ramcore_symbol.raw());
    const int64_t ram_reserve   = itr->base.balance.amount;
    const int64_t eos_reserve   = itr->quote.balance.amount;
    const int64_t cost          = get_bancor_input( ram_reserve, eos_reserve, bytes );
    return cost / double(0.995);            // plus fee
}

void EosioHandler::buyrambytes(name payer, name receiver, int32_t bytes){
	action {
	  permission_level{payer, "active"_n},
	  eosio_system,
	  "buyrambytes"_n,
	  std::make_tuple(payer, receiver, bytes)
	}.send();
}

void EosioHandler::transfer(name from, name to, asset funds, const std::string& memo)
{
	check(funds.amount > 0, "No system token amount for this user");
	
	action {
	  permission_level{from, "active"_n},
	  "eosio.token"_n,
	  "transfer"_n,
	  std::make_tuple(from, to, funds, memo)
	}.send();
}

void EosioHandler::delegatebw(name self, name account, asset stake_net, asset stake_cpu){
    action{
        permission_level{ self, "active"_n},
        eosio_system,
        "delegatebw"_n,
        std::make_tuple(self, account, stake_net, stake_cpu, true)
    }.send();
}

void EosioHandler::createAccount(name self, name account, public_key& pubkey){	
    // Create of the newAccount similar to this old contract: 				https://github.com/DeBankDeFi/signupeoseos/blob/master/signupeoseos.cpp

    key_weight pubkey_weight = key_weight{ pubkey, 1 };
    std::vector<key_weight> keys;
    keys.push_back(pubkey_weight);

    authority owner  = authority{ 1, keys, {}, {} };
    authority active = authority{ 1, keys, {}, {} };

    action{
        permission_level{self, "active"_n},
        eosio_system,
        "newaccount"_n,
        std::make_tuple(self, account, owner, active)
    }.send();
}

int64_t EosioHandler::get_bancor_input(int64_t out_reserve, int64_t inp_reserve, int64_t out) {
    const double ob = out_reserve;
    const double ib = inp_reserve;

    int64_t inp = (ib * out) / (ob - out);

    if ( inp < 0 ) inp = 0;

    return inp;
}