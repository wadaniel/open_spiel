#ifndef _GLOBAL_VARIABLES_H_
#define _GLOBAL_VARIABLES_H_

#include "utils.h"
#include <map>

#define TOTALSTACK 500
#define BBSIZE 20

#define NUM_BUCKETS 200
#define NUM_RTS_BUCKETS 1326

namespace extensions {

const bool applyPruning = true;

std::map<std::string, size_t> preflopBucket;
std::map<std::string, size_t> flopBucket;
std::map<std::string, size_t> turnBucket;
std::map<std::string, size_t> riverBucket;

// 14 legal actions
const std::vector<std::vector<int>> allLegalActions = {
    {0, 1},
    {0, 1, 8},
    {0, 1, 4, 8},
    {0, 1, 4, 5, 8},
    {0, 1, 4, 5, 6, 8},
    {0, 1, 4, 5, 6, 7, 8},
    {0, 1, 3, 8},
    {0, 1, 3, 4, 8},
    {0, 1, 3, 4, 5, 8},
    {0, 1, 3, 4, 5, 6, 8},
    {0, 1, 3, 4, 5, 6, 7, 8},
    {0, 1, 2, 8},
    {0, 1, 2, 3, 4, 5, 6, 7, 8},
    {1, 3, 4, 5, 6, 7, 8}};

// 5 legal actions
const std::vector<std::vector<int>> allLegalReraiseActions = {
    {0, 1}, {0, 1, 8}, {0, 1, 5, 8}, {1, 5, 8}, {1, 8}};

// 9 legal actions
const std::vector<std::vector<int>> allLegalTurnRiverActions = {
    {0, 1},       {0, 1, 8}, {0, 1, 3, 8}, {0, 1, 3, 5, 8},
    {0, 1, 5, 8}, {1, 3, 8}, {1, 3, 5, 8}, {1, 8}};

// 11 legal actions
const std::vector<std::vector<int>> allLegalFlopActions = {
    {0, 1},          {0, 1, 8},      {0, 1, 3, 8},
    {0, 1, 3, 5, 8}, {0, 1, 5, 8},   {1, 3, 8},
    {1, 3, 5, 8},    {1, 8},         {0, 1, 3, 5, 6, 8},
    {0, 1, 5, 6, 8}, {1, 3, 5, 6, 8}};

// Create a map for hash to index of legal actions vector
std::map<size_t, int>
createActionsToIndexMap(const std::vector<std::vector<int>> &actionsVector) {
  std::map<size_t, int> actionsMap;
  for (size_t i = 0; i < actionsVector.size(); ++i) {
    size_t hashValue = vecHash(actionsVector[i]);
    if (actionsMap.find(hashValue) != actionsMap.end()) {
      printf("[global_variables] bad hashing function, collision detected when "
             "populating map");
      printf("[global_variables] exit..");
      abort();
    }
    actionsMap.insert(std::make_pair(hashValue, i));
  }

  return actionsMap;
}

// Maps hash to legal actions index
auto globalLegalActionsToIndexMap = createActionsToIndexMap(allLegalActions);
// Maps hash to legal flop actions index
auto globalLegalFlopActionsToIndexMap =
    createActionsToIndexMap(allLegalFlopActions);
// Maps hash to legal turn and river actions index
auto globalLegalTurnRiverActionsToIndexMap =
    createActionsToIndexMap(allLegalTurnRiverActions);
// Maps hash to legal reraise actions index
auto globalLegalReraiseActionsToIndexMap =
    createActionsToIndexMap(allLegalReraiseActions);
// Overall max number of legal actions in stages
const size_t globalNumLegalActions = allLegalActions.size();
// Vector containing max values for each category
// bucket, stage, active players code, pot pct, call pot pct, current player,
// num legal actions, is reraise
const std::vector<size_t> maxValues = {1, NUM_BUCKETS,           4, 3, 10, 10,
                                       3, globalNumLegalActions, 2};
// Vector containing max values for each category (RTS)
// total max hand, stage, active players code, pot pct, call pot pct, current
// player, num legal actions, is reraise
const std::vector<size_t> maxValuesRTS = {
    1, NUM_RTS_BUCKETS, 4, 3, 10, 10, 3, globalNumLegalActions, 2};

// Calculates the cumulative product and stores intermediate values in vector
std::vector<size_t> getCumMaxValVector(const std::vector<size_t> &maxValVec) {
  std::vector<size_t> cumMaxValVec(maxValVec.size() - 1);
  size_t cumProd = 1;
  for (size_t idx = 0; idx < maxValVec.size() - 1; ++idx) {
    cumProd *= maxValVec[idx];
    cumMaxValVec[idx] = cumProd;
  }
  return cumMaxValVec;
}

// Vector containing cumulative product of max values
const auto maxValuesProd = getCumMaxValVector(maxValues);
// Vector containing cumulative product of max values (RTS)
const auto maxValuesProdRTS = getCumMaxValVector(maxValuesRTS);

// Gets legal action vector for given hash
std::vector<int> codeToLegalAction(size_t code) {
  return allLegalActions[code];
}

// Gets legal action code for legal action vector for given betting stage and
// reraise scenario
size_t getLegalActionCode(bool isReraise, size_t bettingStage,
                          const std::vector<int> &actions) {

  const size_t hashValue = vecHash(actions);

  try {
    if (isReraise)
      return globalLegalReraiseActionsToIndexMap.at(hashValue);
    else if (bettingStage == 0)
      return globalLegalActionsToIndexMap.at(hashValue);
    else if (bettingStage == 1)
      return globalLegalFlopActionsToIndexMap.at(hashValue);
    else /* (bettingStage == 2 v 3) */
      return globalLegalTurnRiverActionsToIndexMap.at(hashValue);
  } catch (const std::out_of_range &e) {
    printf("[global_variables] Legal Action Code (%zu) not found!\n",
           hashValue);
    printf("[global_variables] isReraise %d\n", isReraise);
    printf("[global_variables] bettingStage %zu\n", bettingStage);
    printVec("actions", actions.begin(), actions.end());
    abort();
  }
}

} // namespace extensions

#endif // _GLOBAL_VARIABLES_H_
