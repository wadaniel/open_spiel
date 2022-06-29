#include "open_spiel/extensions/algorithms.h"
#include "open_spiel/extensions/global_variables.h"
#include "open_spiel/extensions/poker_methods.h"
#include "open_spiel/extensions/belief.h"
#include "open_spiel/games/universal_poker.h"

#include <iostream>

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
    cumValue += cfr(updatePlayerIdx, iter, pruneThreshold, useRealTimeSearch,
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
  const bool isTerminal = state.IsTerminal();
  // If terminal, return players reward
  if (isTerminal) {
    float playerReward = state.PlayerReward(updatePlayerIdx);
    printf("terminal");
    return playerReward;
  }

  // If chance node, sample random outcome and reiterate cfr
  if (state.IsChanceNode()) {
    printf("ischance");
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

  // Split of information state string
  const auto informationStateSplit = split(informationState, "]");

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
  const int chipsToCallFrac =
      std::min(callSize / 50,
               9); // TODO (DW): I suggest min(10*n_chips_to_call // my_stack,9)

  // Current bet in 10% of total stack size (values from 0 to 9) //TODO (DW): I
  // suggest min(10*current_bet // total_chips_in_play, 9)
  const int betSizeFrac = std::min(currentBet / 50, 9);

  // Init array index
  size_t arrayIndex = 0;

  // Get index in strategy array
  // we only use lossless hand card abstraction in current betting round
  // and only during realtime search
  // and RTS only starts after the preflop round
  std::cout << " betting stage " << bettingStage << " - " << currentStage << std::endl;
  if (useRealTimeSearch && bettingStage == currentStage) {
    assert(bettingStage > 0);
    assert(handIdsSize == 3);
    arrayIndex =
        getArrayIndex(handIds[currentPlayer], bettingStage, activePlayersCode,
                      chipsToCallFrac, betSizeFrac, currentPlayer,
                      legalActionsCode, isReraise, true);
    std::cout << "arrayIndex " << arrayIndex << std::endl;
    assert(arrayIndex < nSharedStrat);
    assert(arrayIndex < nSharedFrozenStrat);
  } else {
    // Prepare private cards string
    const auto privateCardsSplit = split(informationStateSplit[4], ": ");
    const auto privateCardsStr = privateCardsSplit[1];
    assert(privateCardsStr.size() == 4);

    // Read private cards
    privateCards[1] =
        getCardCode(privateCardsStr[0], privateCardsStr[1]); // lo card
    privateCards[0] =
        getCardCode(privateCardsStr[2], privateCardsStr[3]); // hi card

    assert(privateCards[0] != privateCards[1]);

    // Make sure they are sorted
    // std::sort(privateCards.begin(), privateCards.end());
    assert(std::is_sorted(privateCards.begin(), privateCards.end()));

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

      // Make sure they are sorted
      std::sort(
          publicCards.begin(),
          publicCards.begin() +
              numPublicCards); // TODO (DW) is this a requirement?? ask Jonathan
      assert(std::is_sorted(publicCards.begin(),
                            publicCards.begin() + numPublicCards)); // ascending
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

    // Iterate only over explored actions
    for (const int action : ourLegalActions)
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

    // Multiplier for linear regret
    const float multiplier = 1.; // min(t, 2**10) # stop linear cfr at 32768, be
                                 // careful about overflows

    // Update active player regrets
    for (const int action : ourLegalActions)
      if (explored[action]) {
        const size_t arrayActionIndex = arrayIndex + action;
        sharedRegret[arrayActionIndex] +=
            int(multiplier * (actionValues[action] - expectedValue));
        assert(arrayActionIndex < nSharedRegret);
        if (sharedRegret[arrayActionIndex] > std::numeric_limits<int>::max())
          sharedRegret[arrayActionIndex] = std::numeric_limits<int>::max();
        if (sharedRegret[arrayActionIndex] < pruneThreshold * 1.03)
          sharedRegret[arrayActionIndex] = pruneThreshold * 1.03;
      }
    return expectedValue;
  } else {
    //  Fill regrets
    if (useRealTimeSearch) {
      std::copy(&sharedStrategyFrozen[arrayIndex],
                &sharedStrategyFrozen[arrayIndex + 9], strategy.begin());
      bool allZero = true;
      for (float actionProb : strategy)
        if (actionProb != 0.) {
          allZero = false;
          break;
        }
      if (allZero) {
        std::copy(&sharedRegret[arrayIndex], &sharedRegret[arrayIndex + 9],
                  regrets.begin());
      } else {
        std::copy(&sharedStrategyFrozen[arrayIndex],
                  &sharedStrategyFrozen[arrayIndex + 9], regrets.begin());
      }
    } else {
      std::copy(sharedRegret + arrayIndex, sharedRegret + arrayIndex + 9,
                regrets.begin());
    }

    // Calculate probabilities from regrets
    calculateProbabilities(regrets, ourLegalActions, probabilities);

    // randomChoice returns a value of 0 to 8
    const int sampledAction =
        randomChoice(probabilities.begin(), probabilities.end());
    const size_t absoluteAction =
        actionToAbsolute(sampledAction, maxBet, totalPot, gameLegalActions);
    auto new_state = state.Child(absoluteAction);
    const float expectedValue = cfr(
        updatePlayerIdx, time, pruneThreshold, useRealTimeSearch, handIds,
        handIdsSize, *new_state, currentStage, sharedRegret, nSharedRegret,
        sharedStrategy, nSharedStrat, sharedStrategyFrozen, nSharedFrozenStrat);

    // TODO(DW): update strategy mode 'opponent' (Jonathan: necessary)
    // has to be in non active player
    // Multiplier for linear regret
    const float multiplier = 1.; // min(t, 2**10) # stop linear cfr at 32768, be
                                 // careful about overflows

    // Update active *non frozen* shared strategy
    for (const int action : ourLegalActions) {
      const size_t arrayActionIndex = arrayIndex + action;
      assert(arrayActionIndex < nSharedStrat);
      sharedStrategy[arrayActionIndex] += multiplier * probabilities[action];
    }

    return expectedValue;
  }
}

float cfr_realtime(const int numIter, const int updatePlayerIdx, const int time, 
          const float pruneThreshold, const open_spiel::State &state,
          float* handBeliefs, const size_t numPlayer, const size_t numHands,
          const int currentStage, int *sharedRegret, const size_t nSharedRegret,
          float *sharedStrategy, const size_t nSharedStrat,
          const float *sharedStrategyFrozen, const size_t nSharedFrozenStrat)
{
	assert(currentStage > 0);
  assert(numPlayer > 1);

	// Fill data
    std::vector<std::vector<float>> newHandBeliefs(numPlayer, std::vector<float>(numHands));
	for(size_t player = 0; player < numPlayer; ++player){
		for(size_t idx = 0; idx < numPossibleHands; ++idx){
			newHandBeliefs[player][idx] = handBeliefs[player*numPossibleHands+idx];
    }
  }

  const open_spiel::universal_poker::UniversalPokerState& pokerState = static_cast<const open_spiel::universal_poker::UniversalPokerState&>(state) ;

	// all cards in play
  const auto visibleCards = pokerState.GetVisibleCards(updatePlayerIdx);
    
	const auto& publicCards = visibleCards[numPlayer];
    
  std::cout << " -- publicCards: ";
  printf(" %d", publicCards[0]);
  std::cout << " -- ";
  printf(" %d", publicCards[1]);
  std::cout << " -- ";
  printf(" %d", publicCards[2]);
  std::cout << std::endl;
  const auto& evalPlayerHand = visibleCards[updatePlayerIdx];
	std::cout << " -- evalPlayerHand: ";
  printf(" %d", evalPlayerHand[0]);
  std::cout << " -- ";
  printf(" %d", evalPlayerHand[1]);
  std::cout << std::endl;

	// update beliefs from public cards
	const auto tmpBeliefConst = updateHandProbabilitiesFromSeenCards(publicCards, newHandBeliefs);
    
	// container for sampled hands
  int handIds[numPlayer];
	std::vector<std::vector<uint8_t>> sampledPrivateHands(numPlayer, std::vector<uint8_t>(2));

	
	float cumValue = 0.;
    
	// CFR iterations with hand resampling
    for(size_t iter = 0; iter < numIter; ++iter)
    {
        auto stateCopy = state.Clone();

        // reinitialize beliefs after seeing public cards
		    std::vector<std::vector<float>> tmpBeliefInLoop = tmpBeliefConst;
         
		    // sample hand for eval player first    
        const int sampledEvalPlayerHandIdx = randomChoice(tmpBeliefInLoop[updatePlayerIdx].begin(), tmpBeliefInLoop[updatePlayerIdx].end());
        std::vector<uint8_t> sampledEvalPlayerHand = allPossibleHands[sampledEvalPlayerHandIdx];
        std::cout << " -- handIdx: " << sampledEvalPlayerHandIdx << " -- hand cards: ";
        printf(" %d", sampledEvalPlayerHand[0]);
        std::cout << "/";
        printf(" %d", sampledEvalPlayerHand[1]);
        std::cout << std::endl;
		
		    // set hand id and privte hands
      	handIds[updatePlayerIdx] = sampledEvalPlayerHandIdx;
        sampledPrivateHands[updatePlayerIdx] = sampledEvalPlayerHand;

        // opponents should not sample our 'true' hand
        sampledEvalPlayerHand.insert(sampledEvalPlayerHand.end(), evalPlayerHand.begin(), evalPlayerHand.end());
        tmpBeliefInLoop = updateHandProbabilitiesFromSeenCards(sampledEvalPlayerHand, tmpBeliefInLoop);
 
        printf(" %d", numPlayer);
        for(size_t player = 0; player < numPlayer; ++player) if(player != updatePlayerIdx)
        {
      		const int newHandIdx = randomChoice(tmpBeliefInLoop[player].begin(), tmpBeliefInLoop[player].end());
                
          // set hand id and privte hands
          handIds[player] = newHandIdx;
          sampledPrivateHands[player] = allPossibleHands[newHandIdx];
          // update beliefs st we dont resamle the same cards for the other players
          tmpBeliefInLoop = updateHandProbabilitiesFromSeenCards(sampledPrivateHands[player], tmpBeliefInLoop);
        }
        
        stateCopy->SetPartialGameState(sampledPrivateHands);
        for(size_t player = 0; player < numPlayer; ++player)
        {
            printf("cfr with player %d", player);
            cumValue += cfr(player, time, pruneThreshold, true, handIds, numPlayer, 
                  *stateCopy, currentStage, sharedRegret,
                  nSharedRegret, sharedStrategy, nSharedStrat,
                  sharedStrategyFrozen, nSharedFrozenStrat);
        }
    }

	// return average value
	return cumValue / (float) numIter;
}


// Multiply array elements by factor
void discount(const float factor, float *sharedRegret, float *sharedStrategy,
              const size_t N) {

  for (size_t idx = 0; idx < N; ++idx)
    sharedRegret[idx] *= factor;

  for (size_t idx = 0; idx < N; ++idx)
    sharedStrategy[idx] *= factor;
}

// Load all json files to a cpp map
void loadBuckets() {
  printf("loading preflop buckets..\t");
  fflush(stdout);
  readDictionaryFromJson("./lut_200/pre_flop.txt", preflopBucket);
  printf("DONE!\nloading flop buckets..\t\t");
  fflush(stdout);
  readDictionaryFromJson("./lut_200/flop.txt", flopBucket);
  printf("DONE!\nloading turn buckets..\t\t");
  fflush(stdout);
  readDictionaryFromJson("./lut_200/turn.txt", turnBucket);
  printf("DONE!\nloading river buckets..\t\t");
  fflush(stdout);
  readDictionaryFromJson("./lut_200/river.txt", riverBucket);
  printf("DONE!\n");
}

} // namespace extensions
