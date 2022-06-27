#ifndef _UTILS_H_
#define _UTILS_H_

#include "nlohmann/json.hpp"
#include <fstream>
#include <stdlib.h> // rand

#include <random> // for mt19937
#include <sys/types.h>
#include <unistd.h>

namespace extensions {

// Check if we need to set the seed
bool isRngInitialized = false;
// Rng generator
std::default_random_engine generator;
// Uniform distribution
std::uniform_real_distribution<double> distribution(0.0, 1.0);

// Transform json file to a cpp map
void readDictionaryFromJson(const std::string filename,
                            std::map<std::string, size_t> &dict) {
  std::ifstream ifs(filename);
  auto jobj = nlohmann::json::parse(ifs);
  dict = jobj.get<std::map<std::string, size_t>>();
}

// Helper to print all elements of a container
template <typename Iterator>
void printVec(const std::string &name, Iterator begin, Iterator end) {
  using value_type = typename std::iterator_traits<Iterator>::value_type;

  printf("%s\n", name.c_str());
  while (begin != end) {
    printf("%s\n", std::to_string(*begin).c_str());
    begin++;
  }
}

// Split the string in substrings given by delim, return substrings in vecetor
std::vector<std::string> split(const std::string &text,
                               const std::string &delim) {
  auto start = 0U;
  auto end = text.find(delim);
  std::vector<std::string> res;
  while (end != std::string::npos) {
    res.push_back(text.substr(start, end - start));
    start = end + delim.length();
    end = text.find(delim, start);
  }
  const auto rest = text.substr(start, end - start);
  if (rest.empty() == false)
    res.push_back(rest);

  return res;
}

// Calculate hash of a vector of ints
size_t vecHash(const std::vector<int> &vec) {
  std::size_t seed = vec.size();
  for (const int element : vec)
    seed ^= element + 0x9e3779b9 + (seed << 6) + (seed >> 2);
  return seed;
}

// Sample an index given some probability weights
template <typename Iterator> int randomChoice(Iterator begin, Iterator end) {

  // Set seed
  if (isRngInitialized == false) {
    int pid = getpid();
    printf("Initializing random seed (%d) ..\n", pid);
    generator.seed(pid);
    isRngInitialized = true;
  }

  // Get random number in [0,1)
  const double unif = distribution(generator);

  size_t idx = 0;
  double sumWeight = 0.;
  while ((begin + idx != end) && (sumWeight < unif)) {
    sumWeight += *(begin + idx);
    idx++;
  }

  // Verify smpling worked correctly
  if (sumWeight < unif) {
    printf("Error in randomChoice\n");
    printf("sumWeight %lf unif %lf\n", sumWeight, unif);
    idx = 0;
    while (begin + idx != end) {
      printf("p[%zu] %lf\n", idx, *(begin + idx));
      idx++;
    }
    abort();
  }
  assert(sumWeight >=
         unif); // Note: if we remove this, we cannot take the last idx, it may
                // be a 0 probability action, maybe search for the max p?
  return idx - 1;
}

} // namespace extensions

#endif //_UTILS_H_
