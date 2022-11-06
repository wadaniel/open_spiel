#ifndef _POKER_METHODS_H_
#define _POKER_METHODS_H_

#include <algorithm>
#include <stdlib.h>

#include "open_spiel/extensions/global_variables.h"
#include "open_spiel/extensions/utils.h"

namespace extensions {

int getCardCode(char rank, char suit) {
  int num = (int)rank - 50; // no '0' and no '1', ie '2' == 0

  if (num > 7) {
    if (rank == 'T')
      num = 8;
    else if (rank == 'J')
      num = 9;
    else if (rank == 'Q')
      num = 10;
    else if (rank == 'K')
      num = 11;
    else
      num = 12;
  }

  int suitCode = -1;
  if (suit == 'c')
    suitCode = 0; // clubs
  else if (suit == 'd')
    suitCode = 1; // diamonds
  else if (suit == 'h')
    suitCode = 2; // hearts
  else
    suitCode = 3; // spades

  return num * 4 + suitCode;
}

void getBets(const std::string &info, std::array<int, 3> &bets) {
  auto money = split(info, ": ");
  const auto betStrings = split(money[1], " ");
  for (size_t idx = 0; idx < bets.size(); ++idx) {
    bets[idx] = TOTALSTACK[idx] - std::stoi(betStrings[idx]);
  }
}

// Calculate action probabilities
// Version 0: all uniform
// Version 1: passive, i.e. check or fold if all regrets negative (for RTS)
template<typename Iterator1, typename Iterator2>
void calculateProbabilities(const Iterator1 regretBegin, 
                            const std::vector<int> &legalActions,
                            Iterator2 probabilitiesBegin, 
                            int version = 0) {

  assert( version == 0);
  float sumValue = 0.f;

  for (const int action : legalActions) {
    const float floored = *(regretBegin+action) > 0.f ? *(regretBegin+action) : 0.f;
    *(probabilitiesBegin+action) = floored;
    sumValue += floored;
  }

  if (sumValue > 1e-12) {
    const float invSum = 1. / sumValue;
    for (const int action : legalActions) {
      *(probabilitiesBegin+action) *= invSum;
    }
  } else if (version == 0) {
    const float unif = 1. / (float)legalActions.size();
    for (const int action : legalActions) {
      *(probabilitiesBegin+action) = unif;
    }
  } else /* version == 1 */ {
    // Choose minimal action (fold or check)
    if (legalActions[0] == 0)
      *probabilitiesBegin = 1.; 
    else
      *(probabilitiesBegin+1) = 1.; 
  }
}

std::vector<int> getCardAbstraction(const std::array<int, 2> &privateCards,
                                    const std::array<int, 5> &publicCards,
                                    size_t bettingStage) {

  const size_t numPublicCards = bettingStage + 2;
  const size_t numCards = 4 + bettingStage;

  std::vector<int> sortedCards(numCards);
  std::copy(privateCards.begin(), privateCards.end(), sortedCards.begin());
  std::copy(publicCards.begin(), publicCards.begin() + numPublicCards,
            sortedCards.begin() + 2);

  // sort cards ascending
  std::sort(sortedCards.begin(),
            sortedCards.begin() + 2); 
  std::sort(sortedCards.begin() + 2,
            sortedCards.end()); 

  std::vector<int> cardRanks(numCards);
  std::vector<int> cardSuits(numCards);
  for (size_t i = 0; i < numCards; ++i) {
    cardRanks[i] = sortedCards[i] / 4;
    cardSuits[i] = sortedCards[i] % 4;
  }

  // first numCards filled with ranks, card ids
  // next two entries filled with '[2,0]' for same suits or '[1, 1]' for other
  // suits last four entries filled with suit histogram Note: first card
  // converted to clubs (0) and second to diamond (1) Note: if same suits, hist
  // of last three cards sorted, otherwise hist of last two cards sorted
  std::vector<int> abstraction(numCards + 6);
  std::copy(cardRanks.begin(), cardRanks.end(), abstraction.begin());

  const bool isSameSuits = (cardSuits[0] == cardSuits[1]);

  if (isSameSuits) {
    abstraction[numCards] = 2; // TODO (DW): why do we need '[2,0]' or '[1,1]'?
                               // one entry (0 or 1) is enough
    abstraction[numCards + 1] = 0;
  } else {
    abstraction[numCards] = 1;
    abstraction[numCards + 1] = 1;
  }

  // Count how much there are of each suit in public cards
  std::vector<int> publicSuitsHist(4, 0);
  for (size_t idx = 2; idx < numCards; ++idx)
    publicSuitsHist[cardSuits[idx]]++;

  // Fix the first suit and count how much there are in publics
  int privateFirstSuit = cardSuits[0];
  int privateSecondSuit = cardSuits[1];

  if (privateSecondSuit == 0)
  {
    privateFirstSuit = cardSuits[1];
    privateSecondSuit = cardSuits[0];
    const int firstPublicSuitsHist = publicSuitsHist[privateFirstSuit];
    publicSuitsHist[privateFirstSuit] = publicSuitsHist[privateSecondSuit];
    publicSuitsHist[privateSecondSuit] = firstPublicSuitsHist;
  }

  const int oldNumClubs = publicSuitsHist[0];
  publicSuitsHist[0] = publicSuitsHist[privateFirstSuit];
  publicSuitsHist[privateFirstSuit] = oldNumClubs;

  abstraction[numCards+2] = publicSuitsHist[0];
  
  if (isSameSuits)
  {
    // Sort the last three suits descending and add it to abstraction
    std::sort(publicSuitsHist.begin() + 1, publicSuitsHist.end(),
              std::greater<int>());
    std::copy(publicSuitsHist.begin() + 1, publicSuitsHist.end(),
            abstraction.end() - 3);
  }
  else
  {
    // Fix the second suit aswell and then sort the last two suits descending and add it to abstraction

    const int oldNumDiamonds = publicSuitsHist[1];
    publicSuitsHist[1] = publicSuitsHist[privateSecondSuit];
    publicSuitsHist[privateSecondSuit] = oldNumDiamonds;

    abstraction[numCards+3] = publicSuitsHist[1];
    std::sort(publicSuitsHist.begin() + 2, publicSuitsHist.end(),
              std::greater<int>());
    std::copy(publicSuitsHist.begin() + 2, publicSuitsHist.end(),
            abstraction.end() - 2);
  }

  return abstraction;
}

int actionToAbsolute(int actionIndex, int biggestBet, int totalPot,
                     const std::vector<long int> &legalActions) {
  int absoluteAction = -1;
  const long int stack = legalActions.back();
  if (actionIndex < 2) {
    absoluteAction = actionIndex; // call or fold
  } else if (actionIndex == 8) {
    absoluteAction = stack; // all-in
  } else if (actionIndex < 6) { // 0.25x - 1x
    const float factor = 0.25 * (actionIndex - 1.);
    const int betSize = totalPot * factor;
    absoluteAction = std::min((long int) (biggestBet + betSize), stack);
  } else {
    const int multiplier = actionIndex - 4; // 2x or 3x
    absoluteAction = std::min((long int) (biggestBet + totalPot * multiplier), stack);
  }

  // Check if action is present
  if (std::find(legalActions.begin(), legalActions.end(),
                (long int)absoluteAction) == legalActions.end()) {
    printf("[poker_methods] Error in actionToAbsolute\n");
    printf("[poker_methods] A2A Action not found: %d (biggestBet %d totalPot %d) with stack %d %d %d \n",
           absoluteAction, biggestBet, totalPot, TOTALSTACK[0], TOTALSTACK[1], TOTALSTACK[2]);
    printVec("[poker_methods] legalActions", legalActions.begin(),
             legalActions.end());
    abort();
  }
  return absoluteAction;
}

std::vector<int>
getLegalActionsPreflop(int numActions, int totalPot, int maxBet, int prevBet,
                       bool isReraise,
                       const std::vector<long int> &legalActions) {
  if ((numActions == 2) && (legalActions[0] == 0) && (legalActions[1] == 1))
    return std::vector<int>{0, 1};
  else if ((numActions == 2) && (legalActions[0] == 1))
    return std::vector<int>{1, 8};
  else if ((numActions == 3) && (legalActions[0] == 0) &&
           (legalActions[1] == 1))
    return std::vector<int>{0, 1, 8};

  assert(numActions > 2);

  int minBet = 0;
  size_t numPreActions = 0;
  if (legalActions[0] == 0) {
    minBet = legalActions[2];
    numPreActions = 2;
  } else {
    minBet = legalActions[1];
    numPreActions = 1;
  }

  const float maxLegalAction = legalActions.back();
  // const float betInPctPot = (float)(maxLegalAction - prevBet) /
  // (float)totalPot;
  const float betInPctPot = (float)(maxLegalAction - maxBet) / (float)totalPot;

  size_t maxAction = 1;
  if (betInPctPot > 3.)
    maxAction = 7;
  else if (betInPctPot > 2.)
    maxAction = 6;
  else if (betInPctPot > 1.)
    maxAction = 5;
  else if (betInPctPot > 0.75)
    maxAction = 4;
  else if (betInPctPot > 0.5)
    maxAction = 3;
  else if (betInPctPot > 0.25)
    maxAction = 2;

  // We need to raise at least 1 BB which is 20
  const int minRaise =
      //(maxBet == prevBet) ? BBSIZE : std::max(BBSIZE, minBet - prevBet);
      (maxBet == prevBet) ? BBSIZE : std::max(BBSIZE, minBet - maxBet);

  size_t minAction = 5;
  if (totalPot >= 4 * minRaise)
    minAction = 2;
  else if (totalPot >= 2 * minRaise)
    minAction = 3;
  else if (totalPot >= int(minRaise * 1.33))
    minAction = 4;

  const size_t addonActions =
      maxAction >= minAction ? maxAction - minAction + 1 : 0;
  std::vector<int> actions(numPreActions + addonActions + 1);
  for (size_t idx = 0; idx < numPreActions; ++idx)
    actions[idx] = legalActions[idx];
  for (size_t idx = 0; idx < addonActions; ++idx)
    actions[numPreActions + idx] =
        minAction +
        idx; // actions between range minAction and (including) maxAction

  actions.back() = 8; // always allow all-in
  return actions;
}

std::vector<int>
getLegalActionsFlop(int numActions, int totalPot, int maxBet, int prevBet,
                    bool isReraise, const std::vector<long int> &legalActions) {
  if ((numActions == 2) && (legalActions[0] == 0) && (legalActions[1] == 1))
    return std::vector<int>{0, 1};
  else if ((numActions == 2) && (legalActions[0] == 1))
    return std::vector<int>{1, 8};
  else if ((numActions == 3) && (legalActions[0] == 0) &&
           (legalActions[1] == 1))
    return std::vector<int>{0, 1, 8};

  assert(numActions > 2);

  int minBet = 0;
  size_t numPreActions = 0;
  if (legalActions[0] == 0) {
    minBet = legalActions[2];
    numPreActions = 2;
  } else {
    minBet = legalActions[1];
    numPreActions = 1;
  }

  const float maxLegalAction = legalActions.back();
  // const float betInPctPot = (float)(maxLegalAction - prevBet) /
  // (float)totalPot;
  const float betInPctPot = (float)(maxLegalAction - maxBet) / (float)totalPot;

  size_t maxAction = 1;
  if (betInPctPot > 2.)
    maxAction = 6;
  else if (betInPctPot > 1.)
    maxAction = 5;
  else if (betInPctPot > 0.5)
    maxAction = 3;

  // We need to raise at least 1 BB which is 20
  const int minRaise =
      //(maxBet == prevBet) ? BBSIZE : std::max(BBSIZE, minBet - prevBet);
      (maxBet == prevBet) ? BBSIZE : std::max(BBSIZE, minBet - maxBet);

  const size_t minAction = (totalPot >= 2 * minRaise) ? 3 : 5;

  const size_t addonActions =
      maxAction >= minAction ? maxAction - minAction + 1 : 0;
  const bool skipActionFour = (minAction < 4 && maxAction > 4);

  std::vector<int> actions(numPreActions + addonActions - skipActionFour + 1);
  for (size_t idx = 0; idx < numPreActions; ++idx)
    actions[idx] = legalActions[idx];

  if (skipActionFour) {
    int skipIdx = 0;
    for (size_t idx = 0; idx < addonActions; ++idx) {
      const int action = minAction + idx;
      if (action != 4) {
        actions[numPreActions + skipIdx] =
            action; // actions between range minAction and (including) maxAction
        skipIdx++;
      }
    }
  } else {
    for (size_t idx = 0; idx < addonActions; ++idx)
      actions[numPreActions + idx] =
          minAction +
          idx; // actions between range minAction and (including) maxAction
  }

  actions.back() = 8; // always allow all-in
  return actions;
}

std::vector<int>
getLegalActionsTurnRiver(int numActions, int totalPot, int maxBet, int prevBet,
                         bool isReraise,
                         const std::vector<long int> &legalActions) {
  if ((numActions == 2) && (legalActions[0] == 0) && (legalActions[1] == 1))
    return std::vector<int>{0, 1};
  else if ((numActions == 2) && (legalActions[0] == 1))
    return std::vector<int>{1, 8};
  else if ((numActions == 3) && (legalActions[0] == 0) &&
           (legalActions[1] == 1))
    return std::vector<int>{0, 1, 8};

  assert(numActions > 2);

  size_t numPreActions = 0;
  int minBet = 0;
  if (legalActions[0] == 0) {
    minBet = legalActions[2];
    numPreActions = 2;
  } else {
    minBet = legalActions[1];
    numPreActions = 1;
  }

  const float maxLegalAction = legalActions.back();
  // const float betInPctPot = (float)(maxLegalAction - prevBet) /
  // (float)totalPot;
  const float betInPctPot = (float)(maxLegalAction - maxBet) / (float)totalPot;

  size_t maxAction = 1;
  if (betInPctPot > 1.)
    maxAction = 5;
  else if (betInPctPot > 0.5)
    maxAction = 3;

  // We need to raise at least 1 BB which is 20
  const int minRaise =
      //(maxBet == prevBet) ? BBSIZE : std::max(BBSIZE, minBet - prevBet);
      (maxBet == prevBet) ? BBSIZE : std::max(BBSIZE, minBet - maxBet);

  const size_t minAction = (totalPot >= 2 * minRaise) ? 3 : 5;

  const size_t addonActions =
      maxAction >= minAction ? maxAction - minAction + 1 : 0;
  const bool skipActionFour = (minAction < 4 && maxAction > 4);

  std::vector<int> actions(numPreActions + addonActions - skipActionFour + 1);
  for (size_t idx = 0; idx < numPreActions; ++idx)
    actions[idx] = legalActions[idx];

  if (skipActionFour) {
    actions[numPreActions] = 3;
    actions[numPreActions + 1] = 5;
  } else if (maxAction == 3) {
    actions[numPreActions] = 3;
  } else if (minAction == 5) {
    actions[numPreActions] = 5;
  }
  actions.back() = 8; // always allow all-in
  return actions;
}

std::vector<int>
getLegalActionsReraise(int numActions, int totalPot, int maxBet, int prevBet,
                       bool isReraise,
                       const std::vector<long int> &legalActions) {
  std::vector<int> actions;
  if ((numActions == 2) && (legalActions[0] == 0) && (legalActions[1] == 1)) {
    actions = std::vector<int>{0, 1};
  } else if ((numActions == 2) && (legalActions[0] == 1)) {
    actions = std::vector<int>{1, 8};
  } else if ((numActions == 3) && (legalActions[0] == 0) &&
             (legalActions[1] == 1)) {
    actions = std::vector<int>{0, 1, 8};
  } else {

    assert(numActions > 2);
    const float maxLegalAction = legalActions.back();
    const float betInPctPot =
        (float)(maxLegalAction - maxBet) / (float)totalPot;

    if (legalActions[0] == 0) {
      if (betInPctPot > 1.)
        actions = std::vector<int>{0, 1, 5, 8};
      else
        actions = std::vector<int>{0, 1, 8};
    } else {
      if (betInPctPot > 1.)
        actions = std::vector<int>{1, 5, 8};
      else
        actions = std::vector<int>{1, 8};
    }
  }
  return actions;
}

std::vector<int> getLegalActions(int currentStage, int totalPot, int maxBet,
                                 int currentBet, bool isReraise,
                                 const std::vector<long int> &gameLegalActions) {
  const size_t numActions = gameLegalActions.size();
  
  // Actions in case of reraise
  if (isReraise) {
    return getLegalActionsReraise(numActions, totalPot, maxBet, currentBet,
                                  isReraise, gameLegalActions);
  }
  // Actions for pre-flop
  else if (currentStage == 0) {
    return getLegalActionsPreflop(numActions, totalPot, maxBet, currentBet,
                                  isReraise, gameLegalActions);
  }
  // Actions for flop
  else if (currentStage == 1) {
    return getLegalActionsFlop(numActions, totalPot, maxBet, currentBet,
                               isReraise, gameLegalActions);
  }
  // Actions for turn and river
  else {
    return getLegalActionsTurnRiver(numActions, totalPot, maxBet, currentBet,
                                    isReraise, gameLegalActions);
  } 
}

} // namespace extensions

#endif //_POKER_METHODS_H_
