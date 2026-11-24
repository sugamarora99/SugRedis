#include "store.h"

#include <utility>

namespace SugRedis {

    constexpr size_t DEFAULT_TABLE_SIZE = 4;

    Store::Store() {
        hashtables_[0].buckets.resize(DEFAULT_TABLE_SIZE);
        hashtables_[0].size = DEFAULT_TABLE_SIZE;
        hashtables_[0].mask = DEFAULT_TABLE_SIZE - 1;
    }

    Store::Node* Store::findKey(Node* head, const std::string& key) {
        while (head != nullptr) {
            if (head->key == key) {
                return head;
            }
            head = head->next;
        }
        return nullptr;
    }

    Store::Node* Store::findNode(const std::string& key, uint64_t hash) {
        if (rehashcursor != -1) {
            long origIdx = static_cast<long>(hash & hashtables_[0].mask);
            if (origIdx < rehashcursor) {
                return findKey(hashtables_[1].buckets[hash & hashtables_[1].mask], key);
            }
            return findKey(hashtables_[0].buckets[origIdx], key);
        }
        return findKey(hashtables_[0].buckets[hash & hashtables_[0].mask], key);
    }

    // migrates n buckets to new hashtable.
    // if n + rehashCursor > old hashtable size, then this function will
    // just rehash all remaining buckets in the old hashtable.
    void Store::rehashStep(unsigned int n) {
        if (rehashcursor == -1) {
            return;
        }

        long end = static_cast<long>(hashtables_[0].size);
        while (n > 0 && rehashcursor < end && hashtables_[0].used > 0) {
            if (hashtables_[0].buckets[rehashcursor] != nullptr) {
                //move all nodes in this bucket
                Node* entry = hashtables_[0].buckets[rehashcursor];
                while (entry != nullptr) {
                    Node* next = entry->next;
                    uint64_t idx = entry->hash & hashtables_[1].mask;
                    entry->next = hashtables_[1].buckets[idx];
                    hashtables_[1].buckets[idx] = entry;
                    hashtables_[0].used--;
                    hashtables_[1].used++;
                    entry = next;
                }
                hashtables_[0].buckets[rehashcursor] = nullptr;
            }
            rehashcursor++;
            n--;
        }

        if (hashtables_[0].used == 0) {
            finishRehash();
        }
    }

    // starts rehashing only if not already rehashing and load factor does threshold
    void Store::maybeStartRehash() {
        if (rehashcursor != -1) {
            return;
        }

        double loadFactor = static_cast<double>(hashtables_[0].used) / hashtables_[0].size;
        if (loadFactor < MAX_LOAD_FACTOR) {
            return;
        }

        size_t newSize = hashtables_[0].size * 2;
        hashtables_[1].buckets.resize(newSize);
        hashtables_[1].size = newSize;
        hashtables_[1].mask = newSize - 1;
        hashtables_[1].used = 0;
        rehashcursor = 0;
    }

    void Store::finishRehash() {
        std::swap(hashtables_[0], hashtables_[1]);
        hashtables_[1] = Table{};
        rehashcursor = -1;
    }

    void Store::set(std::string key, std::string value) {
        rehashStep(1);

        uint64_t hash = hasher_(key);

        Node* existing = findNode(key, hash);
        if (existing != nullptr) {
            existing->value = std::move(value);
            return;
        }

        Table& table = (rehashcursor != -1) ? hashtables_[1] : hashtables_[0];
        uint64_t idx = hash & table.mask;
        table.buckets[idx] = new Node(std::move(key), std::move(value), hash, table.buckets[idx]);
        table.used++;

        maybeStartRehash();
    }

    bool Store::get(const std::string& key, std::string& out) {
        uint64_t hash = hasher_(key);
        Node* node = findNode(key, hash);
        if (node == nullptr) {
            return false;
        }
        out = node->value;
        return true;
    }

    bool Store::del(const std::string& key) {
        rehashStep(1);

        uint64_t hash = hasher_(key);

        Table* table;
        uint64_t idx;
        if (rehashcursor != -1) {
            long origIdx = static_cast<long>(hash & hashtables_[0].mask);
            if (origIdx < rehashcursor) {
                table = &hashtables_[1];
                idx = hash & hashtables_[1].mask;
            } else {
                table = &hashtables_[0];
                idx = origIdx;
            }
        } else {
            table = &hashtables_[0];
            idx = hash & hashtables_[0].mask;
        }

        Node* prev = nullptr;
        Node* head = table->buckets[idx];
        while (head != nullptr) {
            if (head->key == key) {
                if (prev != nullptr) {
                    prev->next = head->next;
                } else {
                    table->buckets[idx] = head->next;
                }
                table->used--;
                delete head;
                return true;
            }
            prev = head;
            head = head->next;
        }
        return false;
    }

};  // namespace SugRedis
