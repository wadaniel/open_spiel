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
* @param updatePlayerIdx
* @param useRealTimeSearch
* @param state
* @param shardStrategy TODO
* @param shardStrategyFrozen TODO
* @return The expected value of the CFR
*/
float cfr(int updatePlayerIdx, bool useRealTimeSearch, std::unique_ptr<open_spiel::State> state, float* sharedStrategy, float* sharedStrategyFrozen = nullptr);

}

#endif // _ALGORITHMS_H_
