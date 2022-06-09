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


struct cfrMemory
{
	std::array<float, 9> actionValues;
	std::array<float, 9> probabilities;
	std::array<bool, 9> explored;
	std::array<int, 3> bets;
	std::array<int, 2> privateCards;
	std::array<int, 5> publicCards;
	std::array<float, 9> regrets;
};

/**
* @brief CFR calculation and updated of shared strategy
* @param updatePlayerIdx Index of the player to update
* @param time The training iteration
* @param pruneThreshold Threshold to skip cfr updates
* @param useRealTimeSearch Set true durin real time search
* @param handIds Pointer to hand IDs
* @param handIdsSize The length of the handIds vector
* @param state State of the pyspiel
* @param shardStrategy The shared strategy
* @param shardStrategyFrozen The frozen constant backup strategy, only used for RTS
* @return The expected value of the CFR
*/
float cfr(int updatePlayerIdx, int time, float pruneThreshold, bool useRealTimeSearch, int* handIds, size_t handIdsSize, const open_spiel::State& state, float* sharedStrategy, size_t nSharedStrat, const float* sharedStrategyFrozen, size_t nSharedFrozenStrat, cfrMemory& work);

}

#endif // _ALGORITHMS_H_
