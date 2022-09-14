#ifndef _UTILS_H_
#define _UTILS_H_

#include "nlohmann/json.hpp"
#include <fstream>
#include <stdlib.h> // rand

#include <random> // for mt19937
#include <regex>
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

// Transform json file to a cpp map
void readDeepDictionaryFromJson(const std::string filename,
                            std::map<std::string, std::map<std::string, size_t>> &dict) {
  std::ifstream ifs(filename);
  auto jobj = nlohmann::json::parse(ifs);
  // does not work so we do 2-stage parsing
  dict = jobj.get<std::map<std::string, std::map<std::string, size_t>>>();
  
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
inline std::vector<std::string> split(const std::string &text,
                                      const std::string &delim) {
  /*
  const std::regex reg(delim);
  std::sregex_token_iterator first{text.begin(), text.end(), reg, -1}, last;
  const std::vector<std::string> result{first, last};
  return result;
  */

  auto start = 0U;
  auto end = text.find(delim);
  std::vector<std::string> result;
  while (end != std::string::npos) {
    result.push_back(text.substr(start, end - start));
    start = end + delim.length();
    end = text.find(delim, start);
  }
  const auto rest = text.substr(start, end - start);
  if (rest.empty() == false)
    result.push_back(rest);

  return result;
}

// Calculate hash of a vector of ints
size_t vecHash(const std::vector<int> &vec) {
  size_t seed = vec.size();
  for (const int element : vec)
    seed ^= element + 0x9e3779b9 + (seed << 6) + (seed >> 2);
  return seed;
}

// Calculate hash of a vector of ints
size_t vecHashAlt(const std::vector<int> &vec) {
  size_t seed = vec.size();
  for (auto x : vec) {
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = (x >> 16) ^ x;
    seed ^= x + 0x9e3779b9 + (seed << 6) + (seed >> 2);
  }
  return seed;
}

// Sample an index given some probability weights
template <typename Iterator> int randomChoice(Iterator begin, Iterator end) {

  // Set seed
  if (isRngInitialized == false) {
    int pid = getpid();
    printf("[utils] Initializing random seed (%d) ..\n", pid);
    generator.seed(pid);
    isRngInitialized = true;
  }

  size_t idx = 0;
  double unif, sumWeight;
  do {
    idx = 0;
    sumWeight = 0.;
    // Get random number in [0,1)
    unif = distribution(generator);

    while ((begin + idx != end) && (sumWeight < unif)) {
      sumWeight += *(begin + idx);
      idx++;
    }
  } while (sumWeight < unif); // Save sampling

  // Verify smpling worked correctly
  if (sumWeight < unif) {
    printf("[utils] Error in randomChoice\n");
    printf("[utils] sumWeight %lf unif %lf\n", sumWeight, unif);
    idx = 0;
    while (begin + idx != end) {
      printf("p[%zu] %lf\n", idx, *(begin + idx));
      idx++;
    }
    abort();
  }
  assert(sumWeight >= unif);
  // Note: if we remove this, we cannot take the last idx, it may
  // be a 0 probability action, maybe search for the max p?
  return idx - 1;
}

} // namespace extensions

#endif //_UTILS_H_
