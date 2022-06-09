#ifndef _GLOBAL_VARIABLES_H_
#define _GLOBAL_VARIABLES_H_

#include "utils.h"

//#define FAKEDICT

#define TOTALSTACK 500
#define BBSIZE 20

#define NUM_BUCKETS 500
#define NUM_RTS_BUCKETS 1326


namespace extensions
{

bool applyPruning = true;


std::map<std::string, size_t> preflopBucket;
std::map<std::string, size_t> flopBucket;
std::map<std::string, size_t> turnBucket;
std::map<std::string, size_t> riverBucket;

// 14 legal actions
const std::vector<std::vector<int>> allLegalActions = { {0, 1},
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
                                                  {1, 3, 4, 5, 6, 7, 8} };

// 5 legal actions
const std::vector<std::vector<int>> allLegalReraiseActions = 
    { {0, 1},
      {0, 1, 8},
      {0, 1, 5, 8},
      {1, 5, 8},
      {1, 8 } };

// 9 legal actions
const std::vector<std::vector<int>> allLegalTurnRiverActions = 
    { {0, 1},
      {0, 1, 8},
      {0, 1, 3, 8},
      {0, 1, 3, 5, 8},
      {0, 1, 5, 8},
      {1, 3, 8},
      {1, 3, 5, 8},
      {1, 5, 8},
      {1, 8} };

// 11 legal actions
const std::vector<std::vector<int>> allLegalFlopActions = 
    { {0, 1},
      {0, 1, 8},
      {0, 1, 3, 8},
      {0, 1, 3, 5, 8},
      {0, 1, 5, 8},
      {1, 3, 8},
      {1, 3, 5, 8},
      {1, 8},
      {0, 1, 3, 5, 6, 8},
      {0, 1, 5, 6, 8},
      {1, 3, 5, 6, 8} };

// Create a map for hash to index of legal actions vector
std::map<int, int> createActionsToIndexMap(const std::vector<std::vector<int>>& actionsVector)
{
    std::map<int, int> actionsMap;
    for(size_t i = 0; i < actionsVector.size(); ++i)
    {
        int hashValue = vecHash(actionsVector[i]);
        actionsMap.insert(std::make_pair(hashValue, i));
    }

    return actionsMap;
}

// Maps hash to legal actions index
const auto globalLegalActionsToIndexMap = createActionsToIndexMap(allLegalActions);
// Maps hash to legal flop actions index
const auto globalLegalFlopActionsToIndexMap = createActionsToIndexMap(allLegalFlopActions);
// Maps hash to legal turn and river actions index
const auto globalLegalTurnRiverActionsToIndexMap = createActionsToIndexMap(allLegalTurnRiverActions);
// Maps hash to legal reraise actions index
const auto globalLegalReraiseActionsToIndexMap = createActionsToIndexMap(allLegalReraiseActions);
// Overall max number of legal actions in stages
const size_t globalNumLegalActions = allLegalActions.size();
// Vector containing max values for each category
// bucket, stage, active players code, pot pct, call pot pct, current player, num legal actions, is reraise
const std::vector<size_t> maxValues = {1, NUM_BUCKETS, 4, 3, 10, 10, 3, globalNumLegalActions, 2 }; 
// Vector containing max values for each category (RTS)
// total max hand, stage, active players code, pot pct, call pot pct, current player, num legal actions, is reraise
const std::vector<size_t> maxValuesRTS = {1, NUM_RTS_BUCKETS, 4, 3, 10, 10, 3, globalNumLegalActions, 2 };

// Calculates the cumulative product and stores intermediate values in vector
std::vector<size_t> getCumMaxValVector(const std::vector<size_t>& maxValVec)
{    
    std::vector<size_t> cumMaxValVec(maxValVec.size()-1);
    size_t cumProd = 1;
    for(size_t idx = 0; idx < maxValVec.size()-1; ++idx)
    {
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
std::vector<int> codeToLegalAction(size_t code)
{
    return allLegalActions.at(code);
}

// Gets legal preflop action code for legal action vector
size_t getLegalActionCodePreFlop(const std::vector<int>& actions)
{
    int hashValue = vecHash(actions);
    return globalLegalActionsToIndexMap.at(hashValue);
}

// Gets legal rereaise action code for legal action vector
size_t getLegalActionCodeReraise(const std::vector<int>& actions)
{
    int hashValue = vecHash(actions);
    return globalLegalReraiseActionsToIndexMap.at(hashValue);
}

// Gets legal turnriver action code for legal action vector
size_t getLegalActionCodeTurnRiver(const std::vector<int>& actions)
{
    int hashValue = vecHash(actions);
    return globalLegalTurnRiverActionsToIndexMap.at(hashValue);
}

// Gets legal flop action code for legal action vector
size_t getLegalActionCodeFlop(const std::vector<int>& actions)
{
    int hashValue = vecHash(actions);
    return globalLegalFlopActionsToIndexMap.at(hashValue);
}

// Gets legal action code for legal action vector for given betting stage and reraise scenario
size_t getLegalActionCode(bool isReraise, size_t bettingStage, const std::vector<int>& actions)
{
    //printf("isReraise %d, bettingStage %zu\n", isReraise, bettingStage);
    if(isReraise)
        return getLegalActionCodeReraise(actions);
    else if(bettingStage == 0)
        return getLegalActionCodePreFlop(actions);
    else if(bettingStage == 1)
        return getLegalActionCodeFlop(actions);
    else if(bettingStage == 2)
        return getLegalActionCodeTurnRiver(actions);
}


}

#endif // _GLOBAL_VARIABLES_H_
