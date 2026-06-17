#pragma once

#include <functional>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace rbft {

template <typename K, typename V>
class CustomHashTable {
public:
    explicit CustomHashTable(size_t bucket_count = 16)
        : buckets_(bucket_count == 0 ? 16 : bucket_count) {}

    void Put(const K& key, const V& value) {
        // 链地址法哈希表：负载因子过高时扩容，避免桶内线性查找退化过快。
        if (LoadFactor() > 0.75) {
            Rehash(buckets_.size() * 2);
        }
        auto& bucket = buckets_[Index(key)];
        for (auto& entry : bucket) {
            if (entry.key == key) {
                entry.value = value;
                return;
            }
        }
        bucket.push_back(Entry{key, value});
        ++size_;
    }

    bool Get(const K& key, V& out) const {
        const auto& bucket = buckets_[Index(key)];
        for (const auto& entry : bucket) {
            if (entry.key == key) {
                out = entry.value;
                return true;
            }
        }
        return false;
    }

    std::optional<V> Get(const K& key) const {
        V out{};
        if (Get(key, out)) {
            return out;
        }
        return std::nullopt;
    }

    bool Contains(const K& key) const {
        V ignored{};
        return Get(key, ignored);
    }

    bool Remove(const K& key) {
        auto& bucket = buckets_[Index(key)];
        for (auto it = bucket.begin(); it != bucket.end(); ++it) {
            if (it->key == key) {
                bucket.erase(it);
                --size_;
                return true;
            }
        }
        return false;
    }

    void Rehash(size_t new_bucket_count) {
        if (new_bucket_count < 1) {
            throw std::invalid_argument("bucket count must be positive");
        }
        // rehash 时重新走 Put，让每个 key 按新桶数量重新分布。
        auto old = std::move(buckets_);
        buckets_.assign(new_bucket_count, {});
        size_ = 0;
        for (const auto& bucket : old) {
            for (const auto& entry : bucket) {
                Put(entry.key, entry.value);
            }
        }
    }

    size_t Size() const { return size_; }
    size_t BucketCount() const { return buckets_.size(); }
    double LoadFactor() const { return buckets_.empty() ? 0.0 : static_cast<double>(size_) / buckets_.size(); }
    void Clear() {
        for (auto& bucket : buckets_) {
            bucket.clear();
        }
        size_ = 0;
    }

private:
    struct Entry {
        K key;
        V value;
    };

    size_t Index(const K& key) const {
        return std::hash<K>{}(key) % buckets_.size();
    }

    std::vector<std::vector<Entry>> buckets_;
    size_t size_{0};
};

} // namespace rbft
