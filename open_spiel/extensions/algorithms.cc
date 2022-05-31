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


float cfr(int updatePlayerIdx, int time, float pruneThreshold, bool useRealTimeSearch, int* handIds, size_t handIdsSize, std::unique_ptr<open_spiel::State> state, float* sharedStrategy, float* sharedStrategyFrozen) 
{ 
    const int currentPlayer = state->CurrentPlayer();
    const bool isTerminal = state->IsTerminal();

    // If terminal, return players reward
    if (isTerminal) {
        return state->PlayerReward(currentPlayer);
    }

    // Retrieve information state
    std::string informationState = state->InformationStateString(currentPlayer);

    // Read betting stage
    const size_t bettingStage = informationState[7] - 48; // 0-4

    // Split of information state string
    auto informationStateSplit = split(informationState, "][");

    // Bets of players
    std::vector<int> bets(3, 0);
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

    const auto tmp3 = split(informationStateSplit[4],": ");
    std::string privateCardsStr = tmp3[1];
    assert(privateCardsStr.size() == 4);

    // Read private cards
    std::vector<int> privateCards = 
        { getCardCode(privateCardsStr[0], privateCardsStr[1]),
          getCardCode(privateCardsStr[2], privateCardsStr[3]) };

    const auto tmp4 = split(informationStateSplit[5], ": ");
    std::string publicCardsStr = tmp4[1];
    assert(publicCardsStr.size()%2 == 0);

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
    auto gameLegalActions = state->LegalActions();
    std::sort(gameLegalActions.begin(), gameLegalActions.end());
    
    // Calculate our legal actions based on abstraction
    const auto ourLegalActions = getLegalActions(bettingStage, totalPot, maxBet, currentBet, isReraise, gameLegalActions);
    const int legalActionsCode = getLegalActionCode(isReraise, bettingStage, ourLegalActions);
    
    // Bet size in values from 0 to 9
    const int betSizeFrac = std::min(maxBet / 50, 9);
    
    // Call size in values from 0 to 9
    const int chipsToCallFrac = std::min(callSize / 50, 9);
   
    // Init array index
    int arrayIndex = -1;

    // Get index in strategy array
    if(handIdsSize > 0)
    {
        arrayIndex = getArrayIndex(handIds[currentPlayer], bettingStage, activePlayersCode, chipsToCallFrac, betSizeFrac, currentPlayer, legalActionsCode, isReraise, true);
    }
    else
    {
       // Read public cards
       std::vector<int> publicCards(privateCardsStr.size()/2);
       for(size_t i = 0; i < publicCardsStr.size(); i += 2)
       {
         publicCards[i] = getCardCode(publicCardsStr[i], publicCardsStr[i+1]);
       }

       // Get card bucket based on abstraction
       const int bucket = getCardBucket(privateCards, publicCards, bettingStage);

       arrayIndex = getArrayIndex(bucket, bettingStage, activePlayersCode, chipsToCallFrac, betSizeFrac, currentPlayer, legalActionsCode, isReraise, false);
    }

    // Container for action probabilities calculation
    std::vector<float> probabilities(9, 0.);

    if(currentPlayer == updatePlayerIdx)
    {
        std::vector<float>regrets(&sharedStrategy[arrayIndex], &sharedStrategy[arrayIndex+9]);
        if(useRealTimeSearch)
        {

            bool allZero = true;
            for(float regret : regrets) if (regret != 0.) { allZero = false; break; }
            
            if (allZero)
            {
                std::copy(&sharedStrategyFrozen[arrayIndex], &sharedStrategyFrozen[arrayIndex+9], regrets.begin());
            }
            else
            {
                std::vector<float> probabilities(ourLegalActions.size(), 0.f);

                for(int action : ourLegalActions)
                    probabilities[action] = regrets[action];

                float expectedValue = 0.;
                for(size_t i = 0; i < ourLegalActions.size(); ++i)
                {
                    const int action = ourLegalActions[i];
                    const float actionValue = cfr(updatePlayerIdx, time, pruneThreshold, useRealTimeSearch, handIds, handIdsSize, state->Child(action), sharedStrategy, sharedStrategyFrozen);
                    expectedValue += actionValue * probabilities[action];
                }

            }
        }

        calculateProbabilities(regrets, ourLegalActions, probabilities);

    }
    else
    {

        std::vector<float> regrets(&sharedStrategy[arrayIndex], &sharedStrategy[arrayIndex+9]);
        if(useRealTimeSearch)
        {
            bool allZero = true;
            for(float regret : regrets) if (regret != 0.) { allZero = false; break; }
            if(allZero)
            {
                std::copy(&sharedStrategyFrozen[arrayIndex], &sharedStrategyFrozen[arrayIndex+9], regrets.begin());
            }
        }
        
        calculateProbabilities(regrets, ourLegalActions, probabilities);
    }
 
    const int action = randomChoice(ourLegalActions, probabilities);
    const float expectedValue = cfr(updatePlayerIdx, time, pruneThreshold, useRealTimeSearch, handIds, handIdsSize, state->Child(action), sharedStrategy, sharedStrategyFrozen);

    // TODO(DW): update strategy mode 'opponent' (optional)

    return expectedValue;
}

}
