#pragma once

#include <eosio/eosio.hpp>
#include <eosio/system.hpp>

using namespace eosio;

#define defined_token_contract "token.savact"

// Singletons cannot be shown by the nodes, therefore a multi index table is used
class globals
{
public:
	struct Star {
		int64_t amount;
		std::vector<char> user;	// contains the user name or the key
		int64_t mark;
		int64_t gain;
	};

	// Sale properties (Example)
	static constexpr uint8_t paymentPrecision = 4;
	static constexpr uint8_t contractPrecision = 4;
	static constexpr symbol  paymentSymbol 	= eosio::symbol("EOS", paymentPrecision);
	static constexpr symbol  contractSymbol = eosio::symbol("SAVACT", contractPrecision);
	static constexpr int64_t TotalToken = 2432000000000; 	// 320,000,000 SavAct * 76% = 243,200,000
	static constexpr double  StartPrice = 0.04; 			// EOS    		
	static constexpr double  EndPrice	= 0.10; 			// EOS
	static constexpr int64_t GiftForLastBuyer = 1000000;    // 100 SavAct // Should be more than EndPrice * minSystemTokenAmount to avoid unsaleable tokens
	static constexpr eosio::name TokenContractName = name(defined_token_contract);
	static constexpr int64_t minPayOnCreateAcc = 100000; 	// EOS

	static constexpr int StarAmount = 5;
	static constexpr int64_t Mark[StarAmount] { 2432000000, 24320000000, 243200000000, 486400000000, 2432000000000 };	// 0,1%, 1%, 10%, 20% and 100%
	static constexpr int64_t Gain[StarAmount] { 100000000, 1000000000, 5000000000, 10000000000, 20000000000 };		    // 10,000 SavAct, 100,000 SavAct, 500,000 SavAct, 1,000,000 SavAct and 2,000,000 SavAct

	// Whole sale value in EOS: ((EndPrice + StartPrice) * TotalToken / 2) - Gift

	globals(name self) : _status(self, self.value), _self(self)
	{
		// Create this initial record if it does not exist
		if (_status.begin() == _status.end())
		{
			_status.emplace(_self, [&](auto& entry) {
				entry.sold = 0;
				entry.frozen = true;
				entry.started = false;
				entry.release = false;
				entry.start = 0;
				entry.total = eosio::asset(TotalToken, contractSymbol);
				entry.endprice =  eosio::asset(EndPrice * powl(10, paymentPrecision), paymentSymbol);
				entry.sp = StartPrice * powl(10, paymentPrecision);
				
				for(int i = 0; i < StarAmount; i++){
					Star s;
					s.amount = 0;
					s.user = {'*','*','*','*','*','*','*','*','*','*','*','*','*','*','*','*','*','*','*','*','*','*','*','*','*','*','*','*'};	// fill the max space it will ever need
					s.gain = Gain[i];
					s.mark = Mark[i];
					entry.stars.push_back(s);
				}
			});
		}
	}

	// Initialize the start time
	void init(int32_t startTime);

	// Functions to freeze the contract:
	// get the frozen state of the contract
	bool isFrozen();
	// set the status of the contract to frozen or unfrozen
	void setFreezestatus(bool freezestatus);
	// abort the whole action if the contract is frozen
	void checkFrozen();
	// abort the whole action if the start time has not been reached
	void checkTime();

	// add to the current sold amount 
	void addSold(int64_t amount);
	// get the current sold amount 
	int64_t getSold();

	void deleteWholeStatus() {
		// Delete all records in status table
		auto itr = _status.begin();
		while (itr != _status.end()) {
			itr = _status.erase(itr);
		}
	}

private:
	const name _self;

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
	status_table _status;
};

void globals::addSold(int64_t amount)
{
	_status.modify(_status.begin(), _self, [&](auto& entry) {
		entry.sold += amount;
	});
}

int64_t globals::getSold()
{
	return _status.begin()->sold;
}

bool globals::isFrozen()
{
	return _status.begin()->frozen;
}

void globals::checkFrozen()
{
	eosio::check(!isFrozen(), "Contract is frozen!");
}

void globals::checkTime()
{
	if(_status.begin()->started) {
		return;
	}
	
	eosio::check(eosio::current_time_point().sec_since_epoch() >= _status.begin()->start, "Start time has not been reached yet!");

	_status.modify(_status.begin(), _self, [&](auto& entry) {
		entry.started = true;
	});
}

void globals::setFreezestatus(bool freezestatus)
{
	_status.modify(_status.begin(), _self, [&](auto& entry) {
		entry.frozen = freezestatus;
	});
}

void globals::init(int32_t startTime)
{
	_status.modify(_status.begin(), _self, [&](auto& entry) {
		entry.start = startTime;
		entry.started = false;
		entry.frozen = false;
		entry.release = false;
	});

	
}
