# TokenSale
From backend to frontend; a decentralized version of a token sale. With a fix price increase and self-regulating funding of advertising partners.

# Features
- Linear price increase.
- Selection of different funding accounts.
- Integration of affiliate codes.
- Optional integration of a SavWeb landing page.
- On a deposit, the required memory is automatically acquired.
- Compatible with deposits via crypto exchanges.
- No account needed; but creates one on demand.


# Actions
```cpp
/** Initialize the contract by setting a start time and unfreeze the contract   
 * @param starttime Unix time stamp. From that time on payments are accepted  
 */
ACTION createsale(uint32_t starttime)
```
```cpp
/** Set pages for the sale
 * @param key   Primary key of the entry. key == 0 for the landing page
 * @param refs  Reference to the landing page file: ref[0] contains referred transaction of the first transaction. If the file is portioned there is a second entry ref[1] with a reference to the last entry
 * @param attri Attributes / optional moreover stuff
 * @param fname File name with name extension
 */
ACTION setpage(uint64_t key, vector<Ref>& refs, string& attri, string& fname)
```
```cpp
/** Pay off contract tokens to the account name
*	@param currenttime	The current unix timestamp
*	@param tokenowner	Account name which should get the contract tokens
* 	@param sig			Signature of the string "{contract account name} {currenttime} {tokenowner}" (without quote signs or curly braces)
* 	@param pubkey		Public key of the token owner
*	@param rampayer		Account name which pays the RAM
*/
ACTION payoff(uint32_t currenttime, name tokenowner, const signature& sig, const public_key& pubkey, name rampayer)
```
```cpp
/** Change the distribution model for an affiliate account name
*	@param affiliate	Name of the affiliate
* 	@param convinced    Status if the affiliate wants 100% contract token or a bit in system token
*/
ACTION setconvinced(name affiliate, bool convinced)
```
```cpp
// Clear all tables, including the global status
ACTION clearall()
```

# Values
Default values defined in globals.hpp

- Payment Symbol: **EOS** with precision **4**
- Contract Symbol: **SAVACT** with precision **4**
- Total issued token: **243200000 SAVACT**
- Start price: **0.04 EOS**
- End price: **0.1 EOS**
- All from the last remaining **100 SAVACT** will be given to the last payer as gift, to settle rounding differences.
- Steps marked in RAM by 0.1%, 1% 10%, 20% and 100%. Can be used to pay out a bonus to the biggest participant until the respective step.

# License
The whole project is open source and free to use, see MIT license. Feel free to suggest improvements.