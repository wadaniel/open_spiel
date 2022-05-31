#ifndef _GLOBAL_VARIABLES_H_
#define _GLOBAL_VARIABLES_H_

#include "utils.h"

#define NUM_BUCKETS 500
#define NUM_RTS_BUCKETS 1326

std::vector<float> sharedRTSstrategy;
std::vector<float> sharedRTSstrategyFrozen;
std::vector<float> sharedRTSRegret;
std::vector<float> sharedRegret;

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

const std::vector<std::vector<int>> allLegalReraiseActions = 
    { {0, 1},
      {0, 1, 8},
      {0, 1, 5, 8},
      {1, 5, 8},
      {1, 8 } };

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

const auto globalLegalActionsToIndexMap = createActionsToIndexMap(allLegalActions);
const auto globalLegalReraiseActionsToIndexMap = createActionsToIndexMap(allLegalReraiseActions);
const auto globalLegalTurnRiverActionsToIndexMap = createActionsToIndexMap(allLegalTurnRiverActions);
const auto globalLegalFlopActionsToIndexMap = createActionsToIndexMap(allLegalFlopActions);

const size_t globalNumLegalActions = allLegalActions.size();
const std::vector<size_t> maxValues = {1, NUM_BUCKETS, 4, 3, 10, 10, 3, globalNumLegalActions, 2 };

std::vector<size_t> getCumMaxValVector(const std::vector<size_t>& maxValVec)
{    
    std::vector<size_t> cumMaxValVec(maxValVec.size());
    size_t cumProd = 1;
    for(size_t idx = 0; idx < maxValVec.size(); ++idx)
    {
        cumProd *= maxValVec[idx];
        cumMaxValVec[idx] = cumProd;
    }
    return cumMaxValVec;
}

const std::vector<size_t> maxValuesRTS = {1, NUM_RTS_BUCKETS, 4, 3, 10, 10, 3, globalNumLegalActions, 2 };

const auto maxValuesProd = getCumMaxValVector(maxValues);
const auto maxValuesProdRTS = getCumMaxValVector(maxValuesRTS);

std::vector<int> codeToLegalAction(size_t code)
{
    return allLegalActions.at(code);
}

size_t getLegalActionCodePreFlop(const std::vector<int>& actions)
{
    int hashValue = vecHash(actions);
    return globalLegalActionsToIndexMap.at(hashValue);
}

size_t getLegalActionCodeReraise(const std::vector<int>& actions)
{
    int hashValue = vecHash(actions);
    return globalLegalReraiseActionsToIndexMap.at(hashValue);
}

size_t getLegalActionCodeTurnRiver(const std::vector<int>& actions)
{
    int hashValue = vecHash(actions);
    return globalLegalTurnRiverActionsToIndexMap.at(hashValue);
}

size_t getLegalActionCodeFlop(const std::vector<int>& actions)
{
    int hashValue = vecHash(actions);
    return globalLegalFlopActionsToIndexMap.at(hashValue);
}

size_t getLegalActionCode(bool isReraise, size_t bettingStage, const std::vector<int>& actions)
{
    if(isReraise)
        return getLegalActionCodeReraise(actions);
    else if(bettingStage == 0)
        return getLegalActionCodePreFlop(actions);
    else if(bettingStage == 1)
        return getLegalActionCodeFlop(actions);
    else if(bettingStage == 2)
        return getLegalActionCodeTurnRiver(actions);
}


#endif // _GLOBAL_VARIABLES_H_
