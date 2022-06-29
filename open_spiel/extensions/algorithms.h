#ifndef _ALGORITHMS_H_
#define _ALGORITHMS_H_

#include "open_spiel/spiel.h"

namespace extensions {

/**
 * @brief CFR calculation and updated of shared strategy
 * @param updatePlayerIdx Index of the player to update
 * @param time The training iteration
 * @param pruneThreshold Threshold to skip cfr updates
 * @param useRealTimeSearch Set true durin real time search
 * @param handIds Pointer to hand IDs
 * @param handIdsSize The length of the handIds vector
 * @param state State of the pyspiel
 * @param currentStage State of the pyspiel
 * @param sharedRegret State of the pyspiel
 * @param sharedStrategy The shared strategy
 * @param sharedStrategyFrozen The frozen constant backup strategy, only used
 * for RTS
 * @return The expected value of the CFR
 */
float cfr(int updatePlayerIdx, const int time, const float pruneThreshold,
          const bool useRealTimeSearch, const int *handIds,
          const size_t handIdsSize, const open_spiel::State &state,
          const int currentStage, int *sharedRegret, const size_t nSharedRegret,
          float *sharedStrategy, const size_t nSharedStrat,
          const float *sharedStrategyFrozen, const size_t nSharedFrozenStrat);

/**
 * @brief Multiple CFR calculation and updated of shared strategy
 * @param numIter Number of repeated cfr calls
 * @param updatePlayerIdx Index of the player to update
 * @param time The training iteration
 * @param pruneThreshold Threshold to skip cfr updates
 * @param useRealTimeSearch Set true durin real time search
 * @param handIds Pointer to hand IDs
 * @param handIdsSize The length of the handIds vector
 * @param state State of the pyspiel
 * @param currentStage State of the pyspiel
 * @param sharedRegret State of the pyspiel
 * @param sharedStrategy The shared strategy
 * @param sharedStrategyFrozen The frozen constant backup strategy, only used
 * for RTS
 * @return Expected value of the CFR averaged over numIter
 */

float multi_cfr(int numIter, const int updatePlayerIdx, const int startTime,
                const float pruneThreshold, const bool useRealTimeSearch,
                const int *handIds, const size_t handIdsSize,
                const open_spiel::State &state, const int currentStage,
                int *sharedRegret, const size_t nSharedRegret,
                float *sharedStrategy, const size_t nSharedStrat,
                const float *sharedStrategyFrozen,
                const size_t nSharedFrozenStrat);

/**
 * @brief Multiple CFR runs with card sampling according to beliefs
 * @param numIter Number of CFR iterations
 * @param updatePlayerIdx Index of the player to update
 * @param time The training iteration
 * @param pruneThreshold Threshold to skip cfr updates
 * @param state State of the pyspiel
 * @param handBeliefs Arrays of hand beliefs,  for each player one array
 * @param numPlayer Number of players
 * @param numHands Length of inner dimension of handBeliefs
 * @param currentStage
 * @param sharedRegret State of the pyspiel
 * @param nSharedRegret
 * @param sharedStrategy The shared strategy
 * @param nSharedStrat
 * @param sharedStrategyFrozen The frozen constant backup strategy, only used
 * @param nSharedFrozenStrat
 * @return Expected value of the CFR averaged over numIter
 */
float cfr_realtime(const int numIter, const int updatePlayerIdx, const int time,
                  const float pruneThreshold, const open_spiel::State &state,
                  float *handBeliefs, const size_t numPlayer,
                  const size_t numHands, const int currentStage,
                  int *sharedRegret, const size_t nSharedRegret,
                  float *sharedStrategy, const size_t nSharedStrat,
                  const float *sharedStrategyFrozen,
                  const size_t nSharedFrozenStrat);

void discount(const float factor, float *sharedRegret, float *sharedStrategy,
              const size_t N);

/**
 * @brief Load all buckets and store them in global dictionary
 */
void loadBuckets();

size_t getCardBucket(const std::array<int, 2> &privateCards,
                     const std::array<int, 5> &publicCards,
                     size_t bettingStage);


} // namespace extensions

#endif // _ALGORITHMS_H_
