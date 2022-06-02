#ifndef _UTILS_H_
#define _UTILS_H_

namespace extensions
{

#include <stdlib.h> // rand

template <class T>
void printVec(const std::string& name, const std::vector<T>& vec)
{
	printf("%s\n", name.c_str());
	for(T element : vec)
		printf("%s\n",std::to_string(element).c_str());
}

// Split the string in substrings given by delim, return substrings in vecetor
std::vector<std::string> split(const std::string& text, const std::string& delim)
{
	printf("split: %s\n", text.c_str());
	auto start = 0U;
    auto end = text.find(delim);
	std::vector<std::string> res;
    while (end != std::string::npos)
    {
		res.push_back(text.substr(start, end - start));
        start = end + delim.length();
        end = text.find(delim, start);
    }
	const auto rest = text.substr(start, end - start);
	if (rest.empty() == false)
    	res.push_back(rest);
	for(auto& s : res) printf("[%s]\n",s.c_str());
	
	return res;
}

// Calculate hash of a vector of ints (TODO (DW): verify if this is clas hfree)
size_t vecHash(const std::vector<int>& vec) {
  	std::size_t seed = vec.size();
	for(auto& element : vec) seed ^= element + 0x9e3779b9 + (seed << 6) + (seed >> 2);
 	return seed;
}

// Randomly sample one element from options according to the probability weights
template <class T>
T randomChoice(const std::vector<T>& options, const std::vector<float>& weights)
{
	T choice;
    float sumWeight = 0.f;
    const float unif = (float)rand()/(float)RAND_MAX;
	
	size_t idx = 0;
    for(; idx < weights.size(); ++idx)
    {
        sumWeight += weights[idx];
        if (sumWeight >= unif)
        {
            choice = options[idx];
			break;
        }
    }
	assert(sumWeight >= unif);
    return choice;
}


}

#endif //_UTILS_H_
