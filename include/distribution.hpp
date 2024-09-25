#pragma once

// Steps in contract token int64_t units for calculating the percentages for contract and system tokens.     // TODO: Values should be revised
#define affiStep0   20000000
#define affiStep1  200000000
#define affiStep2  800000000
#define affiStep3 2000000000

// The percentage of added amount for mentioning an affiliate
#define UserPercentageForAffi 0.05

struct Distribution {
    struct AffiPercentage{
        double contract = 0;	// Credited percentage of contract token
        double system = 0;		// Credited percentage of system token

        // Calculate the percentages for contract and system tokens
        static AffiPercentage Calc(int64_t amount, bool convinced = true) {
            AffiPercentage distP;
            if(amount < affiStep0){
                distP.contract = 0;
                distP.system = 0;
                return distP;
            }
            if(convinced){
                distP.system = 0;
                if (amount < affiStep1) {
                    distP.contract = 0.05;
                } else if (amount < affiStep2){
                    distP.contract = 0.08;
                } else if (amount < affiStep3){
                    distP.contract = 0.12;
                } else {
                    distP.contract = 0.15;
                }
            } else {
                if (amount < affiStep1) {
                    distP.system = 0.02;
                    distP.contract = 0.02;
                } else if (amount < affiStep2){
                    distP.system = 0.03;
                    distP.contract = 0.03;
                } else if (amount < affiStep3){
                    distP.system = 0.05;
                    distP.contract = 0.05;
                } else {
                    distP.system = 0.05;
                    distP.contract = 0.07;
                }
            }
            return distP;
        }

    } affiPercentage;

    struct Amount {
        // Credited amount of contract token for the user and the affiliate
        struct Contract{
            int64_t user;
            int64_t affiliate;
        } contract;			
        
        // Credited amount of system token for the affiliate
        int64_t system;	
    } amount;
};