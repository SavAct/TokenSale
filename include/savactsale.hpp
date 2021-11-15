#include <eosio/eosio.hpp>
#include <eosio/crypto.hpp>
#include <eosio/asset.hpp>
#include <eosio/symbol.hpp>
#include <cmath>
#include <iterator>
#include <vector>
#include "globals.hpp"
#include "stringConversion.hpp"
#include "tokenCalculation.hpp"
#include "distribution.hpp"
#include "eosioHandler.hpp"

using namespace std;
using namespace eosio;

CONTRACT savactsale : public contract{

public:

	// Owner of the bougth token
	struct Owner
	{
		std::vector<char> key;
		int64_t amount;
		int64_t affiliate;

		static Owner createOwner(const ecc_public_key& pub_key, const int64_t amount, const int64_t affiSold)
		{
			Owner owner;
			owner.amount = amount;
			owner.affiliate = affiSold;
			owner.key = std::vector<char>(pub_key.begin(), &pub_key[PubKeyWithoutPrimarySize]);		
			return owner;
		}
	};

	/**
	 * Reference to a transaction on the blockchain composed of the block number and transaction id
	*/
	struct Ref{
		uint64_t Block;
		checksum256 TrxId;
	};

private:
	#define ram_amountlist_entry 163
	#define ram_amountlist_entry_without_primary 42
	#define ram_affiList_entry 129
	#define ram_open_contracttoken 240

	#define minSystemTokenAmount 1000

	// Table to store public keys with the corresponding amount of purchased token
	TABLE amountlist {
		uint64_t id;				// primary key calculated with the public key
		std::list<Owner> owner;
		auto primary_key() const { return id; }
	};
	typedef multi_index<name("purchased"), amountlist> amountlist_table;

	// Table to store affiliates by user name
	TABLE affiList {
		name account;
		int64_t amount;
		bool convinced;				// Get extra accredit in contract token or payed token
		auto primary_key() const { return account.value; }
	};
	typedef multi_index<name("nameaffis"), affiList> affis_table;

	/**
	 * Table to store a savweb files
	*/
	TABLE static_index_table {
		uint64_t      key;        // Primary key
		string        fname;      // File name with name extention
		vector<Ref>   refs;       // ref[0] contains reffered trasnaction of the first transaction. If the file is portioned there is a second entry ref[1] with a reference to the last entry  
		string        attri;      // Attributes / optional moreover stuff
		
		auto primary_key() const { return key; }
	};
	typedef multi_index<"index"_n, static_index_table> index_table;

	// This struct is a copy from globals.hpp and has to be placed here again to show this table by the nodes
	TABLE statusstruct {
		uint64_t id;
		uint32_t start;
		bool started;
		bool frozen;
		bool release;
		int64_t sold;
		eosio::asset total;
		eosio::asset endprice;
		int64_t sp;
		std::vector<globals::Star> stars;
		auto primary_key() const { return id; }
	};
	typedef multi_index<"stat"_n, statusstruct> status_table;

	// Contains the global status like already sold token
	globals globalStatus;

	// Recipients of the funding which can be chosen by the contributor
	name donationAccounts[6] = {
		name("savactsavact"),
		name("savactsavpay"),
		name("savactvoting"),
		name("savactsavweb"),
		name("savactwallet")
	};

	struct [[eosio::table, eosio::contract(defined_token_contract)]] token_accounts {
        asset balance;
		uint64_t primary_key()const { return balance.symbol.code().raw(); }
    };
    typedef eosio::multi_index< "accounts"_n, token_accounts > tokenAcc_table;


public:
	using contract::contract;
	
	savactsale(name account, name code, datastream<const char*> ds) : contract(account, code, ds), globalStatus(get_self())
	{ }

	[[eosio::on_notify("eosio.token::transfer")]]
	void deposit(name from, name to, asset fund, string memo) {
		globalStatus.checkFrozen();
		globalStatus.checkTime();
		donate(from, to, fund, memo);
	}

	/** Initialize the contract by setting a start time and unfreeze the contract   
	 * @param startime Unix time stamp. From that time on payments are accepted  
	 */
	ACTION createsale(uint32_t starttime) {
		require_auth(get_self());
		globalStatus.init(starttime);
	}

	/** Set pages for the sale
	 * @param key   Primary key of the entry. key == 0 for the landing page
	 * @param refs  Reference to the landing page file: ref[0] contains referred transaction of the first transaction. If the file is portioned there is a second entry ref[1] with a reference to the last entry
	 * @param attri Attributes / optional moreover stuff
	 * @param fname File name with name extension
	 */
	ACTION setpage(uint64_t key, vector<Ref>& refs, string& attri, string& fname) {
		require_auth(get_self());

		check(refs.size() > 0, "No ref mentioned.");

		// Init the table
		index_table _indexTable(get_self(), get_self().value);

		// Find the entry
		auto itr = _indexTable.find(key);

		if(itr == _indexTable.end()){
			// Add to table
			_indexTable.emplace(get_self(), [&](auto& p) {
				p.key = key;
				p.attri = attri;
				p.fname = fname;
				p.refs = refs;
			});
		} else {
			// Modify table
			_indexTable.modify(itr, eosio::same_payer, [&](auto& p) {
				p.attri = attri;
				p.fname = fname;
				p.refs = refs;
			});
		}
	}

	/** Pay off contract tokens to the account name
	*	@param currenttime	The current unix timestamp
	*	@param tokenowner	Account name which should get the contract tokens
	* 	@param sig			Signature of the string "{contract account name} {currenttime} {tokenowner}" (without quote signs or curly braces)
	* 	@param pubkey		Public key of the token owner
	*	@param rampayer		Account name which pays the RAM
	*/
	ACTION payoff(uint32_t currenttime, name tokenowner, const signature& sig, const public_key& pubkey, name rampayer) {
		check(is_account(tokenowner), "Can't pay off to this name. It doesn't exists.");
		check(eosio::current_time_point().sec_since_epoch() - currenttime < 600, "Mentioned time is older than 10 min.");
		globalStatus.checkFrozen();

		check(pubkey.index() == 0, "This public key format is not in use.");
		ecc_public_key public_key = std::get<0>(pubkey);

		// Check the signature with the token owner
		string checkString =  get_self().to_string() + std::string(" ") + std::to_string(currenttime) + std::string(" ") + tokenowner.to_string();
		calc::ecVerify(checkString, sig, pubkey);

		// Get token amount
		amountlist_table _amountlist(get_self(), get_self().value);
		auto entry = _amountlist.find(getIntFromPubKey(public_key));
		check(entry != _amountlist.end(), "The mentioned entry is not given.");
		auto ownerItr = findOwner(entry, public_key);
		check(ownerItr != entry->owner.end(), "The mentioned owner is not given.");

		// Delete owner entry 
		deleteOwner(_amountlist, entry, ownerItr);

		// Send to token owner
		transferContractToken(tokenowner, ownerItr->amount, "Congrats! You are part of SavAct.", rampayer);
	}

	/** Change the distribution model for an affiliate account name
	*	@param affiliate	Name of the affiliate
	* 	@param convinced	Status if the affiliate wants 100% contract token or a bit in system token
	*/
	ACTION setconvinced(name affiliate, bool convinced) {
		require_auth(affiliate);
		globalStatus.checkFrozen();

		affis_table affiTable(get_self(), get_self().value);
		auto itr = affiTable.find(affiliate.value);

		check(itr != affiTable.end(), "This affiliate name does not exist. It requires at least one client of this affiliate.");
		
		affiTable.modify(itr, get_self(), [&](auto& item) {
			check(item.convinced != convinced, "This value was already setted.");
			item.convinced = convinced;
		});
	}

	// Clear all tables, including the global status
	ACTION clearall() {
	 	require_auth(get_self());

	 	amountlist_table _amountlist(get_self(), get_self().value);
		affis_table _affi_Table(get_self(), get_self().value);
		index_table _indexTable(get_self(), get_self().value);

		// Delete all ref pages
		auto itr0 = _indexTable.begin();
		while (itr0 != _indexTable.end()) {
			itr0 = _indexTable.erase(itr0);
		}

		// Delete all records in amounts table
		auto itr1 = _amountlist.begin();
		while (itr1 != _amountlist.end()) {
			itr1 = _amountlist.erase(itr1);
		}

		// Delete all records in affiliate name table
		auto itr2 = _affi_Table.begin();
		while (itr2 != _affi_Table.end()) {
			itr2 = _affi_Table.erase(itr2);
		}

		// Delete global status
		globalStatus.deleteWholeStatus();
	}

private:

	/** Donate an amount of system token and get contract token
	*	@param from	Sender of the system token
	* 	@param to	Receiver of the system token (This contract)
	* 	@param fund	Asset of the system token
	* 	@param memo	Parameters which contains the recipients of the system and contract tokens
	*/
	void donate(name from, name to, asset& fund, string& memo);

	/** Donate an amount of system token and get contract token
	*	@param order			Converted parameters which contains the recipients of the system and contract tokens
	* 	@param user				
	* 	@param fund				Asset of the system token
	* 	@param contractToken	Amount of contract token for the user
	* 	@param ramUsage			Hold the amount of RAM which will be used
	*/
	void accreditUser(MemoParams& order, asset& fund, int64_t contractToken, int32_t& ramUsage);

	/** Donate an amount of system token and get contract token
	*	@param order	Converted parameters which contains the recipients of the system and contract tokens
	* 	@param fund		Asset of the system token
	* 	@param ramUsage	Hold the amount of RAM which will be used
	*	@return 		The amount of bought contract token		
	*/
	int64_t buyContractTokenAndAccreditAffiliate(MemoParams& order, asset& fund, int32_t& ramUsage);

	/** Get the entry iterator of a public key in the amounts list
	*	@param amountlistTB	Amounts list table
	* 	@param pub_key		Public key
	* 	@return 			Iterator to the entry in the amounts list
	*/
	static amountlist_table::const_iterator findEntry(amountlist_table& amountlistTB, ecc_public_key& pub_key){	
		uint64_t id(getIntFromPubKey(pub_key)); // Get the primary key of this public key
		return amountlistTB.find(id);
	}

	/** Keep the relation to an affiliate for a bought amount of contract token
	*	@param self			This contract account name
	*	@param account		Name of the affiliate
	*	@param affiTable	Reference to the affiliate table
	*	@param itr			Iterator to the name of the affiliate in the affiliate table
	*	@param amount		Amount of contract token which were credited to an other user through this affiliate
	*	@return 			Ram consumption
	*/
	static int32_t addToAffiNameTable(name self, name account, affis_table& affiTable, affis_table::const_iterator itr, const int64_t amount);

	/** Transfer funds of the purchased token
	*	@param to		Name of the recipient 
	*	@param amount	Amount of funds
	*	@param memo		Referring memo
	*	@return			Amount of needed RAM
	*/
	int32_t transferContractToken(name to, int64_t amount, const string& memo, name rampayer);

	/** Buy an account for the user and reduce the fund by its costs
	*	@param pubkey	Public key of the new account
	*	@param account	Name of the nuew account
	*	@param fund 	Available funds to creat an account
	*/
	void buyAccount(public_key& pubkey, name account, asset& fund);

	/** Converts the last 8 Bytes of an eos public key data to an integer.
	*	@param ecc_public_key	Public key
	*	@return 				Integeger as recast of the last 8 bytes
	*/
	static uint64_t getIntFromPubKey(const ecc_public_key& pub_key);

	/** Buy the amount of contract token which can be bought with the amount of payed token
	*	@param fund_amount	Sum of the system token which should be used for the calculation
	*	@return 			The amount of bought contract token
	*/
	int64_t buyContractToken(int64_t fund_amount);

	/** Buy the amount of contract token which can be bought with the amount of payed token and the added percentages
	*	@param sumPayed		Sum of the payed token for real
	* 	@param dist			Should contains the destribution percentages before and stores the destribution results after executing this function. 
	*/
	void GetToken(int64_t sumPayed, Distribution& dist);

	/** Stores the public key with a bought amount in the amounts table
	*	@param pub_key		Public key which owns the amount
	*	@param amount		Amount of token
	*	@param affiSold		Amount of token which were credited to other users through this user as affiliate.
	*	@return 			Ram consumption
	*/
	int32_t addToAmountTable(const ecc_public_key& pub_key, const int64_t amount, const int64_t affiSold = 0);

	/** Check that the user and affiliate are not the same
	*	@param order	Converted parameters which contains the recipients of the system and contract tokens
	*/
	static void checkUserIsNotAffiliate(MemoParams& order);

	/** Find the entry of the token owner in the table entry
	*	@param itr		Iterator to the table entry with the list of owners
	*	@param pubKey	Public key of the owner
	*	@return			List-iterator to the owner
	*/
	std::list<Owner>::const_iterator findOwner(amountlist_table::const_iterator itr, const ecc_public_key& pubKey);

	/** Delete the entry of a token owner. If the entry has no owners left, it will delete the whole table entry.
	*	@param amountlistTB		Table with the lists of owners
	*	@param itr				Iterator to the table entry with the list of owners
	*	@param ownerItr			Iterator to the owner
	*/
	void deleteOwner(amountlist_table& amountlistTB, amountlist_table::const_iterator itr, std::list<Owner>::const_iterator ownerItr);


	static constexpr int PubKeyWithoutPrimarySize = 33 - sizeof(uint64_t);
	
	// Parameters for account creation
	static constexpr int ramForUser = 4000;			// Bytes of RAM
	static constexpr int netCostForUser = 5000;		// amount in system token 
	static constexpr int cpuCostForUser = 10000;	// amount in system token
};

// Caslculation results for needed RAM:
//
// The following amount of bytes need to be provided by the contract.
// 241 (old) Status initialisation		[uint64][bool][int64] => new has [uint64][uint32][bool][bool][bool][int64][asset][asset][int64][Star[3]];
// 112 Create affiliate name tabele 	[name][int64][bool]
// 112 Create purchased key table 		[uint64][list[uint64][uint64][vector<char, 25>]]
// + 163 Bytes for temporary handlings
// ------------------
// 628 Bytes 

// 77 Payoff => Releasing 163 Bytes by removing the line and consumes 240 Bytes for opening an entry in token contract  

// The following amount on RAM will be purchased automatically by a user payment. It doesn't reduce the amount of contract token.
// 129 New affiliate name entry
// 163 New purchased table entry
//  42 New owner without a new primary key purchased table
// 240 Open an entry in eosio.token contract 
