#ifndef _POKER_METHODS_H_
#define _POKER_METHODS_H_

#include <algorithm>
#include "open_spiel/extensions/utils.h"

namespace extensions
{

int getCardCode(char rank, char suit)
{
	int num = (int)rank - 50; // no '0' and no '1', ie '2' == 0

	if (num > 8)
	{
		if      (rank == 'J') num = 9;
		else if (rank == 'Q') num = 10;
		else if (rank == 'K') num = 11;
		else  			  	  num = 12;
	}
    
    int suitCode = -1;
    if 		(suit == 'c') suitCode = 0; // clubs
    else if (suit == 'd') suitCode = 1; // diamonds
    else if (suit == 'h') suitCode = 2; // hearts
    else				  suitCode = 3; // spades

    return num*4+suitCode;
}

void getBets(const std::string& info, std::vector<int>& bets)
{
    auto money = split(info, ": ");
    const auto betStrings = split(money[1]," ");
    for (size_t idx = 0; idx < bets.size(); ++idx)
    {
      bets[idx] = TOTALSTACK - std::stoi(betStrings[idx]);
    }
}

void calculateProbabilities(const std::vector<float>& regret, const std::vector<int>& legalActions, std::vector<float>& probabilities)
{
    float sumValue = 0.f;
    std::vector<float> flooredRegret(legalActions.size(), 0.f);
    for(size_t idx = 0; idx < legalActions.size(); ++idx)
    {
        const int action = legalActions[idx];
        const float floored = regret[action] > 0.f ? regret[action] : 0.f;   
        flooredRegret[idx] = floored;
        sumValue += floored;
    }

    if( sumValue > 1e-12 )
    {
        const float invSum = 1./sumValue;
        for(size_t idx = 0; idx < legalActions.size(); ++idx)
        {
            probabilities[idx] = flooredRegret[idx] * invSum;
        }
    }
    else
    {
        const float unif = 1./(float)legalActions.size();
        for(size_t idx = 0; idx < legalActions.size(); ++idx)
        {
            probabilities[idx] = unif; //TODO(DW): balanced version
        }
    }
}

int getArrayIndex(int bucket, int bettingStage, int activePlayersCode, int chipsToCallFrac, int betSizeFrac, int currentPlayer, int legalActionsCode, int isReraise, bool useRealTimeSearch)
{
    int cumSumProd = 0.;
    const std::vector<int> values = { bucket, bettingStage, activePlayersCode, chipsToCallFrac, betSizeFrac, currentPlayer, legalActionsCode, isReraise };
    if (useRealTimeSearch)
    {
      for(size_t idx = 0; idx < values.size(); ++idx)
        cumSumProd += values[idx]*maxValuesProdRTS[idx];

    }
    else
    {
      for(size_t idx = 0; idx < values.size(); ++idx)
        cumSumProd += values[idx]*maxValuesProd[idx];
    }
    return 9*cumSumProd;
}

int getCardBucket(const std::vector<int>& privateCards, const std::vector<int>& publicCards, size_t bettingStage)
{

#ifdef FAKEDICT
    return std::rand()%150;
#endif

    std::vector<int> lookUpCards;
    if (bettingStage == 0)
    {
		lookUpCards.assign(privateCards.begin(), privateCards.end());
    }
    else
    {
        const size_t numCards = privateCards.size() + publicCards.size();

        std::vector<int> sortedCards(numCards);
        std::copy(privateCards.begin(), privateCards.begin()+2, sortedCards.begin());
        std::copy(publicCards.begin(), publicCards.end(), sortedCards.begin()+2);

        std::vector<int> cardRanks(numCards);
        std::vector<int> cardSuits(numCards);
        for(size_t i = 0; i < numCards; ++i)
        {
            cardRanks[i] = sortedCards[i]/4;
            cardSuits[i] = sortedCards[i]%4;
        }

        const size_t abstractionLen = numCards + 6;
        std::vector<int> abstraction(6);
        std::copy(sortedCards.begin(), sortedCards.end(), abstraction.begin());
        
        const auto cardSuitsOrig = cardSuits;
        const bool isSameSuits = (cardSuits[0] == cardSuits[1]);
        if (isSameSuits)
        {
            abstraction[numCards] = 2;
            abstraction[numCards+1] = 0;
        }
        else
        {
            abstraction[numCards] = 1;
            abstraction[numCards+1] = 1;
        }
        
        std::vector<int> publicSuitsHist(4,0);
        for(size_t i = 0; i < numCards-2; ++i)
            publicSuitsHist[cardSuits[i+2]]++;
        
        // First private card is not clubs '0'
        if(cardSuitsOrig[0] != 0)
        {
            int origPrivateSuit = cardSuitsOrig[0];
            int origPublicNumSuit = publicSuitsHist[origPrivateSuit];
            int origPublicNumClubs = publicSuitsHist[0];

            publicSuitsHist[origPrivateSuit] = origPublicNumClubs;
            cardSuits[0] = 0;

            // Both private cards same
            if (cardSuitsOrig[1] == origPrivateSuit)
                cardSuits[1] = 0;
            // If seconds private card clubs switch card suits
            else if (cardSuitsOrig[1] == 0)
            {
                cardSuits[1] == origPrivateSuit;
            }

        }

        if( (cardSuitsOrig[1] != 0) && (cardSuitsOrig[1] != 1) )
        {
            int origHandSuit = cardSuits[1]; // Changing above is important!
            int origNumSuit = publicSuitsHist[origHandSuit];
            int origNumDiamonds = publicSuitsHist[1];

            publicSuitsHist[1] = origHandSuit;
            publicSuitsHist[origHandSuit] = origNumDiamonds;

        }

        std::copy(publicSuitsHist.begin(), publicSuitsHist.end(), abstraction.end()-4);
        if(isSameSuits)
            std::sort(abstraction.end()-3, abstraction.end(), std::greater<int>());
        else
            std::sort(abstraction.end()-2, abstraction.end(), std::greater<int>());

		lookUpCards.assign(abstraction.begin(), abstraction.end());
    }

    
	//TODO
    assert(false);
    // bucket = gv.card_info_lut[betting_stage_str][lookupCards]
    // if(betting_stage_str != "pre_flop"):
    //   bucket = bucket[0]
	// printf("bucket %zu\n", bucket);
    
	return -1;

}

int actionToAbsolute(int actionIndex, int biggestBet, int totalPot)
{
	if (actionIndex == 0)
	{
		return 0; // fold
	}
    else if (actionIndex == 1)
	{
		return 1; // call
	}
    else if (actionIndex == 8)
	{
		return TOTALSTACK; // all-in
	}
    else if (actionIndex < 6)
	{
		// const std::vector<int> factors = {0, 0, .25, 0.5, .75, 1., 2., 3.};
		const float factor = 0.25 * (actionIndex-1.);
		const int betSize = totalPot * factor;
		return std::max(biggestBet + betSize, TOTALSTACK);
	}
	else if (actionIndex == 6)
	{
		return std::max(biggestBet + totalPot * 2, TOTALSTACK); // factor == 2
	}
	else
	{
		return std::max(biggestBet + totalPot * 3, TOTALSTACK); // factor == 3
	}
}

std::vector<int> getLegalActionsPreflop(int numActions, int totalPot, int maxBet, int prevBet, bool isReraise, const std::vector<long int>& legalActions)
{
    if ( (numActions == 2) && (legalActions[0] == 0) && (legalActions[1] == 1) )
        return std::vector<int> {0, 1};
    else if ( (numActions == 3) && (legalActions[0] == 0) && 
              (legalActions[1] == 1) && (legalActions[2] == TOTALSTACK) )
        return std::vector<int> {0, 1, 8};
    

    size_t numPreActions = 0;
    int minBet = 0;
    if(legalActions[0] == 0)
    {
        minBet = legalActions[2];
        numPreActions = 2;
    }
    else
    {
        minBet = legalActions[1];
        numPreActions = 1;
    }

    const float maxRaiseInPctPot = (float)(TOTALSTACK - prevBet)/(float)totalPot;
    
    size_t maxAction = 1;
    if(maxRaiseInPctPot > 3.)
        maxAction = 7;
    else if(maxRaiseInPctPot > 2.)
        maxAction = 6;
    else if(maxRaiseInPctPot > 1.)
        maxAction = 5;
    else if(maxRaiseInPctPot > 0.75)
        maxAction = 4;
    else if(maxRaiseInPctPot > 0.5)
        maxAction = 3;
    else if (maxRaiseInPctPot > 0.25)
        maxAction = 2;

    // We need to raise at least 1 BB which is 20
    const int minRaise = (maxBet == prevBet) ? BBSIZE : std::max(BBSIZE, minBet - maxBet);
    
    size_t minAction = 5;
    if(totalPot >= 4*minRaise)
        minAction = 2;
    else if (totalPot >= 2*minRaise)
        minAction = 3;
    else if (totalPot >= int(minRaise/0.75))
        minAction = 4;

    const size_t totalActions = numPreActions + maxAction - minAction + 2;
    std::vector<int> actions(totalActions);
	for(size_t idx = 0; idx < numPreActions; ++idx)
		actions[idx] = legalActions[idx];
    for(size_t idx = 0; idx < totalActions-1; ++idx)
		actions[numPreActions+idx] = minAction + idx; // actions between range minAction and (including) maxAction
	actions[totalActions-1] = 8; // always allow all-in
    return actions;
}

std::vector<int> getLegalActionsFlop(int numActions, int totalPot, int maxBet, int prevBet, bool isReraise, const std::vector<long int>& legalActions)
{
    if ( (numActions == 2) && (legalActions[0] == 0) && (legalActions[1] == 1) )
        return std::vector<int> {0, 1};
    else if ( (numActions == 3) && (legalActions[0] == 0) && 
              (legalActions[1] == 1) && (legalActions[2] == TOTALSTACK) )
        return std::vector<int> {0, 1, 8};
    
    int numPreActions = 0;
    int minBet = 0;
    if(legalActions[0] == 0)
    {
        minBet = legalActions[2];
        numPreActions = 2;
    }
    else
    {
        minBet = legalActions[1];
        numPreActions = 1;
    }

    const float maxRaiseInPctPot = (float)(TOTALSTACK - prevBet)/(float)totalPot;
   
    int maxAction = 1;
    if(maxRaiseInPctPot > 2.)
        maxAction = 6;
    else if(maxRaiseInPctPot > 1.)
        maxAction = 5;
    else if(maxRaiseInPctPot > 0.5)
        maxAction = 3;

    // We need to raise at least 1 BB which is 20
    const int minRaise = (maxBet == prevBet) ? BBSIZE : std::max(BBSIZE, minBet - maxBet);
    
    const int minAction = (totalPot >= 2*minRaise) ? 3 : 5;

    size_t totalActions = numPreActions + maxAction - minAction + 2;
	const bool skipActionFour = (minAction < 4 && maxAction > 4);
	if (skipActionFour) totalActions -= 1;

    std::vector<int> actions(totalActions);
	for(size_t idx = 0; idx < numPreActions; ++idx)
		actions[idx] = legalActions[idx];

	if (skipActionFour)
	{	
		int skipIdx = 0;
		for (size_t idx = 0; idx < totalActions-1; ++idx) 
		{
			int action = minAction + idx;
			if (action != 4)
			{
				actions[numPreActions+skipIdx] = minAction + idx; // actions between range minAction and (including) maxAction
				skipIdx++;
			}
		}
	}
	else
	{
		for (size_t idx = 0; idx < totalActions-1; ++idx)
			actions[numPreActions+idx] = minAction + idx; // actions between range minAction and (including) maxAction
	}

    actions[totalActions-1] = 8; // always allow all-in
    return actions;

}

std::vector<int> getLegalActionsTurnRiver(int numActions, int totalPot, int maxBet, int prevBet, bool isReraise, const std::vector<long int>& legalActions)
{
    if ( (numActions == 2) && (legalActions[0] == 0) && (legalActions[1] == 1) )
        return std::vector<int> {0, 1};
    else if ( (numActions == 3) && (legalActions[0] == 0) && 
              (legalActions[1] == 1) && (legalActions[2] == TOTALSTACK) )
        return std::vector<int> {0, 1, 8};
    
    int numPreActions = 0;
    int minBet = 0;
    if(legalActions[0] == 0)
    {
        minBet = legalActions[2];
        numPreActions = 2;
    }
    else
    {
        minBet = legalActions[1];
        numPreActions = 1;
    }

    const float maxRaiseInPctPot = (float)(TOTALSTACK - prevBet)/(float)totalPot;
   
    int maxAction = 1;
    if(maxRaiseInPctPot > 1.)
        maxAction = 5;
    else if(maxRaiseInPctPot > 0.5)
        maxAction = 3;
    
    // We need to raise at least 1 BB which is 20
    const int minRaise = (maxBet == prevBet) ? BBSIZE : std::max(BBSIZE, minBet - maxBet);
    
    const int minAction = (totalPot >= 2*minRaise) ? 3 : 5;

    size_t totalActions = numPreActions + maxAction - minAction + 2;
	const bool skipActionFour = (minAction < 4 && maxAction > 4);
	if (skipActionFour) totalActions -= 1;

    std::vector<int> actions(totalActions);
	for(size_t idx = 0; idx < numPreActions; ++idx)
		actions[idx] = legalActions[idx];

	if (skipActionFour)
	{	
		int skipIdx = 0;
		for (size_t idx = 0; idx < totalActions-1; ++idx) 
		{
			const int action = minAction + idx;
			if (action != 4)
			{
				// actions between range minAction and (including) maxAction, excluding action == 4
				actions[numPreActions+skipIdx] = minAction + idx; 
				skipIdx++;
			}
		}
	}
	else
	{
		for (size_t idx = 0; idx < totalActions-1; ++idx)
			// actions between range minAction and (including) maxAction, action == 4 not present
			actions[numPreActions+idx] = minAction + idx; 
	}

    actions[totalActions-1] = 8; // always allow all-in
    return actions;


}

std::vector<int> getLegalActionsReraise(int numActions, int totalPot, int maxBet, int prevBet, bool isReraise, const std::vector<long int>& legalActions)
{
    if (numActions == 2)
        return std::vector<int> {0, 1};
    else if ( (numActions == 3) && (legalActions[0] == 0) && 
              (legalActions[1] == 1) && (legalActions[2] == TOTALSTACK) )
        return std::vector<int> {0, 1, 8};
    else
    {
    	const float maxRaiseInPctPot = (float)(TOTALSTACK - prevBet)/(float)totalPot;
    
        std::vector<int> preActions;
        if (legalActions[0] == 0)
        {
            if (maxRaiseInPctPot > 1.)
                return std::vector<int> { legalActions[0] , legalActions[1], 5, 8 };
            else
                return std::vector<int> { legalActions[0] , legalActions[1], 8 };

            preActions = std::vector<int>(legalActions.begin(), legalActions.begin()+2);
        }
        else
        {
            if (maxRaiseInPctPot > 1.)
                return std::vector<int> { legalActions[0], 5, 8 };
            else 
                return std::vector<int> { legalActions[0], 8 };
        }
    }
}


std::vector<int> getLegalActions(int currentStage, int totalPot, int maxBet, int currentBet, bool isReraise, const std::vector<long int>& legalActions)
{
    const size_t numActions = legalActions.size();
    
    // Actions in case of reraise
    if (isReraise)
    {
        return getLegalActionsReraise(numActions, totalPot, maxBet, currentBet, isReraise, legalActions);
    }
    // Actions for pre-flop
    else if (currentStage == 0)
    {
        return getLegalActionsPreflop(numActions, totalPot, maxBet, currentBet, isReraise, legalActions);
    }
    // Actions for flop
    else if (currentStage == 1)
    {
        return getLegalActionsFlop(numActions, totalPot, maxBet, currentBet, isReraise, legalActions);
    }
    // Actions for turn and river
    else
    {
        return getLegalActionsTurnRiver(numActions, totalPot, maxBet, currentBet, isReraise, legalActions);
    }
}

}

#endif //_POKER_METHODS_H_
