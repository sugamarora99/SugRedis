#ifndef UTILS_H
#define UTILS_H

#include <string_view>
#include <cctype>

namespace SugRedis {
    
    namespace Utils{
        
        // case insensitive string compare
        bool iStrEquals(std::string_view a, std::string_view b){
            if(a.size() != b.size()){
                return false;
            }
            for(size_t i = 0; i < a.size(); i++){
                if (std::tolower((unsigned char)a[i]) != std::tolower((unsigned char)b[i]))
                    return false;
            }
            return true;
        }

    };  // namespace Utils

};  // namespace SugRedis

#endif