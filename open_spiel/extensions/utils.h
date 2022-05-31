#ifndef _UTILS_H_
#define _UTILS_H_

#include <regex>

namespace extensions
{

std::vector<std::string> split(const std::string str, const std::string regex_str)
{
    std::regex regexz(regex_str);
    std::vector<std::string> list(std::sregex_token_iterator(str.begin(), str.end(), regexz, -1),
    std::sregex_token_iterator());
    return list;
}

size_t vecHash(const std::vector<int>& vec) {
      std::size_t seed = vec.size();
        for(auto& i : vec) {
                seed ^= i + 0x9e3779b9 + (seed << 6) + (seed >> 2);
                  }
          return seed;
}

}

#endif //_UTILS_H_
