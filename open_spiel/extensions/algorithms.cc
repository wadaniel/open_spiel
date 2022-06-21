#include "open_spiel/extensions/algorithms.h"
#include "open_spiel/extensions/global_variables.h"
#include "open_spiel/extensions/poker_methods.h"

#include <iostream>

namespace extensions
{

int test_sum(int a, int b) { return a + b; } 

int test_cfr(int idx, float val, float* sharedStrategy) 
{
        
        if ( idx < 0 ) return val;
        sharedStrategy[idx] = val;
        idx -= rand()%10;

        return test_cfr(idx, val, sharedStrategy);
}

/*float multi_cfr(int numIter, int updatePlayerIdx, int time, float pruneThreshold, bool useRealTimeSearch, int* handIds, size_t handIdsSize, const open_spiel::State& state, int currentStage, int* sharedRegret, size_t nSharedRegret, float* sharedStrategy, size_t nSharedStrat, float* sharedStrategyFrozen, size_t nSharedFrozenStrat)
{
    float value;
    for (int i = 0; i < numIter; i++) {
        value = cfr(updatePlayerIdx, time, pruneThreshold, useRealTimeSearch, handIds, handIdsSize, state, currentStage, sharedRegret, nSharedRegret, sharedStrategy, nSharedStrat, sharedStrategyFrozen, nSharedFrozenStrat);
    }
}*/


float cfr(int updatePlayerIdx, int time, float pruneThreshold, bool useRealTimeSearch, int* handIds, size_t handIdsSize, const open_spiel::State& state, int currentStage, int* sharedRegret, size_t nSharedRegret, float* sharedStrategy, size_t nSharedStrat, float* sharedStrategyFrozen, size_t nSharedFrozenStrat) 
{ 
    const bool isTerminal = state.IsTerminal();
    
    // Jonathan: this looks good
	// If terminal, return players reward
    if (isTerminal) {
        float playerReward = state.PlayerReward(updatePlayerIdx);
        return playerReward;
    }

    // Jonathan: this looks good
	// If chance node, sample random outcome and reiterate cfr
	if(state.IsChanceNode())
	{
    	const auto chanceActions = state.ChanceOutcomes();
		const std::vector<float> weights(chanceActions.size(), 1./ (float)chanceActions.size());
		const auto sampledAction = randomChoice(chanceActions.begin(), weights.begin(), weights.end());
        const auto new_state = state.Child(sampledAction.first);

        return cfr(updatePlayerIdx, time, pruneThreshold, useRealTimeSearch, handIds, handIdsSize, *new_state, currentStage, sharedRegret, nSharedRegret, sharedStrategy, nSharedStrat, sharedStrategyFrozen, nSharedFrozenStrat);
	}

    // Jonathan: this looks good
	// Define work variables
	std::array<int, 2> privateCards;
	std::array<int, 5> publicCards;
	std::array<int, 3> bets{0, 0, 0};
	
	std::array<bool, 9> explored{true, true, true, true, true, true, true, true, true};
	std::array<float, 9> actionValues{0., 0., 0., 0., 0., 0., 0., 0., 0.};
	std::array<float, 9> probabilities{0., 0., 0., 0., 0., 0., 0., 0., 0.};
	std::array<int, 9> regrets{0., 0., 0., 0., 0., 0., 0., 0., 0.};
	std::array<float, 9> strategy{0., 0., 0., 0., 0., 0., 0., 0., 0.};

    // Jonathan: this looks good
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

    // Jonathan: this looks good
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

    // Jonathan: this looks good
    // Check if someone reraised
    const bool isReraise = std::count(currentRoundActions.begin(), currentRoundActions.end(), 'r') > 1;

    // Get legal actions provided by the game
    auto gameLegalActions = state.LegalActions();
    std::sort(gameLegalActions.begin(), gameLegalActions.end());
    
    // Calculate our legal actions based on abstraction
    const auto ourLegalActions = getLegalActions(bettingStage, totalPot, maxBet, currentBet, isReraise, gameLegalActions);
    const int legalActionsCode = getLegalActionCode(isReraise, bettingStage, ourLegalActions);
    
    // Call size in 10% of total stack size (values from 0 to 9)
    const int chipsToCallFrac = std::min(callSize / 50, 9); //TODO (DW): I suggest min(10*n_chips_to_call // my_stack,9)
    
    // Current bet in 10% of total stack size (values from 0 to 9) //TODO (DW): I suggest min(10*current_bet // total_chips_in_play, 9)
    const int betSizeFrac = std::min(currentBet / 50, 9);
   
    // Init array index
    int arrayIndex = -1;

    // Get index in strategy array
    // we only use lossless hand card abstraction in current betting round
    // and only during realtime search
    if(useRealTimeSearch && bettingStage == currentStage)
    {
		assert(handIdsSize == 3);
        arrayIndex = getArrayIndex(handIds[currentPlayer], bettingStage, activePlayersCode, chipsToCallFrac, betSizeFrac, currentPlayer, legalActionsCode, isReraise, true);
		assert(arrayIndex < nSharedStrat);
		assert(arrayIndex < nSharedFrozenStrat);
    }
    else
    {
        // Jonathan: this looks good, but should check it is equivalent to get_array_pos

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

        	for(size_t idx = 0; numPublicCards; ++idx)
        	{
            	publicCards[idx] = getCardCode(publicCardsStr[2*idx], publicCardsStr[2*idx+1]);
        	}
	        
			// Make sure they are sorted
			std::sort(publicCards.begin(), publicCards.begin()+numPublicCards); // TODO (DW) is this a requirement?? ask Jonathan
			// assert(std::is_sorted(publicCards.begin(),publicCards.begin()+numPublicCards));	// ascending 
	
		}

        // Get card bucket based on abstraction
        const size_t bucket = getCardBucket(privateCards, publicCards, bettingStage);

        arrayIndex = getArrayIndex(bucket, bettingStage, activePlayersCode, chipsToCallFrac, betSizeFrac, currentPlayer, legalActionsCode, isReraise, false);
		assert(useRealTimeSearch == false || (arrayIndex < nSharedFrozenStrat));
    }
    assert(arrayIndex > -1);

    if(currentPlayer == updatePlayerIdx)
    {
		std::copy(&sharedStrategyFrozen[arrayIndex], &sharedStrategyFrozen[arrayIndex+9], strategy.begin() );
        if(useRealTimeSearch)
        {

            bool allZero = true;
            for(float actionProb : strategy) if (actionProb != 0.) { allZero = false; break; }
            
			// if all entries zero, take regrets from passed learned strategy
            if (allZero)
            {
                std::copy(&sharedRegret[arrayIndex], &sharedRegret[arrayIndex+9], regrets.begin());
            }
            else
            {
                assert(false); // Jonathan: for testing we do not set the frozen regrets so it should not be called

                float expectedValue = 0.;
                for(size_t idx = 0; idx < ourLegalActions.size(); ++idx)
                {
                    const int action = ourLegalActions[idx];
					const int absoluteAction = actionToAbsolute(action, maxBet, totalPot);
                    probabilities[idx] = strategy[action];
    				const auto new_state = state.Child(absoluteAction);
                    const float actionValue = cfr(updatePlayerIdx, time, pruneThreshold, useRealTimeSearch, handIds, handIdsSize, *new_state, currentStage, sharedRegret, nSharedRegret, sharedStrategy, nSharedStrat, sharedStrategyFrozen, nSharedFrozenStrat);
                    expectedValue += actionValue * probabilities[idx];
                }
                return expectedValue;
            }
        }

        // Jonathan: not sure this was copied before
        std::copy(&sharedRegret[arrayIndex], &sharedRegret[arrayIndex+9], regrets.begin());
		calculateProbabilities(regrets, ourLegalActions, probabilities);

        // Jonathan: looks good
        // Find actions to prune
        if(applyPruning == true && bettingStage < 3){
            for(size_t idx = 0; idx < ourLegalActions.size(); ++idx)
            {
                const int action = ourLegalActions[idx];
                if (regrets[action] < pruneThreshold) explored[idx] = false;
                if ((action == 0) || (action == 8)) explored[idx] = true;
            }
        }
        
        float expectedValue = 0.;
        std::fill(actionValues.begin(), actionValues.end(), 0.f);
        
        // Jonathan: looks good
        // Iterate only over explored actions
        for(size_t idx = 0; idx < ourLegalActions.size(); ++idx) if (explored[idx] == true)
        {
            const int action = ourLegalActions[idx];
			const size_t absoluteAction = actionToAbsolute(action, maxBet, totalPot);
    		const auto new_state = state.Child(absoluteAction);
            const float actionValue = cfr(updatePlayerIdx, time, pruneThreshold, useRealTimeSearch, handIds, handIdsSize, *new_state, currentStage, sharedRegret, nSharedRegret, sharedStrategy, nSharedStrat, sharedStrategyFrozen, nSharedFrozenStrat);
            actionValues[idx] = actionValue;
            expectedValue += probabilities[idx] * actionValues[idx]; // shall we renormalize prob? TODO(DW): verify with Jonathan
        }
     
		// Multiplier for linear regret
        const float multiplier = 1.; //min(t, 2**10) # stop linear cfr at 32768, be careful about overflows
        
        // Update active *non frozen* shared strategy
        for(size_t idx = 0; idx < ourLegalActions.size(); ++idx) if(explored[idx] == true)
        {
                const int action = ourLegalActions[idx];
                const size_t arrayActionIndex = arrayIndex + action;
                sharedStrategy[arrayActionIndex] += multiplier*(actionValues[idx] - expectedValue);
                if(sharedStrategy[arrayActionIndex] > 1e32) sharedStrategy[arrayActionIndex] = 1e32;
                if(sharedStrategy[arrayActionIndex] < pruneThreshold*1.03) sharedStrategy[arrayActionIndex] = pruneThreshold*1.03;
     	}
		
		return expectedValue;
    }
    else
    {

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
                std::copy(&sharedStrategyFrozen[arrayIndex], &sharedStrategyFrozen[arrayIndex+9], probabilities.begin());
            }
        }
        else{
            std::copy(sharedRegret+arrayIndex, sharedRegret+arrayIndex+9 , regrets.begin() );
            calculateProbabilities(regrets, ourLegalActions, probabilities);
        }
         
        // Jonathan: looks good
    	const int sampledAction = randomChoice(ourLegalActions.begin(), probabilities.begin(), probabilities.end());
		const size_t absoluteAction = actionToAbsolute(sampledAction, maxBet, totalPot);
		const auto new_state = state.Child(absoluteAction);
        const float expectedValue = cfr(updatePlayerIdx, time, pruneThreshold, useRealTimeSearch, handIds, handIdsSize, *new_state, currentStage, sharedRegret, nSharedRegret, sharedStrategy, nSharedStrat, sharedStrategyFrozen, nSharedFrozenStrat);
        
		// TODO(DW): update strategy mode 'opponent' (Jonathan: necessary)
    	return expectedValue;
    }
}

}
