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
    bets[idx] = TOTALSTACK - std::stoi(betStrings[idx]);
  }
}

// Calculate action probabilities
// Version 0: all uniform
// Version 1: passive, i.e. check or fold if all regrets negative (for RTS) TODO
// Version 2: balanced TODO
void calculateProbabilities(const std::array<int, 9> &regret,
                            const std::vector<int> &legalActions,
                            std::array<float, 9> &probabilities,
                            int version = 0) {
  float sumValue = 0.f;

  for (const int action : legalActions) {
    const float floored = regret[action] > 0.f ? regret[action] : 0.f;
    probabilities[action] = floored;
    sumValue += floored;
  }

  if (sumValue > 1e-12) {
    const float invSum = 1. / sumValue;
    for (const int action : legalActions) {
      probabilities[action] *= invSum;
    }
  } else {
    const float unif = 1. / (float)legalActions.size();
    for (const int action : legalActions) {
      probabilities[action] = unif; // TODO(DW): balanced version
    }
  }
}

//# use lossless abstraction for all states in current stage
// if(len(handIDs) != 0 and stage == currentStage):
// arrayPos = get_array_pos(info_set, handIDs[player])
// #print("CFR handID "+str(handIDs[player])+" - stage "+str(stage)+" -
// arrayposcfr "+str(arrayPos))
size_t getArrayIndex(int bucket, int bettingStage, int activePlayersCode,
                     int chipsToCallFrac, int betSizeFrac, int currentPlayer,
                     int legalActionsCode, int isReraise,
                     bool useRealTimeSearch) {
  size_t cumSumProd = 0.;
  const std::vector<int> values = {
      bucket,      bettingStage,  activePlayersCode, chipsToCallFrac,
      betSizeFrac, currentPlayer, legalActionsCode,  isReraise};
  if (useRealTimeSearch)
    for (size_t idx = 0; idx < values.size(); ++idx)
      cumSumProd += values[idx] * maxValuesProdRTS[idx];
  else
    for (size_t idx = 0; idx < values.size(); ++idx) {
      cumSumProd += values[idx] * maxValuesProd[idx];
    }
  cumSumProd *= 9;
  return cumSumProd; // TODO: check again this logic
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

  // If second card clubs we swap (Note: we ignore the rank in flush)
  if (cardSuits[0] != 0 && cardSuits[1] == 0) {
    cardSuits[1] = cardSuits[0];
    cardSuits[0] = 0;
  }

  const int origPrivateFirstSuit = cardSuits[0];
  const int origPrivateSecondSuit = cardSuits[1];

  // First private card is not clubs '0'
  if (origPrivateFirstSuit != 0) {
    const int origPublicNumSameSuit = publicSuitsHist[origPrivateFirstSuit];
    const int origPublicNumClubs = publicSuitsHist[0];

    publicSuitsHist[origPrivateFirstSuit] = origPublicNumClubs;
    publicSuitsHist[0] = origPublicNumSameSuit;
  }

  // Second private card is not diamonds '1' and not of same suit as first
  if (origPrivateSecondSuit != 1 && isSameSuits == false) {
    const int origPublicNumSameSuit = publicSuitsHist[origPrivateSecondSuit];
    const int origPublicNumDiamonds = publicSuitsHist[1];

    publicSuitsHist[origPrivateSecondSuit] = origPublicNumDiamonds;
    publicSuitsHist[1] = origPublicNumSameSuit;
  }

  /*
  // Both private cards same
  if (cardSuitsOrig[1] == origPrivateSuit)
      cardSuits[1] = 0;
  // If seconds private card clubs switch card suits
  else if (cardSuitsOrig[1] == 0)
  {
      cardSuits[1] == origPrivateSuit;
  }*/

  // Sort histogram of diamond, spades and hearts
  if (isSameSuits)
    std::sort(publicSuitsHist.begin() + 1, publicSuitsHist.end(),
              std::greater<int>());
  // Sort histogram of spades and hearts
  else
    std::sort(publicSuitsHist.begin() + 2, publicSuitsHist.end(),
              std::greater<int>());

  std::copy(publicSuitsHist.begin(), publicSuitsHist.end(),
            abstraction.end() - 4);

  return abstraction;
}

size_t getCardBucket(const std::array<int, 2> &privateCards,
                     const std::array<int, 5> &publicCards,
                     size_t bettingStage) {

#ifdef FAKEDICT
  return std::rand() % 150;
#else
  assert(preflopBucket.size() > 0);
  assert(flopBucket.size() > 0);
  assert(turnBucket.size() > 0);
  assert(riverBucket.size() > 0);
#endif

  size_t bucket = 0;

  try {
    if (bettingStage == 0) {
      char str[20];
      sprintf(str, "%d,%d", privateCards[0], privateCards[1]);
      bucket = preflopBucket.at(str);
    } else {
      std::vector<int> abstraction =
          getCardAbstraction(privateCards, publicCards, bettingStage);
      std::stringstream abstractionStrStream;
      std::copy(abstraction.begin(), abstraction.end(),
                std::ostream_iterator<int>(abstractionStrStream, ""));

      if (bettingStage == 1)
        bucket = flopBucket.at(abstractionStrStream.str());
      else if (bettingStage == 2)
        bucket = turnBucket.at(abstractionStrStream.str());
      else
        bucket = riverBucket.at(abstractionStrStream.str());
    }
  } catch (const std::out_of_range &e) {
    printf("Key not found in buckets!");
    printf("Betting stage %zu\n", bettingStage);
    printVec("privateCards", privateCards.begin(), privateCards.end());
    printVec("publicCards", publicCards.begin(), publicCards.end());
    exit(2);
  }

  return bucket;
}

int actionToAbsolute(int actionIndex, int biggestBet, int totalPot,
                     const std::vector<long int> &legalActions) {
  int absoluteAction = -1;
  if (actionIndex < 2) {
    absoluteAction = actionIndex; // call or fold
  } else if (actionIndex == 8) {
    absoluteAction = TOTALSTACK; // all-in
  } else if (actionIndex < 6) {
    // const std::vector<int> factors = { NA, NA, .25, 0.5, .75, 1., 2., 3.};
    const float factor = 0.25 * (actionIndex - 1.);
    const int betSize = totalPot * factor;
    absoluteAction = std::min(biggestBet + betSize, TOTALSTACK);
  } else {
    const int multiplier = actionIndex - 4; // 2 or 3
    absoluteAction = std::min(biggestBet + totalPot * multiplier, TOTALSTACK);
  }

  // Check if action is present
  if (std::find(legalActions.begin(), legalActions.end(),
                (long int)absoluteAction) == legalActions.end()) {
    printf("Error in actionToAbsolute\n");
    printf("Action not found: %d (biggestBet %d totalPot %d)\n", absoluteAction,
           biggestBet, totalPot);
    printVec("legalActions", legalActions.begin(), legalActions.end());
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
  else if ((numActions == 3) && (legalActions[0] == 0) &&
           (legalActions[1] == 1) && (legalActions[2] == TOTALSTACK))
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
  const float betInPctPot = (float)(maxLegalAction - prevBet) / (float)totalPot;

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
      (maxBet == prevBet) ? BBSIZE : std::max(BBSIZE, minBet - prevBet);

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
  else if ((numActions == 3) && (legalActions[0] == 0) &&
           (legalActions[1] == 1) && (legalActions[2] == TOTALSTACK))
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
  const float betInPctPot = (float)(maxLegalAction - prevBet) / (float)totalPot;

  size_t maxAction = 1;
  if (betInPctPot > 2.)
    maxAction = 6;
  else if (betInPctPot > 1.)
    maxAction = 5;
  else if (betInPctPot > 0.5)
    maxAction = 3;

  // We need to raise at least 1 BB which is 20
  const int minRaise =
      (maxBet == prevBet) ? BBSIZE : std::max(BBSIZE, minBet - prevBet);

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
      int action = minAction + idx;
      if (action != 4) {
        actions[numPreActions + skipIdx] =
            minAction +
            idx; // actions between range minAction and (including) maxAction
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
  else if ((numActions == 3) && (legalActions[0] == 0) &&
           (legalActions[1] == 1) && (legalActions[2] == TOTALSTACK))
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
  const float betInPctPot = (float)(maxLegalAction - prevBet) / (float)totalPot;

  size_t maxAction = 1;
  if (betInPctPot > 1.)
    maxAction = 5;
  else if (betInPctPot > 0.5)
    maxAction = 3;

  // We need to raise at least 1 BB which is 20
  const int minRaise =
      (maxBet == prevBet) ? BBSIZE : std::max(BBSIZE, minBet - prevBet);

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
  } else if ((numActions == 2) && (legalActions[0] == 1) &&
             (legalActions[1] == TOTALSTACK)) {
    actions = std::vector<int>{1, 8};
  } else if ((numActions == 3) && (legalActions[0] == 0) &&
             (legalActions[1] == 1) && (legalActions[2] == TOTALSTACK)) {
    actions = std::vector<int>{0, 1, 8};
  } else {

    assert(numActions > 2);
    const float maxLegalAction = legalActions.back();
    const float betInPctPot =
        (float)(maxLegalAction - prevBet) / (float)totalPot;

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
                                 const std::vector<long int> &legalActions) {
  const size_t numActions = legalActions.size();
  // Actions in case of reraise
  if (isReraise) {
    return getLegalActionsReraise(numActions, totalPot, maxBet, currentBet,
                                  isReraise, legalActions);
  }
  // Actions for pre-flop
  else if (currentStage == 0) {
    return getLegalActionsPreflop(numActions, totalPot, maxBet, currentBet,
                                  isReraise, legalActions);
  }
  // Actions for flop
  else if (currentStage == 1) {
    return getLegalActionsFlop(numActions, totalPot, maxBet, currentBet,
                               isReraise, legalActions);
  }
  // Actions for turn and river
  else {
    return getLegalActionsTurnRiver(numActions, totalPot, maxBet, currentBet,
                                    isReraise, legalActions);
  }
}

} // namespace extensions

#endif //_POKER_METHODS_H_
