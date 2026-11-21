#ifndef STORE_H
#define STORE_H

#include <string>
#include <map>

namespace SugRedis {

    class Store {
        public:
            Store() = default;
            ~Store() = default;

            void set(std::string key, std::string value){
                data_.insert_or_assign(std::move(key), std::move(value));
            }

            bool get(const std::string& key, std::string& out){
                auto it = data_.find(key);
                if(it == data_.end()){
                    return false;
                }
                out = it->second;
                return true;
            }

            bool del(const std::string& key){
                return data_.erase(key) ? true : false;
            }

        private:
            std::map<std::string, std::string> data_;            
    };

};  // namespace SugRedis

#endif