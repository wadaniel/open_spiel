#ifndef _BELIEF_H_
#define _BELIEF_H_

#include <vector>

namespace extensions {

// get all hand combinations, pairs sorted descending
std::vector<std::vector<uint8_t>> getAllHands() {
  std::vector<std::vector<uint8_t>> allHands;
  for (uint8_t i = 0; i < 52; ++i)
    for (uint8_t j = i + 1; j < 52; ++j)
      allHands.push_back(std::vector<uint8_t>{i, j});

  return allHands;
}

const auto allPossibleHands = getAllHands();
const size_t numPossibleHands = allPossibleHands.size();

std::vector<std::vector<float>> updateHandProbabilitiesFromSeenCards(
    const std::vector<uint8_t> &seenCards,
    const std::vector<std::vector<float>> &handBeliefs) {
  // Copy old beliefs
  std::vector<std::vector<float>> newHandBeliefs = handBeliefs;

  // Init vars
  size_t numPlayer = newHandBeliefs.size();
  std::vector<float> cumBelief(numPlayer, 0.);

  // Search cards
  for (size_t idx = 0; idx < numPossibleHands; ++idx) {
    for (uint8_t card : seenCards) {
      // Set hand belief 0 if card has been seen
      if ((allPossibleHands[idx][0] == card) ||
          (allPossibleHands[idx][1] == card)) {
        for (size_t player = 0; player < numPlayer; player++)
          newHandBeliefs[player][idx] = 0.;
		break;
	  }
    }

    for (size_t player = 0; player < numPlayer; player++)
      cumBelief[player] += newHandBeliefs[player][idx];
  }

  // Normalize beliefs
  for (size_t player = 0; player < numPlayer; ++player) {
    assert(cumBelief[player] > 1e-12);
    for (size_t idx = 0; idx < numPossibleHands; ++idx) {
      newHandBeliefs[player][idx] /= cumBelief[player];
    }
  }

  return newHandBeliefs;
}

int getHandId(std::vector<uint8_t> hand) {
  for (int handIdx = 0; handIdx < allPossibleHands.size(); handIdx++) {
    if ((hand[0] == allPossibleHands[handIdx][0]) &&
        (hand[1] == allPossibleHands[handIdx][1]))
      return handIdx;
  }
  assert(false);
  return -1;
}

} // namespace extensions

#endif // _BELIEF_H_

/*
def getHandProbabilitiesFromStrategy(publicCards, infoSet, action, legalActions,
priorBeliefs, actualHand=None, handID=-1): # print("attention, not using
getHandProbabilitiesFromStrategy") #return priorBeliefs

    infoSet = copy.deepcopy(infoSet)
    handProbabilities = np.zeros(numPossibleHands)
    actionIdx = np.argwhere(legalActions == action)[0]

    if priorBeliefs.sum() < 1e-32:
        print("[belief] sth strange is going on, very low prior for all hands,
exit..") print("[belief] sum prior p: {}".format(priorBeliefs.sum()))
        assert(False)

    actualHandIdx = None
    if actualHand is not None:
        actualHand = np.sort(actualHand)

    for hidx, hand in enumerate(allPossibleHands):

        # find hidx which corresponds to players hand (if provided)
        if actualHand is not None:
            if (hand == actualHand).all():
                actualHandIdx = hidx

        # skip very unlikely hands
        if priorBeliefs[hidx] < 1e-32:
            continue

        # get hypothetical bucket
        bettingStage = infoSet[1]

        # attention: with the current implementation, from flop there is always
rts before calculating the belief update at the end of the round # be careful
that shared_rts_strategy is longer than shared_strategy now
        if(gv.useRealTimeSearch and bettingStage > 0):
            bucket = hidx
            infoSet[0] = bucket
            arrayPos = get_array_pos(infoSet, hidx)
            sharedStrategy =
np.asarray(gv.shared_rts_strategy[arrayPos:arrayPos+9]) else: bucket =
getCardsCluster(hand, publicCards, bettingStage) infoSet[0] = bucket arrayPos =
get_array_pos(infoSet) sharedStrategy =
np.asarray(gv.shared_strategy[arrayPos:arrayPos+9])

        probabilities = np.asarray([sharedStrategy[action] for _, action in
enumerate(legalActions)]) cumProb = probabilities.sum() if cumProb > 1e-9:
            probabilities /= cumProb
        else:
            probabilities = np.ones(len(legalActions))/len(legalActions)
        pActions = probabilities

        # get hypothetical probability of taken action
        pHand = pActions[actionIdx]
        handProbabilities[hidx] = pHand

    if (actualHand is not None and actualHandIdx is None):
        print("[belief] sth strange is going on, actual hand not found")
        assert(False)

    if handProbabilities.sum() < 1e-32:
        handProbabilities = priorBeliefs / priorBeliefs.sum()

    else:
        # TODO: maybe we just take beliefs from last stage?
        # empirically this gives better results
        newHandProbabilities = handProbabilities * priorBeliefs
        #newHandProbabilities = handProbabilities
        sumNewHandP = newHandProbabilities.sum()

        if sumNewHandP < 1e-32:
            handProbabilities = priorBeliefs  / priorBeliefs.sum()
        else:
            handProbabilities = newHandProbabilities / sumNewHandP

    return handProbabilities
*/
