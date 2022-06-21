#ifndef _ALGORITHMS_H_
#define _ALGORITHMS_H_

#include "open_spiel/spiel.h"

namespace extensions
{

/**
* @brief Calculates the sum of two numbers.
* @param a The first summand
* @param b The second summand
* @return The sum
*/
int test_sum(int a, int b);

/**
* @brief Randomly modifies the values in sharedStrategy.
* @param idx The highes index to modify
* @param idx The vale we set
* @param shardStrategy The array to modify
* @return The val
*/
int test_cfr(int idx, float val, float* sharedStrategy);

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
* @param sharedStrategyFrozen The frozen constant backup strategy, only used for RTS
* @return The expected value of the CFR
*/
float cfr(int updatePlayerIdx, int time, float pruneThreshold, bool useRealTimeSearch, int* handIds, size_t handIdsSize, const open_spiel::State& state, int currentStage, int* sharedRegret, size_t nSharedRegret, float* sharedStrategy, size_t nSharedStrat, float* sharedStrategyFrozen, size_t nSharedFrozenStrat); 

float multi_cfr(int numIter, int updatePlayerIdx, int startTime, float pruneThreshold, bool useRealTimeSearch, int* handIds, size_t handIdsSize, const open_spiel::State& state, int currentStage, int* sharedRegret, size_t nSharedRegret, float* sharedStrategy, size_t nSharedStrat, float* sharedStrategyFrozen, size_t nSharedFrozenStrat);


}

#endif // _ALGORITHMS_H_
