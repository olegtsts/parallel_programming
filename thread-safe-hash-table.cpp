#include <iostream>
#include <list>
#include <mutex>
#include <shared_mutex>
#include <vector>
#include <memory>
#include <algorithm>

template <typename TKey, typename TValue, typename THash=std::hash<TKey> >
class ThreadSafeHashTable {
public:
    using Key = TKey;
    using Value = TValue;
    using Hash = THash;

    ThreadSafeHashTable(size_t num_buckets=19, const THash& hasher = Hash())
    : buckets(num_buckets)
    , hasher(hasher)
    {
        for(size_t i = 0; i < num_buckets; ++i) {
            buckets[i].reset(new BucketType());
        }
    }

    ThreadSafeHashTable(const ThreadSafeHashTable&) = delete;
    ThreadSafeHashTable operator=(const ThreadSafeHashTable&) = delete;

    Value value_for(const TKey& key, const TValue& default_value) const {
        return get_bucket(key).value_for(key, default_value);
    }
    void add_or_update_mapping(const TKey& key, const TValue& default_value) {
        return get_bucket(key).add_or_update_mapping(key, default_value);
    }
    void remove_mapping(const TKey& key) {
        return get_bucket(key).remove_mapping(key);
    }
private:
    class BucketType {
    public:
        Value value_for(const TKey& key, const TValue& default_value) const {
            std::shared_lock<std::shared_timed_mutex> lock(mutex);
            TBucketConstIterator entry = find_entry_for(key);
            return (entry == data.end()) ? default_value : entry->second;
        }
        void add_or_update_mapping(const TKey& key, const TValue& value) {
            std::unique_lock<std::shared_timed_mutex> lock(mutex);
            TBucketIterator entry = find_entry_for(key);
            if (entry == data.end()) {
                data.push_back(TBucketValue(key, value));
            } else {
                entry->second = value;
            }
        }
        void remove_mapping(const TKey& key) {
            std::unique_lock<std::shared_timed_mutex> lock(mutex);
            TBucketIterator entry = find_entry_for(key);
            if (entry != data.end()) {
                data.erase(entry);
            }
        }
    private:
        using TBucketValue = std::pair<Key, Value>;
        using TBucketData = std::list<TBucketValue>;
        using TBucketIterator = typename TBucketData::iterator;
        using TBucketConstIterator = typename TBucketData::const_iterator;

        TBucketConstIterator find_entry_for(const TKey& key) const {
            return std::find_if(data.begin(), data.end(), [&] (const TBucketValue& item) {
                return item.first == key;
            });
        }
        TBucketIterator find_entry_for(const TKey& key) {
            return std::find_if(data.begin(), data.end(), [&] (const TBucketValue& item) {
                return item.first == key;
            });
        }

        TBucketData data;
        mutable std::shared_timed_mutex mutex;
    };

    BucketType& get_bucket(const TKey& key) const {
        return *buckets[hasher(key) % buckets.size()];
    }

    std::vector<std::unique_ptr<BucketType> > buckets;
    Hash hasher;
};
int main() {
    ThreadSafeHashTable<int, double> hash;
    hash.add_or_update_mapping(3, 5.2);
    std::cout << hash.value_for(3, 10) << " " << hash.value_for(4, 11) << std::endl;
    return 0;
}
