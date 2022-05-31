#include "open_spiel/extensions/algorithms.h"
#include "open_spiel/extensions/global_variables.h"
#include "open_spiel/extensions/poker_methods.h"

#include <iostream>

int test_sum(int a, int b) { return a + b; } 

int test_cfr(int idx, float val, float* sharedStrategy) 
{
        if ( idx > 0)
        {
            sharedStrategy[idx-1] = val;
            return test_cfr(idx-1, val, sharedStrategy);
        }
        return val;
}


float cfr(int updatePlayerIdx, bool useRealTimeSearch, std::unique_ptr<open_spiel::State> state, float* sharedStrategy) 
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
    //int(pyspiel.information_state_string(0)[7:8]) # 0 to 4
    const size_t bettingStage = informationState[7] - 48; // 0-4


    // Extract important information
    auto informationStateSplit = split(informationState, "][");
    

    // Bets of players
    std::vector<int> bets(3, 0);
    //bets = np.fromiter((gv.stack - int(x) for x in information_state_split[3].split(": ")[1].split(" ")), dtype=np.int)
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

    //private_cards = information_state_split[4].split(": ")[1]
    const auto tmp3 = split(informationStateSplit[4],": ");
    std::string privateCardsStr = tmp3[1];
    assert(privateCardsStr.size() == 4);

    std::vector<int> privateCards = 
        { getCardCode(privateCardsStr[0], privateCardsStr[1]),
          getCardCode(privateCardsStr[2], privateCardsStr[3]) };

    //public_cards = information_state_split[5].split(": ")[1]
    const auto tmp4 = split(informationStateSplit[5], ": ");
    std::string publicCardsStr = tmp4[1];
    assert(publicCardsStr.size()%2 == 0);

    std::vector<int> publicCards(privateCardsStr.size()/2);
    for(size_t i = 0; i < publicCardsStr.size(); i += 2)
    {
        publicCards[i] = getCardCode(publicCardsStr[i], publicCardsStr[i+1]);
    }

    const int bucket = getCardBucket(privateCards, publicCards, bettingStage);

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
    const bool isReraise = std::count(currentRoundActions.begin(), currentRoundActions.end(), 'r') > 1;

    auto gameLegalActions = state->LegalActions();
    std::sort(gameLegalActions.begin(), gameLegalActions.end());
    const auto ourLegalActions = getLegalActions(bettingStage, totalPot, maxBet, currentBet, isReraise, gameLegalActions);

    const int legalActionsCode = getLegalActionCode(isReraise, bettingStage, ourLegalActions);
    const int chipsToCallFrac = std::min(callSize / 50, 9);
    const int betSizeFrac = std::min(maxBet / 50, 9);
    
    const int arrayIndex = getArrayIndex(bucket, bettingStage, activePlayersCode, chipsToCallFrac, betSizeFrac, currentPlayer, legalActionsCode, isReraise);
 

    // Container for action probabilities calculation
    std::vector<float> probabilities;

    if(currentPlayer == updatePlayerIdx)
    {
        std::vector<float> regret(9);
        if(useRealTimeSearch)
        {
            const std::vector<float> strategy(&sharedRTSstrategyFrozen[arrayIndex], &sharedRTSstrategyFrozen[arrayIndex+9]);

            bool allZero = true;
            for(float s : strategy) if (s != 0.) { allZero = false; break; }
            
            if (allZero)
            {
                std::copy(&sharedRTSRegret[arrayIndex], &sharedRTSRegret[arrayIndex+9], regret.begin());
            }
            else
            {
                std::vector<float> probabilities(ourLegalActions.size(), 0.f);

                for(int action : ourLegalActions)
                    probabilities[action] = strategy[action];

                float expectedValue = 0.;
                for(size_t i = 0; i < ourLegalActions.size(); ++i)
                {
                    long int action = ourLegalActions[i];
                    auto newState = state->Child(action);
                    const float actionValue = cfr(0, true, std::move(newState), sharedStrategy);
                    expectedValue += actionValue * probabilities[action];
                }

            }
        }
        else
        {
            std::copy(&sharedRegret[arrayIndex], &sharedRegret[arrayIndex+9], regret.begin());
        }

        probabilities = calculateProbabilities(regret, ourLegalActions);
        
        const int legalActionsCode = getLegalActionCode(isReraise, bettingStage, ourLegalActions);

    }
    else
    {

        if(useRealTimeSearch)
        {
            const std::vector<float> strategy(&sharedRTSstrategyFrozen[arrayIndex], &sharedRTSstrategyFrozen[arrayIndex+9]);
            bool allZero = true;
            for(float s : strategy) if (s != 0.) { allZero = false; break; }
            if(allZero)
            {
                std::vector<float> regret(&sharedRTSRegret[arrayIndex], &sharedRTSRegret[arrayIndex+9]);
                probabilities = calculateProbabilities(regret, ourLegalActions);
            }
            else
            {
                probabilities = calculateProbabilities(strategy, ourLegalActions);
            }
        }
        else
        {
            const std::vector<float> regret(&sharedRegret[arrayIndex], &sharedRegret[arrayIndex+9]);
            probabilities = calculateProbabilities(regret, ourLegalActions);
        }
    }
 
    const int action = randomChoice(ourLegalActions, probabilities);
    const float expectedValue = cfr(0, true, state->Child(action), sharedStrategy);

    // TODO(DW): update strategy mode 'opponent' (optional)

    return expectedValue;
}
