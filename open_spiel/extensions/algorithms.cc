#include "open_spiel/extensions/algorithms.h"
#include "open_spiel/extensions/belief.h"
#include "open_spiel/extensions/global_variables.h"
#include "open_spiel/extensions/poker_methods.h"
#include "open_spiel/games/universal_poker.h"
#include <iostream>
#include <regex>

namespace extensions {

float multi_cfr(int numIter, const int updatePlayerIdx, const int startTime,
                const float pruneThreshold, const bool useRealTimeSearch,
                const int *handIds, const size_t handIdsSize,
                const open_spiel::State &state, const int currentStage,
                int *sharedRegret, const size_t nSharedRegret,
                float *sharedStrategy, const size_t nSharedStrat,
                const float *sharedStrategyFrozen,
                const size_t nSharedFrozenStrat) {
  float cumValue = 0;
  for (int iter = 0; iter < numIter; iter++) {
    cumValue += cfr(updatePlayerIdx, startTime, pruneThreshold, useRealTimeSearch,
                    handIds, handIdsSize, state, currentStage, sharedRegret,
                    nSharedRegret, sharedStrategy, nSharedStrat,
                    sharedStrategyFrozen, nSharedFrozenStrat);
  }
  return cumValue / (float)numIter;
}

float cfr(int updatePlayerIdx, const int time, const float pruneThreshold,
          const bool useRealTimeSearch, const int *handIds,
          const size_t handIdsSize, const open_spiel::State &state,
          const int currentStage, int *sharedRegret, const size_t nSharedRegret,
          float *sharedStrategy, const size_t nSharedStrat,
          const float *sharedStrategyFrozen, const size_t nSharedFrozenStrat) {

  assert(time > 0);
  const bool isTerminal = state.IsTerminal();
  // If terminal, return players reward
  if (isTerminal) {
    return state.PlayerReward(updatePlayerIdx);
  }

  // If chance node, sample random outcome and reiterate cfr
  if (state.IsChanceNode()) {
    const auto chanceActions = state.ChanceOutcomes();
    const std::vector<float> weights(chanceActions.size(),
                                     1. / (float)chanceActions.size());
    const int sampledIndex = randomChoice(weights.begin(), weights.end());
    const auto sampledAction = chanceActions[sampledIndex].first;
    const auto new_state = state.Child(sampledAction);

    return cfr(updatePlayerIdx, time, pruneThreshold, useRealTimeSearch,
               handIds, handIdsSize, *new_state, currentStage, sharedRegret,
               nSharedRegret, sharedStrategy, nSharedStrat,
               sharedStrategyFrozen, nSharedFrozenStrat);
  }

  // Define work variables
  std::array<int, 2> privateCards{-1, -1};
  std::array<int, 5> publicCards{-1, -1, -1, -1, -1};
  std::array<int, 3> bets{0, 0, 0};

  std::array<bool, 9> explored{true, true, true, true, true,
                               true, true, true, true};
  std::array<int, 9> regrets{0, 0, 0, 0, 0, 0, 0, 0, 0};
  std::array<float, 9> strategy{0., 0., 0., 0., 0., 0., 0., 0., 0.};
  std::array<float, 9> probabilities{0., 0., 0., 0., 0., 0., 0., 0., 0.};

  const int currentPlayer = state.CurrentPlayer();

  // Retrieve information state
  std::string informationState = state.InformationStateString(currentPlayer);

  // Read betting stage
  const size_t bettingStage = informationState[7] - 48; // 0-4
  assert(bettingStage < 4);
  assert(bettingStage >=
         0); // Jonathan: at the moment we use RTS only from flop

  // Split of information state string
  // const auto informationStateSplit = split(informationState, "\\]\\[");
  const auto informationStateSplit = split(informationState, "][");

  // Bets of players
  std::fill(bets.begin(), bets.end(), 0);
  getBets(informationStateSplit[3], bets);

  // Retrieve pot information
  int maxBet = 0;
  int minBet = TOTALSTACK;
  int totalPot = 0;
  for (int bet : bets) {
    if (bet > maxBet)
      maxBet = bet;
    if (bet < minBet)
      minBet = bet;
    totalPot += bet;
  }

  const int currentBet = bets[currentPlayer];
  const int callSize = maxBet - currentBet;

  // Find active players code
  int activePlayersCode = 0; // all active

  // If someone folded find out which player
  if (informationStateSplit[6].find("f") != std::string::npos) {
    if (bets[(currentPlayer + 1) % 3] > bets[(currentPlayer + 2) % 3]) {
      activePlayersCode = 1;
    } else {
      activePlayersCode = 2;
    }
  }

  const auto roundActions = split(informationStateSplit[6], "|");
  std::string currentRoundActions = roundActions[roundActions.size() - 1];

  // Check if someone reraised
  const bool isReraise = std::count(currentRoundActions.begin(),
                                    currentRoundActions.end(), 'r') > 1;

  // Get legal actions provided by the game
  auto gameLegalActions = state.LegalActions();
  std::sort(gameLegalActions.begin(), gameLegalActions.end());

  // Calculate our legal actions based on abstraction
  const auto ourLegalActions = getLegalActions(
      bettingStage, totalPot, maxBet, currentBet, isReraise, gameLegalActions);

  assert(ourLegalActions.size() > 0);
  // printVec("ourLegalActions", ourLegalActions.begin(),
  // ourLegalActions.end());
  for (int action : ourLegalActions)
    assert(action < 9);

  const int legalActionsCode =
      getLegalActionCode(isReraise, bettingStage, ourLegalActions);

  // Call size in 10% of total stack size (values from 0 to 9)
  const int chipsToCallFrac = std::min(callSize / 50, 9);

  // Current bet in 10% of total stack size (values from 0 to 9)
  const int betSizeFrac = std::min(currentBet / 50, 9);

  // Init array index
  size_t arrayIndex = 0;

  // Get index in strategy array
  // we only use lossless hand card abstraction in current betting round
  // and only during realtime search
  // and RTS only starts after the preflop round
  if (useRealTimeSearch && (bettingStage == currentStage) &&
      bettingStage != 0) {
    assert(bettingStage >= 0);
    assert(handIdsSize == 3);
    arrayIndex =
        getArrayIndex(handIds[currentPlayer], bettingStage, activePlayersCode,
                      chipsToCallFrac, betSizeFrac, currentPlayer,
                      legalActionsCode, isReraise, true);
    assert(arrayIndex < nSharedStrat);
    assert(arrayIndex < nSharedFrozenStrat);
  } else {
    // Prepare private cards string
    const auto privateCardsSplit = split(informationStateSplit[4], ": ");
    const auto privateCardsStr = privateCardsSplit[1];
    assert(privateCardsStr.size() == 4);

    // Read private cards
    privateCards[0] =
        getCardCode(privateCardsStr[2], privateCardsStr[3]); // lo card
    privateCards[1] =
        getCardCode(privateCardsStr[0], privateCardsStr[1]); // hi card

    assert(privateCards[0] != privateCards[1]);

    // Process public cards if flop or later
    if (bettingStage > 0) {
      // Prepare public cards string
      const auto publicCardSplit = split(informationStateSplit[5], ": ");
      const auto publicCardsStr = publicCardSplit[1];
      assert(publicCardsStr.size() % 2 == 0);
      assert(publicCardsStr.size() == (2 + bettingStage) * 2);

      // Read public cards
      const size_t numPublicCards = bettingStage + 2;
      assert(numPublicCards == publicCardsStr.size() / 2);

      for (size_t idx = 0; idx < numPublicCards; ++idx) {
        publicCards[idx] =
            getCardCode(publicCardsStr[2 * idx], publicCardsStr[2 * idx + 1]);
      }
    }

    // Get card bucket based on abstraction
    const size_t bucket =
        getCardBucket(privateCards, publicCards, bettingStage);

    arrayIndex = getArrayIndex(bucket, bettingStage, activePlayersCode,
                               chipsToCallFrac, betSizeFrac, currentPlayer,
                               legalActionsCode, isReraise, false);

    assert(arrayIndex < nSharedStrat);
    assert(arrayIndex < nSharedFrozenStrat);
  }

  if (currentPlayer == updatePlayerIdx) {
    std::copy(&sharedStrategyFrozen[arrayIndex],
              &sharedStrategyFrozen[arrayIndex + 9], strategy.begin());

    if (useRealTimeSearch) {

      bool allZero = true;
      for (float actionProb : strategy)
        if (actionProb != 0.) {
          allZero = false;
          break;
        }

      // if all entries zero, take regrets from passed trained strategy
      assert(allZero);
      if (allZero) {
        std::copy(&sharedRegret[arrayIndex], &sharedRegret[arrayIndex + 9],
                  regrets.begin());
      } else {
        float expectedValue = 0.;
        for (const int action : ourLegalActions) {
          const size_t absoluteAction =
              actionToAbsolute(action, maxBet, totalPot, gameLegalActions);
          probabilities[action] = strategy[action];
          auto new_state = state.Child(absoluteAction);
          const float actionValue =
              cfr(updatePlayerIdx, time, pruneThreshold, useRealTimeSearch,
                  handIds, handIdsSize, *new_state, currentStage, sharedRegret,
                  nSharedRegret, sharedStrategy, nSharedStrat,
                  sharedStrategyFrozen, nSharedFrozenStrat);
          expectedValue += actionValue * probabilities[action];
        }
        return expectedValue;
      }
    } else {
      std::copy(&sharedRegret[arrayIndex], &sharedRegret[arrayIndex + 9],
                regrets.begin());
    }
    calculateProbabilities(regrets, ourLegalActions, probabilities);

    // Find actions to prune
    if (applyPruning == true && bettingStage < 3) {
      for (const int action : ourLegalActions) {
        if (regrets[action] < pruneThreshold)
          explored[action] =
              false; // Do not explore actions below prune threshold
        if ((action == 0) || (action == 8))
          explored[action] = true; // Always explore terminal actions
      }
    }

    float expectedValue = 0.;
    std::array<float, 9> actionValues{0., 0., 0., 0., 0., 0., 0., 0., 0.};
    assert(ourLegalActions.size() >= 1);
    // Iterate only over explored actions
    for (const int action : ourLegalActions) {
      if (explored[action]) {
        const size_t absoluteAction =
            actionToAbsolute(action, maxBet, totalPot, gameLegalActions);
        auto new_state = state.Child(absoluteAction);
        const float actionValue =
            cfr(updatePlayerIdx, time, pruneThreshold, useRealTimeSearch,
                handIds, handIdsSize, *new_state, currentStage, sharedRegret,
                nSharedRegret, sharedStrategy, nSharedStrat,
                sharedStrategyFrozen, nSharedFrozenStrat);
        actionValues[action] = actionValue;

        expectedValue += probabilities[action] * actionValue;
      }
    }

    // Multiplier for linear regret
    const float multiplier = std::min(time, 32768); 

    // Update active player regrets
    for (const int action : ourLegalActions)
      if (explored[action]) {
        const size_t arrayActionIndex = arrayIndex + action;
        sharedRegret[arrayActionIndex] +=
            int(multiplier * (actionValues[action] - expectedValue));

        assert(arrayActionIndex < nSharedRegret);
        if (sharedRegret[arrayActionIndex] < pruneThreshold * 1.03)
          sharedRegret[arrayActionIndex] = pruneThreshold * 1.03;
        if (sharedRegret[arrayActionIndex] > (int) (std::numeric_limits<int>::max() * 0.95))
          sharedRegret[arrayActionIndex] = (int) (std::numeric_limits<int>::max() * 0.95);
      }

    return expectedValue;
  } else {
    //  Only your own probabilities can be frozen, not the ones of other players
    std::copy(&sharedRegret[arrayIndex], &sharedRegret[arrayIndex + 9],
              regrets.begin());

    // Calculate probabilities from regrets
    calculateProbabilities(regrets, ourLegalActions, probabilities);

    // randomChoice returns a value of 0 to 8
    const int sampledAction =
        randomChoice(probabilities.begin(), probabilities.end());
    const size_t absoluteAction =
        actionToAbsolute(sampledAction, maxBet, totalPot, gameLegalActions);
    auto new_state = state.Child(absoluteAction);

    //Update shared strategy
    if(useRealTimeSearch && (currentPlayer == (updatePlayerIdx + 1)%3))
    {
      const float multiplier = std::min(time, 32768);
      for (const int action : ourLegalActions) {
        const size_t arrayActionIndex = arrayIndex + action;
        assert(arrayActionIndex < nSharedStrat);
        sharedStrategy[arrayActionIndex] += multiplier * probabilities[action];
      }
    }

    return cfr(updatePlayerIdx, time, pruneThreshold, useRealTimeSearch, handIds,
        handIdsSize, *new_state, currentStage, sharedRegret, nSharedRegret,
        sharedStrategy, nSharedStrat, sharedStrategyFrozen, nSharedFrozenStrat);
  }
}

float cfr_realtime(const int numIter, const int updatePlayerIdx, const int time,
                   const float pruneThreshold, const open_spiel::State &state,
                   float *handBeliefs, const size_t numPlayer,
                   const size_t numHands, const int currentStage,
                   int *sharedRegret, const size_t nSharedRegret,
                   float *sharedStrategy, const size_t nSharedStrat,
                   const float *sharedStrategyFrozen,
                   const size_t nSharedFrozenStrat) {
  assert(currentStage > 0);

  // Fill data
  const open_spiel::universal_poker::UniversalPokerState &pokerState =
      static_cast<const open_spiel::universal_poker::UniversalPokerState &>(
          state);
  std::vector<float> newHandBeliefs =
      std::vector<float>(handBeliefs, handBeliefs + numPlayer * numHands);

  // all cards in play
  const auto visibleCards = pokerState.GetVisibleCards(updatePlayerIdx);

  const auto &publicCards = visibleCards[numPlayer];
  const auto &evalPlayerHand = visibleCards[updatePlayerIdx];

  // update beliefs from public cards
  updateHandProbabilitiesFromSeenCards(publicCards, newHandBeliefs, numPlayer,
                                       numHands);

  // container for sampled hands
  int handIds[numPlayer];
  std::vector<std::vector<uint8_t>> sampledPrivateHands(
      numPlayer, std::vector<uint8_t>(2));

  float cumValue = 0.;

  // CFR iterations with hand resampling
  for (size_t iter = 0; iter < numIter; ++iter) {
    auto stateCopy = state.Clone();

    // sample hand for eval player first
    const int sampledEvalPlayerHandIdx =
        randomChoice(&newHandBeliefs[updatePlayerIdx * numHands],
                     &newHandBeliefs[(updatePlayerIdx + 1) * numHands]);
    std::vector<uint8_t> sampledEvalPlayerHand =
        allPossibleHands[sampledEvalPlayerHandIdx];

    // set hand id and privte hands
    handIds[updatePlayerIdx] = sampledEvalPlayerHandIdx;
    sampledPrivateHands[updatePlayerIdx] = sampledEvalPlayerHand;

    // opponents should not sample our 'true' hand
    sampledEvalPlayerHand.insert(sampledEvalPlayerHand.end(),
                                 evalPlayerHand.begin(), evalPlayerHand.end());

    auto handBeliefsInLoop = newHandBeliefs;
    updateHandProbabilitiesFromSeenCards(
        sampledEvalPlayerHand, handBeliefsInLoop, numPlayer, numHands);

    for (size_t player = 0; player < numPlayer; ++player)
      if (player != updatePlayerIdx) {
        const int newHandIdx =
            randomChoice(&handBeliefsInLoop[player * numHands],
                         &handBeliefsInLoop[(player + 1) * numHands]);

        // set hand id and privte hands
        handIds[player] = newHandIdx;
        sampledPrivateHands[player] = allPossibleHands[newHandIdx];
        // update beliefs st we dont resamle the same cards for the other
        // players
        updateHandProbabilitiesFromSeenCards(sampledPrivateHands[player],
                                             handBeliefsInLoop, numPlayer,
                                             numHands);
      }

    stateCopy->SetPartialGameState(sampledPrivateHands);
    for (size_t player = 0; player < numPlayer; ++player) {
      cumValue += cfr(player, time, pruneThreshold, true, handIds, numPlayer,
                      *stateCopy, currentStage, sharedRegret, nSharedRegret,
                      sharedStrategy, nSharedStrat, sharedStrategyFrozen,
                      nSharedFrozenStrat);
    }
  }

  // return average value
  return cumValue / (float)numIter;
}

// Multiply array elements by factor
void discount(const float factor, int *sharedRegret, float *sharedStrategy,
              const size_t N) {
  assert(factor > 0.);
  assert(factor <= 1.);

  for (size_t idx = 0; idx < N; ++idx)
    sharedRegret[idx] *= factor;

  for (size_t idx = 0; idx < N; ++idx)
    sharedStrategy[idx] *= factor;
}

// Multiply array elements by factor
void update_strategy(const int *sharedRegret, float *sharedStrategy, const size_t N) {
  std::array<int, 9> regrets;
  std::array<float, 9> probabilities;
  
  printf("[update_strategy] call update_strategy %zu\n", N);
  // TODO: can be optimized, we only need to search preflop indices (DW)
  for  (size_t idx = 0; idx < N; idx+= 9)
  {  
     // Init arrays
     std::fill(probabilities.begin(), probabilities.end(), 0);	  
     std::copy(&sharedRegret[idx], &sharedRegret[idx+9], regrets.begin());
 
     // Find legal actions (non zeros)    
     std::vector<int> legalActions;
     legalActions.reserve(9);
     for (int actionIdx = 0; actionIdx < 9; ++actionIdx)
     {
       if(regrets[actionIdx] != 0)
         legalActions.push_back(actionIdx);
     }
     printVec("[update_strategy] legalActions", legalActions.begin(), legalActions.end());
     calculateProbabilities(regrets, legalActions, probabilities);
     printVec("[update_strategy] probabilities", probabilities.begin(), probabilities.end());
     
     // Udpate shared strategy
     for (auto action : legalActions)
        sharedStrategy[idx+action] += probabilities[action];
     printVec("[update_strategy] sharedStrategy", &sharedStrategy[idx], &sharedStrategy[idx+9]);
  }
}


// Load all json files to a cpp map
void loadBuckets() {
  printf("[algorithms] loading preflop buckets..\t");
  fflush(stdout);
  readDictionaryFromJson("./lut_200/pre_flop.txt", preflopBucket);
  printf("DONE!\n[algorithms] loading flop buckets..\t");
  fflush(stdout);
  readDictionaryFromJson("./lut_200/flop.txt", flopBucket);
  printf("DONE!\n[algorithms] loading turn buckets..\t");
  fflush(stdout);
  readDictionaryFromJson("./lut_200/turn.txt", turnBucket);
  printf("DONE!\n[algorithms] loading river buckets..\t");
  fflush(stdout);
  readDictionaryFromJson("./lut_200/river.txt", riverBucket);
  printf("DONE!\n");
}

//# use lossless abstraction for all states in current stage
// if(len(handIDs) != 0 and stage == currentStage):
// arrayPos = get_array_pos(info_set, handIDs[player])
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

size_t getCardBucket(const std::array<int, 2> &privateCards,
                     const std::array<int, 5> &publicCards,
                     size_t bettingStage) {

#ifdef FAKEDICT
  return std::rand() % 150;
#endif

  assert(preflopBucket.size() > 0);
  assert(flopBucket.size() > 0);
  assert(turnBucket.size() > 0);
  assert(riverBucket.size() > 0);
  const size_t numPublicCards = bettingStage > 0 ? bettingStage + 2 : 0;

  size_t bucket = 0;

  try {
    if (bettingStage == 0) {
      char str[20];
      if (privateCards[0] < privateCards[1])
        sprintf(str, "%d,%d", privateCards[0], privateCards[1]);
      else
        sprintf(str, "%d,%d", privateCards[1], privateCards[0]);

      bucket = preflopBucket.at(str);
    } else {
      std::vector<int> abstraction =
          getCardAbstraction(privateCards, publicCards, bettingStage);
      std::stringstream abstractionStrStream;
      std::copy(abstraction.begin(), abstraction.end(),
                std::ostream_iterator<int>(abstractionStrStream, ","));
      if (bettingStage == 1)
        bucket = flopBucket.at(abstractionStrStream.str());
      else if (bettingStage == 2)
        bucket = turnBucket.at(abstractionStrStream.str());
      else
        bucket = riverBucket.at(abstractionStrStream.str());
    }
  } catch (const std::out_of_range &e) {
    printf("[algorithms] Cardbucket not found!\n");
    printf("[algorithms] Betting stage %zu\n", bettingStage);
    printVec("privateCards", privateCards.begin(), privateCards.end());
    printVec("publicCards", publicCards.begin(), publicCards.end());
    abort();
  }

  return bucket;
}

size_t cfr_array_index(int updatePlayerIdx, const int time,
                       const float pruneThreshold, const bool useRealTimeSearch,
                       const int *handIds, const size_t handIdsSize,
                       const open_spiel::State &state, const int currentStage,
                       int *sharedRegret, const size_t nSharedRegret,
                       float *sharedStrategy, const size_t nSharedStrat,
                       const float *sharedStrategyFrozen,
                       const size_t nSharedFrozenStrat) {

  const bool isTerminal = state.IsTerminal();

  assert(isTerminal == false);
  assert(state.IsChanceNode() == false);

  // Define work variables
  std::array<int, 2> privateCards{-1, -1};
  std::array<int, 5> publicCards{-1, -1, -1, -1, -1};
  std::array<int, 3> bets{0, 0, 0};

  const int currentPlayer = state.CurrentPlayer();

  // Retrieve information state
  std::string informationState = state.InformationStateString(currentPlayer);

  // Read betting stage
  const size_t bettingStage = informationState[7] - 48; // 0-4
  assert(bettingStage < 4);
  assert(bettingStage >=
         0); // Jonathan: at the moment we use RTS only from flop

  // Split of information state string
  const auto informationStateSplit = split(informationState, "][");
  // const auto informationStateSplit = split(informationState, "\\]\\[");

  // Bets of players
  std::fill(bets.begin(), bets.end(), 0);
  getBets(informationStateSplit[3], bets);

  // Retrieve pot information
  int maxBet = 0;
  int minBet = TOTALSTACK;
  int totalPot = 0;
  for (int bet : bets) {
    if (bet > maxBet)
      maxBet = bet;
    if (bet < minBet)
      minBet = bet;
    totalPot += bet;
  }

  const int currentBet = bets[currentPlayer];
  const int callSize = maxBet - currentBet;

  // Find active players code
  int activePlayersCode = 0; // all active

  // If someone folded find out which player
  if (informationStateSplit[6].find("f") != std::string::npos) {
    if (bets[(currentPlayer + 1) % 3] > bets[(currentPlayer + 2) % 3]) {
      activePlayersCode = 1;
    } else {
      activePlayersCode = 2;
    }
  }

  const auto roundActions = split(informationStateSplit[6], "|");
  std::string currentRoundActions = roundActions[roundActions.size() - 1];

  // Check if someone reraised
  const bool isReraise = std::count(currentRoundActions.begin(),
                                    currentRoundActions.end(), 'r') > 1;

  // Get legal actions provided by the game
  auto gameLegalActions = state.LegalActions();
  std::sort(gameLegalActions.begin(), gameLegalActions.end());
  // printVec("gameLegalActions", gameLegalActions.begin(),
  // gameLegalActions.end()); printf("totalPot %d maxBet %d currentBet %d\n",
  // totalPot, maxBet, currentBet);
  // Calculate our legal actions based on abstraction
  const auto ourLegalActions = getLegalActions(
      bettingStage, totalPot, maxBet, currentBet, isReraise, gameLegalActions);

  assert(ourLegalActions.size() > 0);
  // printVec("ourLegalActions", ourLegalActions.begin(),
  // ourLegalActions.end());

  const int legalActionsCode =
      getLegalActionCode(isReraise, bettingStage, ourLegalActions);
  // printf("ir %d bs %d lac %d\n", isReraise, bettingStage, legalActionsCode);

  // Call size in 10% of total stack size (values from 0 to 9)
  const int chipsToCallFrac = std::min(callSize / 50, 9);

  // Current bet in 10% of total stack size (values from 0 to 9)
  const int betSizeFrac = std::min(currentBet / 50, 9);

  // Init array index
  size_t arrayIndex = 0;

  // Get index in strategy array
  // we only use lossless hand card abstraction in current betting round
  // and only during realtime search
  // and RTS only starts after the preflop round
  if (useRealTimeSearch && (bettingStage == currentStage) &&
      bettingStage != 0) {
    assert(bettingStage >= 0);
    assert(handIdsSize == 3);

    printf(
        "[algorithms] hId %d bs %zu apc %d ctcf %d bsf %d lac %d cp %d ir %d\n",
        handIds[currentPlayer], bettingStage, activePlayersCode,
        chipsToCallFrac, betSizeFrac, legalActionsCode, currentPlayer,
        isReraise);

    arrayIndex =
        getArrayIndex(handIds[currentPlayer], bettingStage, activePlayersCode,
                      chipsToCallFrac, betSizeFrac, currentPlayer,
                      legalActionsCode, isReraise, true);
    assert(arrayIndex < nSharedStrat);
    assert(arrayIndex < nSharedFrozenStrat);
  } else {
    // Prepare private cards string
    const auto privateCardsSplit = split(informationStateSplit[4], ": ");
    const auto privateCardsStr = privateCardsSplit[1];
    assert(privateCardsStr.size() == 4);

    // Read private cards
    privateCards[0] =
        getCardCode(privateCardsStr[2], privateCardsStr[3]); // lo card
    privateCards[1] =
        getCardCode(privateCardsStr[0], privateCardsStr[1]); // hi card

    assert(privateCards[0] != privateCards[1]);

    // Process public cards if flop or later
    if (bettingStage > 0) {
      // Prepare public cards string
      const auto publicCardSplit = split(informationStateSplit[5], ": ");
      const auto publicCardsStr = publicCardSplit[1];
      assert(publicCardsStr.size() % 2 == 0);
      assert(publicCardsStr.size() == (2 + bettingStage) * 2);

      // Read public cards
      const size_t numPublicCards = bettingStage + 2;
      assert(numPublicCards == publicCardsStr.size() / 2);

      for (size_t idx = 0; idx < numPublicCards; ++idx) {
        publicCards[idx] =
            getCardCode(publicCardsStr[2 * idx], publicCardsStr[2 * idx + 1]);
      }
    }

    // Get card bucket based on abstraction
    const size_t bucket =
        getCardBucket(privateCards, publicCards, bettingStage);

    printf(
        "[algorithms] hId %zu bs %zu apc %d ctcf %d bsf %d lac %d cp %d ir %d\n",
        bucket, bettingStage, activePlayersCode, chipsToCallFrac, betSizeFrac,
        legalActionsCode, currentPlayer, isReraise);

    arrayIndex = getArrayIndex(bucket, bettingStage, activePlayersCode,
                               chipsToCallFrac, betSizeFrac, currentPlayer,
                               legalActionsCode, isReraise, false);

    assert(arrayIndex < nSharedStrat);
    assert(arrayIndex < nSharedFrozenStrat);
  }

  return arrayIndex;
}

} // namespace extensions
