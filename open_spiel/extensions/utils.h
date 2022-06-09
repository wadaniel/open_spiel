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

void printVec(const std::string& name, Iterator begin, Iterator end)
{
    using value_type = typename std::iterator_traits<Iterator>::value_type;
	
	printf("%s\n", name.c_str());
	while(begin != end)
	{
		printf("%s\n",std::to_string(*begin).c_str());
		begin++;
	}
}

// Split the string in substrings given by delim, return substrings in vecetor
std::vector<std::string> split(const std::string& text, const std::string& delim)
{
	//printf("split: %s\n", text.c_str());
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
	//for(auto& s : res) printf("[%s]\n",s.c_str());
	
	return res;
}

// Calculate hash of a vector of ints (TODO (DW): verify if this is clas hfree)
size_t vecHash(const std::vector<int>& vec) {
  	std::size_t seed = vec.size();
	for(auto& element : vec) seed ^= element + 0x9e3779b9 + (seed << 6) + (seed >> 2);
 	return seed;
}

// Randomly sample one element from options according to the probability weights
template<typename Iterator, typename Iterator2>
typename std::iterator_traits<Iterator>::value_type 
randomChoice(Iterator options_iterator_start, Iterator2 begin, Iterator2 end)
{
    using value_type = typename std::iterator_traits<Iterator>::value_type;
    using value_type2 = typename std::iterator_traits<Iterator2>::value_type;
	value_type choice = value_type();
    value_type2 sumWeight = value_type2();
    const float unif = (float)rand()/(float)RAND_MAX;
	
    for(size_t idx=0; begin != end; ++begin)
    {
        sumWeight += *begin;
        if (sumWeight >= unif)
        {
            choice = *(options_iterator_start+idx);
			break;
        }
		idx++;
    }
	assert(sumWeight >= unif);
    return choice;
}



}

#endif //_UTILS_H_
