#include "open_spiel/extensions/algorithms.h"
#include "open_spiel/extensions/belief.h"
#include "open_spiel/extensions/global_variables.h"
#include "open_spiel/extensions/poker_methods.h"
#include "open_spiel/games/universal_poker.h"
#include <iterator>
#include <iostream>
#include <regex>

#include <chrono>
using namespace std::chrono;

namespace extensions {

float multi_cfr(int numIter, const int updatePlayerIdx, const int startTime,
                const float pruneThreshold, const bool useRealTimeSearch,
                const int *handIds, const size_t handIdsSize,
                const open_spiel::State &state, const int currentStage,
                int *sharedRegret, float *sharedStrategy, const float *sharedStrategyFrozen,
                const size_t N) {
  float cumValue = 0;
  for (int iter = 0; iter < numIter; iter++) {
    cumValue += cfr(updatePlayerIdx, startTime, pruneThreshold, useRealTimeSearch,
                    handIds, handIdsSize, state, currentStage, sharedRegret,
                    sharedStrategy, sharedStrategyFrozen, N);
  }
  return cumValue / (float)numIter;
}

float cfr(int updatePlayerIdx, const int time, const float pruneThreshold,
          const bool useRealTimeSearch, 
          const int *handIds, const size_t handIdsSize, 
          const open_spiel::State &state, const int currentStage, 
          int *sharedRegret, float *sharedStrategy, const float *sharedStrategyFrozen, const size_t N) {

  assert(time > 0);

  
  if ( (useRealTimeSearch == true) && (maxValuesProdRTS.back()*9 != N) )
  {
	fprintf(stderr, "[algorithms] rts array length mismatch (is %zu should be %zu)\n", N, maxValuesProdRTS.back()*9);
  	assert ( maxValuesProdRTS.back()*9 == N );
  }
  else if ( (useRealTimeSearch == false) && (maxValuesProd.back()*9 != N) )
  {
	fprintf(stderr, "[algorithms] array length mismatch (is %zu should be %zu)\n", N, maxValuesProd.back()*9);
  	assert ( maxValuesProd.back()*9 == N );
  }

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
    assert(sampledAction >= 0);
    const auto new_state = state.Child(sampledAction);

    return cfr(updatePlayerIdx, time, pruneThreshold, useRealTimeSearch,
               handIds, handIdsSize, *new_state, currentStage, sharedRegret,
               sharedStrategy, sharedStrategyFrozen, N);
  }

  // Define work variables
  std::array<int, 2> privateCards{-1, -1};
  std::array<int, 5> publicCards{-1, -1, -1, -1, -1};
  std::array<int, 3> bets{0, 0, 0};

  std::array<bool, 9> explored{true, true, true, true, true,
                               true, true, true, true};
  std::array<float, 9> strategy{0., 0., 0., 0., 0., 0., 0., 0., 0.};
  std::array<float, 9> probabilities{0., 0., 0., 0., 0., 0., 0., 0., 0.};

  const int currentPlayer = state.CurrentPlayer();

  // Retrieve information state
  std::string informationState = state.InformationStateString(currentPlayer);
  printf("iss %s\n", informationState.c_str());

  // Read betting stage
  const size_t bettingStage = informationState[7] - 48; // 0-4

  assert(bettingStage < 4);
  assert(bettingStage >= 0); // Jonathan: at the moment we use RTS only from flop

  // Split of information state string
  // const auto informationStateSplit = split(informationState, "\\]\\[");
  const auto informationStateSplit = split(informationState, "][");
  
  //std::array<int, 3> stack{0, 0, 0};
  //getStack(informationStateSplit[3], stack);

  // Bets of players
  std::fill(bets.begin(), bets.end(), 0);
  getBets(informationStateSplit[3], bets);

  // Retrieve pot information
  int maxBet = 0;
  int minBet = TOTALSTACK[currentPlayer];
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
  const auto ourLegalActions = getLegalActions(bettingStage, totalPot, maxBet, currentBet, isReraise, gameLegalActions);

  assert(ourLegalActions.size() > 0);
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
    assert(bettingStage > 0);
    assert(handIdsSize == 3);
    arrayIndex =
        getArrayIndex(handIds[currentPlayer], bettingStage, activePlayersCode,
                      chipsToCallFrac, betSizeFrac, currentPlayer,
                      legalActionsCode, isReraise, useRealTimeSearch);
    //assert(arrayIndex < N);
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
  }

  if (currentPlayer == updatePlayerIdx) {
    if (useRealTimeSearch) {
      std::copy(&sharedStrategyFrozen[arrayIndex],
      	&sharedStrategyFrozen[arrayIndex + 9], strategy.begin());


      bool allZero = true;
      for (float actionProb : strategy)
        if (actionProb != 0.) {
          allZero = false;
          break;
        }

      // if all entries zero, take regrets from passed trained strategy
      if (allZero) {
	assert(arrayIndex < N);
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
                  sharedStrategy, sharedStrategyFrozen, N);
          expectedValue += actionValue * probabilities[action];
        }
        return expectedValue;
      }
    } else {
      assert(arrayIndex < N);
    }

    calculateProbabilities(sharedRegret+arrayIndex, ourLegalActions, probabilities.begin());

    // Find actions to prune
    if (applyPruning == true && bettingStage < 3) {
      for (const int action : ourLegalActions) {
        if (sharedRegret[arrayIndex+action] < pruneThreshold)
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
                sharedStrategy, sharedStrategyFrozen, N);
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
	assert(arrayActionIndex < N);
        sharedRegret[arrayActionIndex] +=
            int(multiplier * (actionValues[action] - expectedValue));

        if (sharedRegret[arrayActionIndex] < pruneThreshold * 1.03)
          sharedRegret[arrayActionIndex] = pruneThreshold * 1.03;
        if (sharedRegret[arrayActionIndex] > (int) (std::numeric_limits<int>::max() * 0.95))
          sharedRegret[arrayActionIndex] = (int) (std::numeric_limits<int>::max() * 0.95);
      }

    return expectedValue;
  } else {
    // Calculate probabilities from regrets
    calculateProbabilities(sharedRegret+arrayIndex, ourLegalActions, probabilities.begin());

    // randomChoice returns a value of 0 to 8
    const int sampledAction =
        randomChoice(probabilities.begin(), probabilities.end());
    const size_t absoluteAction =
        actionToAbsolute(sampledAction, maxBet, totalPot, gameLegalActions);
    auto new_state = state.Child(absoluteAction);

    // Update shared strategy
    if( currentPlayer == (updatePlayerIdx + 1)%3 )
    {
      const float multiplier = std::min(time, 32768);
      for (const int action : ourLegalActions) {
        const size_t arrayActionIndex = arrayIndex + action;
        assert(arrayActionIndex < N);
        sharedStrategy[arrayActionIndex] += multiplier * probabilities[action];
      }
    }

    return cfr(updatePlayerIdx, time, pruneThreshold, useRealTimeSearch, handIds,
        handIdsSize, *new_state, currentStage, sharedRegret,
        sharedStrategy, sharedStrategyFrozen, N);
  }
}

float cfr_realtime(const int numIter, const int updatePlayerIdx, const int time,
                   const float pruneThreshold, const open_spiel::State &state, 
                   const float *handBeliefs, const size_t numPlayer,
                   const size_t numHands, const int currentStage,
                   int *sharedRegret,
                   float *sharedStrategy,
                   const float *sharedStrategyFrozen,
                   const size_t N) {
  assert(currentStage > 0);

  auto start_init = high_resolution_clock::now();

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

  auto stop_init = high_resolution_clock::now();
  
  auto start_beliefs = high_resolution_clock::now();
  
  // update beliefs from public cards
  updateHandProbabilitiesFromSeenCards(publicCards, newHandBeliefs, numPlayer,
                                       numHands);

  auto stop_beliefs = high_resolution_clock::now();
  
  // container for sampled hands
  int handIds[numPlayer];
  std::vector<std::vector<uint8_t>> sampledPrivateHands(
      numPlayer, std::vector<uint8_t>(2));

  float cumValue = 0.;
  

  size_t time_clone = 0;
  size_t time_sample = 0;
  size_t time_cfr = 0;

  // CFR iterations with hand resampling
  for (size_t iter = 0; iter < numIter; ++iter) {
  
    auto start_clone = high_resolution_clock::now();
    
    auto stateCopy = state.Clone();
    
    auto stop_clone = high_resolution_clock::now();

    auto start_sample = high_resolution_clock::now();
    
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
    
    auto stop_sample = high_resolution_clock::now();
    auto start_cfr = high_resolution_clock::now();
    
    for (size_t player = 0; player < numPlayer; ++player) {
      cumValue += cfr(player, time, pruneThreshold, true, handIds, numPlayer,
                      *stateCopy, currentStage, sharedRegret,
                      sharedStrategy, sharedStrategyFrozen,
                      N);
    }
    auto stop_cfr = high_resolution_clock::now();

    time_sample += duration_cast<nanoseconds>(stop_sample - start_sample).count();
    time_clone += duration_cast<nanoseconds>(stop_clone - start_clone).count();
    time_cfr += duration_cast<nanoseconds>(stop_cfr - start_cfr).count();
  }

  size_t time_init = duration_cast<nanoseconds>(stop_init - start_init).count();
  size_t time_beliefs = duration_cast<nanoseconds>(stop_beliefs - start_beliefs).count();

  printf("[algorithms] time init %zu ns\n", time_init);
  printf("[algorithms] time beliefs %zu ns\n", time_beliefs);
  printf("[algorithms] time sample %zu ns\n", time_sample);
  printf("[algorithms] time clone %zu ns\n", time_clone);
  printf("[algorithms] time cfr %zu ns\n", time_cfr);

  // return average value
  return cumValue / (float)numIter;
}

// Multiply array elements by factor
void discount(const float factor, int *sharedRegret, float *sharedStrategy, float *sharedStrategyDiscrete,
              const size_t N) {
  
  assert(factor > 0.);
  assert(factor <= 1.);

  if ( maxValuesProd.back()*9 != N )
  {
	fprintf(stderr, "[algorithms] array length mismatch (is %zu should be %zu)\n", N, maxValuesProd.back()*9);
  	assert ( maxValuesProd.back()*9 == N );
  }

 
  for (size_t idx = 0; idx < N; ++idx)
    sharedRegret[idx] *= factor;

  for (size_t idx = 0; idx < N; ++idx)
    sharedStrategy[idx] *= factor;

  for (size_t idx = 0; idx < N; ++idx)
    sharedStrategyDiscrete[idx] *= factor;

}

// Multiply array elements by factor
void update_strategy(const int *sharedRegret, float *sharedStrategy, const size_t N) {
  std::array<float, 9> probabilities;

  //std::array<float, 9> sharedStrategyLocal;
  //std::array<float, 9> sharedProbabilitiesBefore;
  //std::array<float, 9> sharedProbabilitiesAfter;
  
  if ( maxValuesProd.back()*9 != N )
  {
	fprintf(stderr, "[algorithms] array length mismatch (is %zu should be %zu)\n", N, maxValuesProd.back()*9);
  	assert ( maxValuesProd.back()*9 == N );
  }

  //float klDivergence = 0.;

  for  (size_t idx = 0; idx < N; idx+= 9)
  {  
     size_t segment = idx / GLOBAL_NUM_BUCKETS;
     if (segment % 4 == 0)
     {
       // Init arrays
       std::fill(probabilities.begin(), probabilities.end(), 0.);	  
 
       // Find legal actions (non zeros)    
       std::vector<int> legalActions;
       legalActions.reserve(9);
       for (int actionIdx = 0; actionIdx < 9; ++actionIdx)
       {
         if(sharedRegret[idx+actionIdx] != 0)
           legalActions.push_back(actionIdx);
       }
       calculateProbabilities(sharedRegret+idx, legalActions, probabilities.begin());
     
       // Udpate shared strategy
       for (auto action : legalActions)
         sharedStrategy[idx+action] += probabilities[action];
       
       // Calculate shared probabilities after update
       //std::copy(&sharedStrategy[idx], &sharedStrategy[idx+9], sharedStrategyLocal.begin());
       //calculateProbabilities(sharedStrategyLocal, legalActions, sharedProbabilitiesAfter);
 
       // Calculate KL divergence
       //for (auto action : legalActions)
       //klDivergence += sharedProbabilitiesBefore[idx+action] * std::log2(sharedProbabilitiesBefore[idx+action]/sharedProbabilitiesAfter[idx+action]);
     }
  }
  
  //printf("[algorithms] KL-divergence statistics of strategy update: %f\t", klDivergence);
}


// Load all json files to a cpp map
void loadBuckets(const std::string& lutPath) {
  printf("[algorithms] loading preflop buckets..\t");
  fflush(stdout);
  readDictionaryFromJson(lutPath + "/pre_flop.txt", preflopBucket);
  printf("DONE!\n[algorithms] loading flop buckets..\t");
  fflush(stdout);
  readDictionaryFromJson(lutPath + "/flop.txt", flopBucket);
  printf("DONE!\n[algorithms] loading turn buckets..\t");
  fflush(stdout);
  readDictionaryFromJson(lutPath + "/turn.txt", turnBucket);
  printf("DONE!\n[algorithms] loading river buckets..\t");
  fflush(stdout);
  readDictionaryFromJson(lutPath + "/river.txt", riverBucket);
  printf("DONE!\n");
}
  
// Load file to a cpp map
void loadTurnPerFlopBuckets(const std::string& lutPath) {
  printf("\n[algorithms] loading per-flop turn buckets from ..\t");
  //printf(lutPath); // no known conversion
  printf("/turn_per_flop_emd.txt");
  fflush(stdout);
  readDeepDictionaryFromJson(lutPath + "/turn_per_flop_emd.txt", turnBucketPerFlop);
  printf("DONE!\n");
}

void setTurnBuckets(const std::string& flopAbstraction) {
  printf("\nSetting turn buckets");
  turnBucket = turnBucketPerFlop[flopAbstraction];
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
  std::string abstractionString;

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
      abstractionString = abstractionStrStream.str();
      abstractionString.pop_back(); // remove trailing comma

      if (bettingStage == 1)
        bucket = flopBucket.at(abstractionString);
      else if (bettingStage == 2)
        bucket = turnBucket.at(abstractionString);
      else
        bucket = riverBucket.at(abstractionString);
    }
  } catch (const std::out_of_range &e) {
    printf("[algorithms] Cardbucket not found!\n");
    printf("[algorithms] Key %s\n", abstractionString.c_str());
    printf("[algorithms] Betting stage %zu\n", bettingStage);
    printVec("privateCards", privateCards.begin(), privateCards.end());
    printVec("publicCards", publicCards.begin(), publicCards.end());
    abort();
  }

  return bucket;
}

void setStacks(const std::array<int, 3> &stacks){
  TOTALSTACK[0] = stacks[0];
  TOTALSTACK[1] = stacks[1];
  TOTALSTACK[2] = stacks[2];
}

size_t cfr_array_index(int updatePlayerIdx, const int time,
                       const float pruneThreshold, const bool useRealTimeSearch,
                       const int *handIds, const size_t handIdsSize,
                       const open_spiel::State &state, const int currentStage,
                       int *sharedRegret,
                       float *sharedStrategy,
                       const float *sharedStrategyFrozen,
                       const size_t N) {

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
  std::cout << informationState << std::endl;

  // Read betting stage
  const size_t bettingStage = informationState[7] - 48; // 0-4
  assert(bettingStage < 4);
  assert(bettingStage >= 0); // Jonathan: at the moment we use RTS only from flop

  // Split of information state string
  const auto informationStateSplit = split(informationState, "][");
  // const auto informationStateSplit = split(informationState, "\\]\\[");

  // Bets of players
  std::fill(bets.begin(), bets.end(), 0);
  getBets(informationStateSplit[3], bets);

  // Retrieve pot information
  int maxBet = 0;
  int minBet = TOTALSTACK[currentPlayer];
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

  }

  return arrayIndex;
}

} // namespace extensions
