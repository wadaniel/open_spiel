#ifndef _UTILS_H_
#define _UTILS_H_

namespace extensions
{

template <class T>
void printVec(std::string name, std::vector<T> vec)
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
    res.push_back(text.substr(start, end - start));
	
	for(auto s : res) printf("%s\n",s.c_str());
	return res;
}

// Calculate hash of a vector of ints (TODO (DW): verify if this is clas hfree)
size_t vecHash(const std::vector<int>& vec) {
      std::size_t seed = vec.size();
        for(auto& i : vec) {
                seed ^= i + 0x9e3779b9 + (seed << 6) + (seed >> 2);
                  }
          return seed;
}

// Randomly sample one element from options according to the probability weights
template <class T>
inline T randomChoice(std::vector<T> options, std::vector<float> weights)
{
    T choice;
    const float unif = std::rand()/RAND_MAX;
    float sumWeight = 0.f;
    for(size_t i = 0; i < weights.size(); ++i)
    {
        sumWeight += weights[i];
        if (sumWeight > unif)
        {
            choice = options[i];
        }
    }
    return choice;
}


}

#endif //_UTILS_H_
