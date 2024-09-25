#pragma once
#include "base58.hpp"
#include <vector>

using namespace eosio;
using namespace std;

class MemoParams
{
public:
	std::vector<std::variant<name, ecc_public_key>> user;
	std::variant<name, ecc_public_key> affiliate;
	int donationAccNumber = 0;
	bool hasAffiliate = false;

	/** Holds the parameters: user and affiliate name or public key and the donationNumber
	*	@param memo	A string which contains the parameters separated by the chars of the static const parasigns property
	*/
	MemoParams(const string& memo)
	{
		// Trim the start and the end of the memo
		string _memo(Trim(memo));

		// Get all parameters which are defined in the memo
		GetParams(_memo);
	}

	/** The signs to separate the memo. The sequence of the chars are imported. */
	inline static const string parasigns = "- #@";

	/** Set a parameter from string
	*	@param parameter	Parameter as string
	*	@param type 		The type of the parameter
	*/
	void SetParameter(const string& parameter, int type){
		if(parameter.size() > 0){
			// Switch between the order of chars in parasigns
			switch(type){
				case 0:	case 1: 
					// User name or key
					user.push_back(GetUser(parameter));
					break;
				case 2:	
					// Affiliate name or key
					affiliate = GetUser(parameter);
					hasAffiliate = true;
					break;
				case 3:	
					// Donation account number
					donationAccNumber = stoi(parameter);
					break;
			}
		}
	}

	/** Get a user parameter from string
	*	@param para_str	User parameter as string
	*	@return 		The user parameter as variant of an account name or a public key
	*/
	static std::variant<name, ecc_public_key> GetUser(const string& para_str){
		if (para_str.length() > 12)
			return string_to_ecc_public_key(para_str);
		else
			return name(para_str);
	} 

	/** Get all parameters which are defined in a string
	*	@param memo The string with all parameters
	*/
	void GetParams(const string& memo)
	{	 
		int type, lastType = 0; 
		string::const_iterator it = memo.begin();
		string::const_iterator last_it = memo.begin();
		for(; it != memo.end(); ++it) {
			type = parasigns.find(*it);
			if(type != string::npos) {
				SetParameter(string(last_it, it), lastType);
				last_it = it + 1;
				lastType = type;
			}			
		}
		SetParameter(std::string(last_it, it), lastType);
	}

	/** Trim a string at the start and the end
	*	@return The trimmed string
	*/
	static string Trim(const string& str)
	{
		static const string whitespaces(" \t\f\v\n\r");
		auto strStart = str.find_first_not_of(whitespaces);
		auto strEnd = str.find_last_not_of(whitespaces);

		if (strStart != string::npos)
		{
			if (strEnd != string::npos)
				return str.substr(strStart, strEnd + 1);
			else
				return str.substr(strStart);
		}
		else
		{
			if (strEnd != string::npos)
				return str.substr(0, strEnd + 1);
			else
				return str;
		}
	}

	/** Convert a string to a name and check if it exists
	*	@return The trimmed string
	*/
	static name getCheckedName(const string& str)
	{
		name Name(str);
		check(is_account(Name), "Account does not exist");
		return Name;
	}

	/** Convert the public key from string to array<char, 33>
	*	@return The public key
	*/
	static ecc_public_key string_to_ecc_public_key(const string& public_key_str)
	{
		check(public_key_str.length() == 53, "Length of public key should be 53 but is " + to_string(public_key_str.length()) + ".");

		// Check the prefix "EOS" currently for "K1" and "R1" key type
		string pubkey_prefix("EOS");
		auto result = mismatch(pubkey_prefix.begin(), pubkey_prefix.end(), public_key_str.begin());
		check(result.first == pubkey_prefix.end(), "Public key should be prefixed with 'EOS'.");

		// Remove the prefix
		auto base58substr = public_key_str.substr(pubkey_prefix.length());

		// Decode the string with base 58
		vector<unsigned char> vch;
		check(Base58::decode_base58(base58substr, vch), "Decoding of public key failed.");
		check(vch.size() == 37, "Invalid public key");

		// Store the first 33 byte in an array
		ecc_public_key pubkey_data; //array<unsigned char,33> pubkey_data;
		copy_n(vch.begin(), 33, pubkey_data.begin());

		// Check checksum
		checksum160 checksum2 = ripemd160(reinterpret_cast<char*>(pubkey_data.data()), 33);
		int res = memcmp(checksum2.extract_as_byte_array().data(), &vch.end()[-4], 4);
		check(res == 0, "ERROR: Wrong checksum, check your public key for typos.");

		return pubkey_data;
	}
};