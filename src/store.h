#ifndef STORE_H
#define STORE_H

#include <string>
#include <vector>
#include <array>
#include <cstdint>
#include <functional>
#include <utility>

namespace SugRedis {

    class Store {
        public:
            Store();
            ~Store() = default;

            void set(std::string key, std::string value);
            bool get(const std::string& key, std::string& out);
            bool del(const std::string& key);

        private:
            static constexpr double MAX_LOAD_FACTOR = 1.0;
            static constexpr double MIN_LOAD_FACTOR = 0.1;

            struct Node {
                std::string key;
                std::string value;
                uint64_t hash;   // full hash, kept so rehash doesn't recompute it
                Node* next;

                Node(std::string k, std::string v, uint64_t code, Node* nextN)
                    : key(std::move(k)), value(std::move(v)), hash(code), next(nextN) {}
            };

            struct Table {
                std::vector<Node*> buckets;
                size_t used = 0;   // number of entries
                size_t size = 0;   // number of buckets (power of 2)
                size_t mask = 0;   // size - 1, used in place of %
            };

            Node* findKey(Node* head, const std::string& key);
            Node* findNode(const std::string& key, uint64_t hash);
            void rehashStep(unsigned int n);
            void maybeStartRehash();
            void finishRehash();

            std::hash<std::string> hasher_;
            std::array<Table, 2> hashtables_;
            long rehashcursor = -1;   // -1 means "not rehashing"
    };

};  // namespace SugRedis

#endif
