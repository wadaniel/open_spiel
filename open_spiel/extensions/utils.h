#ifndef _UTILS_H_
#define _UTILS_H_

#include <fstream>
#include <stdlib.h> // rand
#include "nlohmann/json.hpp"

#include <sys/types.h>
#include <unistd.h>

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

template<typename Iterator>
int randomChoice(Iterator begin, Iterator end)
{
    using value_type = typename std::iterator_traits<Iterator>::value_type;
    value_type sumWeight = value_type();
    const float unif = (float)rand()/(float)(RAND_MAX+1.f);
	
    size_t idx = 0;
    while((begin+idx != end) && (sumWeight < unif))
    {
        sumWeight += *(begin+idx);
		idx++;
    }
    if(sumWeight < unif)
    {
        printf("Error in randomChoice\n");
        printf("sumWeight %f unif %f\n", sumWeight, unif);
        idx = 0;
        while(begin+idx != end)
        {
            printf("p[%d] %f\n", idx, *(begin+idx));
			idx++;
        }
        abort();
    }
	assert(sumWeight >= unif);
    return idx-1;
}



}

#endif //_UTILS_H_
