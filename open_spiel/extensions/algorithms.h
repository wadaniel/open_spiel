#ifndef _ALGORITHMS_H_
#define _ALGORITHMS_H_

#include "open_spiel/spiel.h"

int test_sum(int a, int b);

int test_cfr(int idx, float val, float* sharedStrategy);

float cfr(int updatePlayerIdx, bool useRealTimeSearch, std::unique_ptr<open_spiel::State> state, float* sharedStrategy);

#endif // _ALGORITHMS_H_
