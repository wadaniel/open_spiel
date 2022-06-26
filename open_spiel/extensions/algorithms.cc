#include "open_spiel/extensions/algorithms.h"
#include "open_spiel/extensions/global_variables.h"
#include "open_spiel/extensions/poker_methods.h"

#include <iostream>

namespace extensions
{

int test_sum(int a, int b) { return a + b; } 

int test_cfr(int idx, float val, float* sharedStrategy, const std::map<std::string, int>& buckets) 
{
        
        if ( idx < 0 ) return val;
        sharedStrategy[idx] = val;
        idx -= rand()%10;
        
        for (const auto& [key, value] : buckets) {
            printf("[%s] %d\n", key.c_str(), value);
        }

        return test_cfr(idx, val, sharedStrategy, buckets);
}

float multi_cfr(int numIter, 
        const int updatePlayerIdx, 
        const int startTime, 
        const float pruneThreshold, 
        const bool useRealTimeSearch, 
        const int* handIds, 
        const size_t handIdsSize, 
        const open_spiel::State& state, 
        const int currentStage, 
        int* sharedRegret, const size_t nSharedRegret, 
        float* sharedStrategy,const size_t nSharedStrat, 
        const float* sharedStrategyFrozen, const size_t nSharedFrozenStrat)
{
    float cumValue = 0;
    for (int iter = 0; iter < numIter; iter++) {
        cumValue += cfr(updatePlayerIdx, iter, pruneThreshold, useRealTimeSearch, handIds, handIdsSize, state, currentStage, sharedRegret, nSharedRegret, sharedStrategy, nSharedStrat, sharedStrategyFrozen, nSharedFrozenStrat);
    }
    return cumValue / (float)numIter;
}


float cfr(int updatePlayerIdx, 
            const int time, 
            const float pruneThreshold, 
            const bool useRealTimeSearch, 
            const int* handIds, 
            const size_t handIdsSize, 
            const open_spiel::State& state, 
            const int currentStage, 
            int* sharedRegret, const size_t nSharedRegret, 
            float* sharedStrategy, const size_t nSharedStrat, 
            const float* sharedStrategyFrozen, const size_t nSharedFrozenStrat)
{
    const bool isTerminal = state.IsTerminal();
	// If terminal, return players reward
    if (isTerminal) {
        float playerReward = state.PlayerReward(updatePlayerIdx);
        return playerReward;
    }

	// If chance node, sample random outcome and reiterate cfr
	if(state.IsChanceNode())
	{
    	const auto chanceActions = state.ChanceOutcomes();
		const std::vector<float> weights(chanceActions.size(), 1./ (float)chanceActions.size());
		const int sampledIndex = randomChoice(weights.begin(), weights.end());
        const auto sampledAction = chanceActions[sampledIndex].first;
        const auto new_state = state.Child(sampledAction);

        return cfr(updatePlayerIdx, time, pruneThreshold, useRealTimeSearch, handIds, handIdsSize, *new_state, currentStage, sharedRegret, nSharedRegret, sharedStrategy, nSharedStrat, sharedStrategyFrozen, nSharedFrozenStrat);
	}

	// Define work variables
	std::array<int, 2> privateCards{-1, -1};
	std::array<int, 5> publicCards{-1, -1, -1, -1, -1};
	std::array<int, 3> bets{0, 0, 0};
	
	std::array<bool, 9> explored{true, true, true, true, true, true, true, true, true};
	std::array<float, 9> probabilities{0., 0., 0., 0., 0., 0., 0., 0., 0.};
	std::array<int, 9> regrets{0., 0., 0., 0., 0., 0., 0., 0., 0.};
	std::array<float, 9> strategy{0., 0., 0., 0., 0., 0., 0., 0., 0.};

	const int currentPlayer = state.CurrentPlayer();

    // Retrieve information state
    std::string informationState = state.InformationStateString(currentPlayer);

    // Read betting stage
    const size_t bettingStage = informationState[7] - 48; // 0-4

    // Split of information state string
    const auto informationStateSplit = split(informationState, "]");

    // Bets of players
	std::fill(bets.begin(), bets.end(), 0);
    getBets(informationStateSplit[3], bets);
    
	// Retrieve pot information
    int maxBet = 0;
    int minBet = TOTALSTACK;
    int totalPot = 0;
    for(int bet : bets)
    {
        if (bet > maxBet) maxBet = bet;
        if (bet < minBet) minBet = bet;
        totalPot += bet;
    }

    const int currentBet = bets[currentPlayer];
    const int callSize = maxBet - currentBet;

    // Find active players code
    int activePlayersCode = 0; // all active

    // If someone folded find out which player
    if (informationStateSplit[6].find("f") != std::string::npos)
    {
            if(bets[(currentPlayer+1)%3] > bets[(currentPlayer+2)%3])
            {
                activePlayersCode = 1;
            }
            else
            {
                activePlayersCode = 2;
            }
    }

    const auto roundActions = split(informationStateSplit[6], "|");
    std::string currentRoundActions = roundActions[roundActions.size()-1];

    // Check if someone reraised
    const bool isReraise = std::count(currentRoundActions.begin(), currentRoundActions.end(), 'r') > 1;

    // Get legal actions provided by the game
    auto gameLegalActions = state.LegalActions();
    std::sort(gameLegalActions.begin(), gameLegalActions.end());
    //printf("legal actions sorted \n");
    for(const int action : gameLegalActions){
        //std::cout << action << std::endl;
    }

    // Calculate our legal actions based on abstraction
    const auto ourLegalActions = getLegalActions(bettingStage, totalPot, maxBet, currentBet, isReraise, gameLegalActions);

    assert(ourLegalActions.size() > 0);
    for(int action : ourLegalActions) assert(action < 9);

    const int legalActionsCode = getLegalActionCode(isReraise, bettingStage, ourLegalActions);
    
    // Call size in 10% of total stack size (values from 0 to 9)
    const int chipsToCallFrac = std::min(callSize / 50, 9); //TODO (DW): I suggest min(10*n_chips_to_call // my_stack,9)
    
    // Current bet in 10% of total stack size (values from 0 to 9) //TODO (DW): I suggest min(10*current_bet // total_chips_in_play, 9)
    const int betSizeFrac = std::min(currentBet / 50, 9);
   
    // Init array index
    size_t arrayIndex = 0;
    
    // printf("C\n");
    // Jonathan IMPORTANT for debugging DO NOT remove
    // printf("betting state %d \n", bettingStage);
    // printVec("ourLegalActions", ourLegalActions)

    // Get index in strategy array
    // we only use lossless hand card abstraction in current betting round
    // and only during realtime search
    // and RTS only starts after the preflop round
    if(useRealTimeSearch && bettingStage == currentStage)
    {
        assert(bettingStage > 0);
		assert(handIdsSize == 3);
        arrayIndex = getArrayIndex(handIds[currentPlayer], bettingStage, activePlayersCode, chipsToCallFrac, betSizeFrac, currentPlayer, legalActionsCode, isReraise, true);
        assert(arrayIndex < nSharedStrat);
		assert(arrayIndex < nSharedFrozenStrat);
        // Jonathan IMPORTANT for debugging DO NOT remove
        //printf("betting state %d \n", bettingStage);
        //printf("legal actions ab \n");
        //printVec("ourLegalActions", ourLegalActions);
    }
    else
    {
        // Prepare private cards string
        const auto privateCardsSplit = split(informationStateSplit[4],": ");
        const auto privateCardsStr = privateCardsSplit[1];
        assert(privateCardsStr.size() == 4);

        // Read private cards
        privateCards[1] = getCardCode(privateCardsStr[0], privateCardsStr[1]); 	// lo card
        privateCards[0] = getCardCode(privateCardsStr[2], privateCardsStr[3]);  // hi card
		
		assert(privateCards[0] != privateCards[1]);

		// Make sure they are sorted
		//std::sort(privateCards.begin(), privateCards.end());
		assert(std::is_sorted(privateCards.begin(), privateCards.end()));
	
		// Process public cards if flop or later
		if (bettingStage > 0)
        {
			// Prepare public cards string
        	const auto publicCardSplit = split(informationStateSplit[5], ": ");
        	const auto publicCardsStr = publicCardSplit[1];
        	assert(publicCardsStr.size()%2 == 0);
        	assert(publicCardsStr.size() == (2 + bettingStage)*2);

        	// Read public cards
			const size_t numPublicCards = bettingStage + 2;
			assert(numPublicCards == publicCardsStr.size()/2);

        	for(size_t idx = 0; idx < numPublicCards; ++idx)
        	{
            	publicCards[idx] = getCardCode(publicCardsStr[2*idx], publicCardsStr[2*idx+1]);
        	}
	        
			// Make sure they are sorted
			std::sort(publicCards.begin(), publicCards.begin()+numPublicCards); // TODO (DW) is this a requirement?? ask Jonathan
			assert(std::is_sorted(publicCards.begin(),publicCards.begin()+numPublicCards));	// ascending 
	
		}

        // Get card bucket based on abstraction
        const size_t bucket = getCardBucket(privateCards, publicCards, bettingStage);

        arrayIndex = getArrayIndex(bucket, bettingStage, activePlayersCode, chipsToCallFrac, betSizeFrac, currentPlayer, legalActionsCode, isReraise, false);
        ////printf("D\n");
		assert(arrayIndex < nSharedStrat); // this fails, don't put it
		assert(arrayIndex < nSharedFrozenStrat);
    }

    if(currentPlayer == updatePlayerIdx)
    {
		std::copy(&sharedStrategyFrozen[arrayIndex], &sharedStrategyFrozen[arrayIndex+9], strategy.begin() );

        // Jonathan IMPORTANT for debugging DO NOT remove
        ////printf("betting state %d \n", bettingStage);
        //printVec("ourLegalActions", ourLegalActions);

        if(useRealTimeSearch)
        {

            bool allZero = true;
            for(float actionProb : strategy) if (actionProb != 0.) { allZero = false; break; }
            
			// if all entries zero, take regrets from passed trained strategy
            if (allZero)
            {
                std::copy(&sharedRegret[arrayIndex], &sharedRegret[arrayIndex+9], regrets.begin());
            }
            else
            {
                float expectedValue = 0.;
                for(const int action : ourLegalActions)
                {
					const int absoluteAction = actionToAbsolute(action, maxBet, totalPot);
                    probabilities[action] = strategy[action];
    				auto new_state = state.Child(absoluteAction);
                    ////printf("0\n");
                    const float actionValue = cfr(updatePlayerIdx, time, pruneThreshold, useRealTimeSearch, handIds, handIdsSize, *new_state, currentStage, sharedRegret, nSharedRegret, sharedStrategy, nSharedStrat, sharedStrategyFrozen, nSharedFrozenStrat);
                    ////printf("0\n");
                    expectedValue += actionValue * probabilities[action];
                }
                return expectedValue;
            }
        }
        else{
            std::copy(&sharedRegret[arrayIndex], &sharedRegret[arrayIndex+9], regrets.begin());
        }
        
        // Jonathan IMPORTANT for debugging DO NOT remove
        //printf("betting state %d \n", bettingStage);
        //printf("legal actions c \n");
        //printVec("ourLegalActions", ourLegalActions);

        ////printf("E\n");
		calculateProbabilities(regrets, ourLegalActions, probabilities);

        // Find actions to prune
        if(applyPruning == true && bettingStage < 3){
            for(const int action : ourLegalActions) 
            {
                if (regrets[action] < pruneThreshold) explored[action] = false; // Do not explore actions below prune threshold
                if ((action == 0) || (action == 8)) explored[action] = true;    // Always explore terminal actions
            }
        }
        //printf("x\n");
        
        float expectedValue = 0.;
	    std::array<float, 9> actionValues{0., 0., 0., 0., 0., 0., 0., 0., 0.};
        
        // Iterate only over explored actions
        for(const int action : ourLegalActions) if (explored[action])
        {
			const size_t absoluteAction = actionToAbsolute(action, maxBet, totalPot);
            //printf("xx %d\n", action);
    		auto new_state = state.Child(absoluteAction);
            //printf("z %d\n", action);
            //printf("1\n");
            const float actionValue = cfr(updatePlayerIdx, time, pruneThreshold, useRealTimeSearch, handIds, handIdsSize, *new_state, currentStage, sharedRegret, nSharedRegret, sharedStrategy, nSharedStrat, sharedStrategyFrozen, nSharedFrozenStrat);
            //printf("1\n");
            //printf("zz %d\n", action);
            actionValues[action] = actionValue;
            expectedValue += probabilities[action] * actionValue; // shall we renormalize prob? TODO(DW): verify with Jonathan
        }
        //printf("y\n");
		
        // Multiplier for linear regret
        const float multiplier = 1.; //min(t, 2**10) # stop linear cfr at 32768, be careful about overflows

        // Update active player regrets
        for(const int action : ourLegalActions) if (explored[action])
        {
                const size_t arrayActionIndex = arrayIndex + action;
                sharedRegret[arrayActionIndex] += int(multiplier*(actionValues[action] - expectedValue));
                assert(arrayActionIndex < nSharedRegret);
                if(sharedRegret[arrayActionIndex] > std::numeric_limits<int>::max()) sharedRegret[arrayActionIndex] = std::numeric_limits<int>::max();
                if(sharedRegret[arrayActionIndex] < pruneThreshold*1.03) sharedRegret[arrayActionIndex] = pruneThreshold*1.03;
     	}
        //printf("F\n");
        return expectedValue;
    }
    else
    {
        //  Fill regrets
        if(useRealTimeSearch)
        {
            std::copy(&sharedStrategyFrozen[arrayIndex], &sharedStrategyFrozen[arrayIndex+9], strategy.begin() );
            bool allZero = true;
            for(float actionProb : strategy) if (actionProb != 0.) { allZero = false; break; }
            if(allZero)
            {
                std::copy(&sharedRegret[arrayIndex], &sharedRegret[arrayIndex+9], regrets.begin());
            }
            else{
                std::copy(&sharedStrategyFrozen[arrayIndex], &sharedStrategyFrozen[arrayIndex+9], regrets.begin());
            }
        }
        else{
            std::copy(sharedRegret+arrayIndex, sharedRegret+arrayIndex+9 , regrets.begin() );
        }

        // Calculate probabilities from regrets
        calculateProbabilities(regrets, ourLegalActions, probabilities);
        //printf("G\n");
        
        //////printVec("gl", gameLegalActions.begin(), gameLegalActions.end());
        ////////printVec("ola", ourLegalActions.begin(), ourLegalActions.end());
        //printf("%d %d %d %d %d\n", bettingStage, totalPot, maxBet, currentBet, isReraise);
        ////////printVec("r", regrets.begin(), regrets.end());
        //////printVec("p", probabilities.begin(), probabilities.end());
         
        // Jonathan: ATTENTION: randomChoice returns a value of 0 to 8
    	const int sampledAction = randomChoice(probabilities.begin(), probabilities.end());

        const size_t absoluteAction = actionToAbsolute(sampledAction, maxBet, totalPot);
        //printf("absact %zu (%zu) id: ((%zu)) \n", absoluteAction, sampledAction, sampledAction);
       
        auto new_state = state.Child(absoluteAction);
        //printf("2\n");
        const float expectedValue = cfr(updatePlayerIdx, time, pruneThreshold, useRealTimeSearch, handIds, handIdsSize, *new_state, currentStage, sharedRegret, nSharedRegret, sharedStrategy, nSharedStrat, sharedStrategyFrozen, nSharedFrozenStrat);
        //printf("2\n");
        
		// TODO(DW): update strategy mode 'opponent' (Jonathan: necessary)
    	// has to be in non active player
        // Multiplier for linear regret
        const float multiplier = 1.; //min(t, 2**10) # stop linear cfr at 32768, be careful about overflows
        
        // Update active *non frozen* shared strategy
        for(const int action : ourLegalActions)
        {
            const size_t arrayActionIndex = arrayIndex + action;
            assert(arrayActionIndex < nSharedStrat);
            sharedStrategy[arrayActionIndex] += multiplier*probabilities[action];
     	}

        //printf("H %f\n", expectedValue);
        return expectedValue;
    }
}

//void loadBuckets(std::map<std::string, size_t>& preflopBuckets)
void loadBuckets()
{
	printf("loading preflop buckets..\t"); fflush(stdout);
	readDictionaryFromJson("./lut_200/pre_flop.txt", preflopBucket);
	printf("DONE!\nloading flop buckets..\t\t"); fflush(stdout);
	readDictionaryFromJson("./lut_200/flop.txt", flopBucket);
	printf("DONE!\nloading turn buckets..\t\t"); fflush(stdout);
	readDictionaryFromJson("./lut_200/turn.txt", turnBucket);
	printf("DONE!\nloading river buckets..\t\t"); fflush(stdout);
	readDictionaryFromJson("./lut_200/river.txt", riverBucket);
	printf("DONE!\n");
}



}
