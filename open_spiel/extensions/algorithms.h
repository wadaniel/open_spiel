#ifndef _ALGORITHMS_H_
#define _ALGORITHMS_H_

#include "open_spiel/spiel.h"

/*
 * Inputs
 * @a: The first summand
 * @b: The second summand
 *
 * Returns
 * @test_sum: the sum of a and b
 */
int test_sum(int a, int b);

int test_cfr(int idx, float val, float* sharedStrategy);


/*
 * Inputs
 * @updatePlayerIdx:
 * @useRealTimeSearch:
 * @state:
 * @sharedStrategy:
 * @sharedStrategyFrozen
 *
 * Returns
 * @cfr: expected value of current state
 */
float cfr(int updatePlayerIdx, bool useRealTimeSearch, std::unique_ptr<open_spiel::State> state, float* sharedStrategy, float* sharedStrategyFrozen = nullptr);

#endif // _ALGORITHMS_H_
