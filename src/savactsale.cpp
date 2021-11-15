#include <savactsale.hpp>

inline void savactsale::donate(name from, name to, asset& fund, string& memo) {
	if (from == get_self() || to != get_self())
		return;

	// Check input
	check(fund.symbol == globals::paymentSymbol, "Wrong token symbol");
	check(memo.length() > 0, "Empty Memo!");
	check(fund.is_valid(), "Invalid token transfer");
	check(fund.amount > 0, "Zero amount");
	check(fund.amount >= minSystemTokenAmount, "The minimum deposit amount is not reached.");

	// Check special accounts
	for (const eosio::name& donAcc : donationAccounts){
		check(donAcc != from, "Donation accounts can't participate.");
	}

	// Get all parameters of the memo
	MemoParams order(memo);

	// Check memo parameters
	check(order.user.size() > 0, "There was no user or public key defined.");

	// Checks for account cration
	if(order.user.size() > 1){
		check(order.user[0].index() != order.user[1].index(), "Two parameters have the same type. Use only one name and one public key.");
		check(globals::minPayOnCreateAcc <= fund.amount, "Deposit is not enough to get tokens and create an account at the same time.");
	}
	check(order.donationAccNumber >= 0 && order.donationAccNumber < std::size(donationAccounts), "This number referres to no donation account.");
	
	int64_t contractToken;		// Holds the amount of contract token for the user
	int32_t ramUsage(0);		// Holds the amount of ram for all table changes of this contract

	// Buy the amount of contract Token for the given fund and accredit affiliate if it exists
	contractToken = order.hasAffiliate? buyContractTokenAndAccreditAffiliate(order, fund, ramUsage) : buyContractToken(fund.amount);

	// Accredit user
	accreditUser(order, fund, contractToken, ramUsage);	

	// Buy ram
	if(ramUsage > 0) {
		fund.amount -= EosioHandler::calcRamPrice(ramUsage);
		check(fund.amount > 0, "Not enough token to buy necessary RAM");
		EosioHandler::buyrambytes(get_self(), get_self(), ramUsage);
	}

	// Send the system token to the mentioned donation account
	EosioHandler::transfer(get_self(), donationAccounts[order.donationAccNumber], fund, "Anonymous donation to accomplish your goals.");
}

int64_t savactsale::buyContractToken(int64_t fund_amount)
{
	// Calculate the amount of token which can be bought with the payed amount
	int64_t soldToken = globalStatus.getSold();
	int64_t boughtToken = calc::newSoldToken(fund_amount, soldToken);

	check(boughtToken != 0, "Order will be rejected: boughtToken == 0");
	check(boughtToken > 0, "Order will be rejected: boughtToken < 0");

	// Set the new soldToken
	globalStatus.addSold(boughtToken);

	return boughtToken;
}

void savactsale::checkUserIsNotAffiliate(MemoParams& order){
	std::visit([&](auto const& affi){
		using T = std::decay_t<decltype(affi)>;
		for(auto& u : order.user) {
			if(std::holds_alternative<T>(u)){
				check(affi != std::get<T>(u), "You can't enter yourself as affiliate.");
			}
		}
	}, order.affiliate);
}

int64_t savactsale::buyContractTokenAndAccreditAffiliate(MemoParams& order, asset& fund, int32_t& ramUsage) 
{
	std::list<Owner>::const_iterator itrAffi;
	Distribution dist;
	
	// Check that the user and affiliate are not the same			
	checkUserIsNotAffiliate(order);

	// Get distribution for affiliate from the right table
	if(std::holds_alternative<ecc_public_key>(order.affiliate)){
		// Get the amount which were bought though the affiliate and calculate its tier percentage
		ecc_public_key affiKey = std::get<ecc_public_key>(order.affiliate);
		amountlist_table _amountlist(get_self(), get_self().value);
		auto itr = findEntry(_amountlist, affiKey);
		if(itr != _amountlist.end()) {
			itrAffi = findOwner(itr, affiKey);
			if(itrAffi != itr->owner.end()) {
				dist.affiPercentage = Distribution::AffiPercentage::Calc(itrAffi->affiliate);
			}
		}

		// Buy the contract token and set the distribution
		GetToken(fund.amount, dist);

		// Add the amount for this affiliae and the amount of bought token via this affiliate
		ramUsage += addToAmountTable(std::get<ecc_public_key>(order.affiliate), dist.amount.contract.affiliate, dist.amount.contract.user);
	} else {
		name* affiName = &std::get<name>(order.affiliate);
		check(is_account(*affiName), "Affiliate account name does not exist.");

		// Get the amount which were bought though the affiliate and calculate its tier percentage
		affis_table affiTable(get_self(), get_self().value);
		auto itr = affiTable.find(affiName->value);
		if(itr != affiTable.end()){
			dist.affiPercentage = Distribution::AffiPercentage::Calc(itr->amount, itr->convinced);
		}

		// Buy the contract token and set the distribution
		GetToken(fund.amount, dist);

		// Add the amount of bought token via this affiliate
		ramUsage += addToAffiNameTable(get_self(), std::get<name>(order.affiliate), affiTable, itr, dist.amount.contract.user);

		if(dist.amount.contract.affiliate > 0) {
			// Transfer this amount
			ramUsage += transferContractToken(*affiName, dist.amount.contract.affiliate, "Thank you to be a part of SavAct.", get_self());
		}
	}

	// Set the credited system token for the affiliate
	if(dist.affiPercentage.system > 0) {
		asset affiPayment(fund.amount * dist.affiPercentage.system, globals::paymentSymbol);		// Get the asset of payment token for the affiliate
		if(affiPayment.amount > 0) {
			fund -= affiPayment;									// Calc the remaining payment token
			EosioHandler::transfer(get_self(), std::get<name>(order.affiliate), affiPayment, "Thank you to be a part of SavAct.");
		}
	}

	// return contract token for the user
	return dist.amount.contract.user;
}

void savactsale::accreditUser(MemoParams& order, asset& fund, int64_t contractToken, int32_t& ramUsage){
	// Get the first mentioned user
	auto* user = &(order.user[0]);
	
	// Accredit paying user for the case the user is definied by a public key
	if(order.user.size() == 1 && std::holds_alternative<ecc_public_key>(*user)) {	
		// Add to table
		ramUsage += addToAmountTable(std::get<ecc_public_key>(*user), contractToken);
	} else if (order.user.size() == 2){
		// Creat an account idependent of the parameter order
		ecc_public_key* pub_key;
		if(std::holds_alternative<name>(*user)){
			pub_key = &std::get<ecc_public_key>(order.user[1]);
		} else {
			user = &(order.user[1]);
			pub_key = &std::get<ecc_public_key>(order.user[0]);
		}
		
		public_key pubkey; 	
		pubkey.emplace<0>(*pub_key);						// ecc_public_key to public_key
		buyAccount(pubkey, std::get<name>(*user), fund);
	}
	
	// Transfer contract token to the user if an account name is defined
	if(std::holds_alternative<name>(*user)) {
		ramUsage += transferContractToken(std::get<name>(*user), contractToken, "Congrats! You are a part of SavAct.", get_self());
	}
}

void savactsale::buyAccount(public_key& pubkey, name account, asset& fund){
	check(!is_account(account), "The account name is already taken.");

	// remove the cost for a new account
	int64_t ramCostForUser = EosioHandler::calcRamPrice(ramForUser);
	fund.amount -= (netCostForUser + cpuCostForUser + ramCostForUser);
	check(fund.amount > 0, "Not enough amount to create an account");

	// Create the account
	EosioHandler::createAccount(get_self(), account, pubkey);

	// Stake NET and CPU for the account
	EosioHandler::delegatebw(get_self(), account, asset(netCostForUser, globals::paymentSymbol), asset(cpuCostForUser, globals::paymentSymbol));

	// Buy RAM for the account
	EosioHandler::buyrambytes(get_self(), account, ramForUser);
}

int32_t savactsale::addToAmountTable(const ecc_public_key& pub_key, const int64_t amount, const int64_t affiSold)
{
	// Get the primary key of this public key
	uint64_t id(getIntFromPubKey(pub_key));

	// Init the _amountlist table and find the entry of this id
	amountlist_table _amountlist(get_self(), get_self().value);
	auto itr = _amountlist.find(id);

	if(itr == _amountlist.end()) 
	{		
		Owner owner = Owner::createOwner(pub_key, amount, affiSold);

		// Create a new entry and add the owner to the record
		_amountlist.emplace(get_self(), [&](auto& item) {
			item.id = id;
			item.owner.push_back(owner);
		});
		return ram_amountlist_entry;
	}
	else {
		// Find the record of this owner
		auto ownerItr = findOwner(itr, pub_key);
		if(ownerItr == std::end(itr->owner))
		{
			Owner owner = Owner::createOwner(pub_key, amount, affiSold);
			
			// Add the owner to the record
			_amountlist.modify(itr, get_self(), [&](auto& item) {
				item.owner.push_back(owner);
			});

			return ram_amountlist_entry_without_primary;
		} else {
			// Edit the found user
			_amountlist.modify(itr, get_self(), [&](auto& item) 
			{
				auto itr = item.owner.erase(ownerItr, ownerItr); 	// empty erase to get the non const iterator
				itr->amount += amount;
				itr->affiliate += affiSold;
			});
			return 0;
		}
	}
}

int32_t savactsale::addToAffiNameTable(name self, name account, affis_table& affiTable, affis_table::const_iterator itr, const int64_t amount)
{
	if(itr == affiTable.end()) {
		// Create a new entry
		affiTable.emplace(self, [&](auto& item) {
			item.account = account;
			item.amount = amount;
			item.convinced = true;
		});
		return ram_affiList_entry;
	}
	else {
		// Edit existing entry
		affiTable.modify(itr, self, [&](auto& item) {
			item.amount += amount;
		});
		return 0;
	}
}

uint64_t savactsale::getIntFromPubKey(const ecc_public_key& pub_key){
	const char* bytes = &(pub_key.data()[PubKeyWithoutPrimarySize]);			// Get an iterator from the last sizeof(uint64_t) bytes
	uint64_t int_id;
	std::memcpy(&int_id, bytes, sizeof(uint64_t));
	return int_id;
}

int32_t savactsale::transferContractToken(name to, int64_t amount, const string& memo, name rampayer)
{
	asset funds(amount, globals::contractSymbol);
	check(funds.amount > 0, "No contract token amount for this user");
	int32_t usedRam = 0;

	// Check if user has not an accounts entry for the contract token
	tokenAcc_table _acctable(globals::TokenContractName, to.value);
	auto itr = _acctable.find(globals::contractSymbol.code().raw());
	if(itr == _acctable.end()){
		// Open access of contract token for the recipient
		action {
			permission_level{rampayer, "active"_n},
			globals::TokenContractName,
			"open"_n,
			std::make_tuple(to, globals::contractSymbol, rampayer)
		}.send();
		usedRam = ram_open_contracttoken;
	}

	// Send the contract token to the recipient
	action {
	  permission_level{get_self(), "active"_n},
	  globals::TokenContractName,
	  "transfer"_n,
	  std::make_tuple(get_self(), to, funds, memo)
	}.send();

	return usedRam;
}

void savactsale::deleteOwner(amountlist_table& amountlistTB, amountlist_table::const_iterator itr, std::list<Owner>::const_iterator ownerItr){
	// Remove this owner
	amountlistTB.modify(itr, get_self(), [&](auto& entry) {
		entry.owner.erase(ownerItr);
	});

	// Delete the whole entry if there are no owners anymore
	if(itr->owner.empty()) {
		amountlistTB.erase(itr);					
	}
}

std::list<savactsale::Owner>::const_iterator savactsale::findOwner(amountlist_table::const_iterator itr, const ecc_public_key& pubKey){
	auto ownerItr = itr->owner.begin();
	while (ownerItr != itr->owner.end()) 	// For each owner in the list
	{
		// Compare the first PubKeyWithoutPrimarySize bytes in the pubKey
		if(std::equal(ownerItr->key.begin(), ownerItr->key.end(), std::begin(pubKey))){
			break;
		}
		++ownerItr;
	}
	return ownerItr;
}

void savactsale::GetToken(int64_t sumPayed, Distribution& dist){
	// Set the payment amount higher before it will used in the calculation of bought contract tokens
	double totalPercentage = UserPercentageForAffi + dist.affiPercentage.contract;
	int64_t addedAmount = (int64_t)(sumPayed * totalPercentage);
	dist.amount.contract.user = buyContractToken(sumPayed + addedAmount);
	dist.amount.contract.affiliate = dist.amount.contract.user * (dist.affiPercentage.contract / (1.0 + totalPercentage));		// Get the amount for the affiliate
	dist.amount.contract.user -= dist.amount.contract.affiliate;																	// and the amount for the paying user	
}