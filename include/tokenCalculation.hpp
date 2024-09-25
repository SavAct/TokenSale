#include <eosio/eosio.hpp>
#include "globals.hpp"

using namespace std;
using namespace eosio;

class calc
{
public:
	// Calculation of the resulting gains
	// static constexpr int64_t TotalGains = ((EndPrice - StartPrice) * TotalToken) / 2;

	// Constant parameter for the linear function:
	static constexpr double a = (globals::EndPrice - globals::StartPrice) / ((double)globals::TotalToken);
	static constexpr double k = 2.0 / a;
	static constexpr int64_t startT = (int64_t)(globals::StartPrice / a);

	static int64_t newSoldToken(int64_t sumPayed, int64_t soldTotal)
	{
		check(soldTotal < globals::TotalToken, "Order rejected: Sorry you are too late. Here are no more tokens available.");

		// Calc the sold Token
		int64_t startAndSoldTotal = soldTotal + startT;
		double oldS = (double)startAndSoldTotal;		
		
		int64_t newSoldTotal = (int64_t)sqrt((k * sumPayed) + (oldS*oldS)) - startT;	

		// The last buyer gets a bit more token. Because of calculation rounding this may be needed to sell all token 
		int64_t rest = globals::TotalToken - newSoldTotal;
		if(rest <= globals::GiftForLastBuyer){
			// Check that the total token amount is not surpassed
			check(rest > 0, "Order rejected: There are not enough Token for the payed sum.");
			return globals::TotalToken - soldTotal;
		}

		return newSoldTotal - soldTotal;
	}

	// Check the signature for data with a public key 
	static void ecVerify(const string& data, const signature& sig, const public_key& pk)
	{
		const checksum256 digest = sha256(&data[0], data.size());
		assert_recover_key(digest, sig, pk);                          // breaks if the signature doesn't match
	}
};