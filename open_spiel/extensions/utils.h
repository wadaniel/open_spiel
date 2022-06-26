#ifndef _UTILS_H_
#define _UTILS_H_

#include <fstream>
#include <stdlib.h> // rand
#include "nlohmann/json.hpp"

namespace extensions
{

void readDictionaryFromJson(const std::string filename, std::map<std::string, size_t>& dict)
{
	std::ifstream ifs(filename);
	auto jobj = nlohmann::json::parse(ifs);
	dict = jobj.get<std::map<std::string, size_t>>();

}
template<typename Iterator>

// Jonathan disabled due to mysterious prints
void printVec(const std::string& name, Iterator begin, Iterator end)
{
    using value_type = typename std::iterator_traits<Iterator>::value_type;
	
	//printf("%s\n", name.c_str());
	while(begin != end)
	{
		//printf("%s\n",std::to_string(*begin).c_str());
		begin++;
	}
}

// Split the string in substrings given by delim, return substrings in vecetor
std::vector<std::string> split(const std::string& text, const std::string& delim)
{
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
	
	return res;
}

// Calculate hash of a vector of ints (TODO (DW): verify if this is clas hfree)
size_t vecHash(const std::vector<int>& vec) {
  	std::size_t seed = vec.size();
	for(const int element : vec) seed ^= element + 0x9e3779b9 + (seed << 6) + (seed >> 2);
 	return seed;
}

// Randomly sample one element from options according to the probability weights
template<typename Iterator>
typename std::iterator_traits<Iterator>::value_type

// https://www.tutorialspoint.com/random-pick-with-weight-in-cplusplus
/*int randomChoice(Iterator begin, Iterator end){
	float sum = 0;
	vector<float> presums;
	for(auto it = begin; it != end; ++it)
	{
		float num = *it;
		printf("randomchoice new %f %f \n", sum, num);
		sum += num;
		presums.push_back(sum);
	}
	const float unif = (float)rand()/(float)RAND_MAX;


}*/

randomChoice(Iterator begin, Iterator end)
{
    using value_type = typename std::iterator_traits<Iterator>::value_type;
    value_type sumWeight = value_type();
    const float unif = (float)rand()/(float)RAND_MAX;
	
    size_t idx = 0;
    while((begin != end) && (sumWeight < unif))
    {
        sumWeight += *begin;
        //printf("randomchoice %f %f %f %zu\n", sumWeight, *begin, unif, idx);
		idx++;
        begin++;

    }
	assert(sumWeight >= unif);
    return idx-1;
}



}

#endif //_UTILS_H_
