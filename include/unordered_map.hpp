#pragma once

#include <cstddef.hpp>
#include <cstdint.hpp>
#include <memory.hpp>
#include <new.hpp>
#include <string.hpp>
#include <utility.hpp>

namespace std {
template <typename T>
struct hash;

template <>
struct hash<int> {
    [[nodiscard]] size_t operator()(int value) const noexcept { return static_cast<size_t>(value) * 11400714819323198485ull; }
};

template <>
struct hash<unsigned int> {
    [[nodiscard]] size_t operator()(unsigned int value) const noexcept { return static_cast<size_t>(value) * 11400714819323198485ull; }
};

template <>
struct hash<long> {
    [[nodiscard]] size_t operator()(long value) const noexcept { return static_cast<size_t>(value) * 11400714819323198485ull; }
};

template <>
struct hash<unsigned long> {
    [[nodiscard]] size_t operator()(unsigned long value) const noexcept { return static_cast<size_t>(value) * 11400714819323198485ull; }
};

template <>
struct hash<long long> {
    [[nodiscard]] size_t operator()(long long value) const noexcept { return static_cast<size_t>(value) * 11400714819323198485ull; }
};

template <>
struct hash<unsigned long long> {
    [[nodiscard]] size_t operator()(unsigned long long value) const noexcept { return static_cast<size_t>(value) * 11400714819323198485ull; }
};

template <>
struct hash<string> {
    [[nodiscard]] size_t operator()(const string& value) const noexcept {
        size_t h = 1469598103934665603ull;
        for (char ch : value) {
            h ^= static_cast<unsigned char>(ch);
            h *= 1099511628211ull;
        }
        return h;
    }
};

template <typename T>
struct hash<T*> {
    [[nodiscard]] size_t operator()(T* value) const noexcept {
        return (reinterpret_cast<size_t>(value) >> 4) * 11400714819323198485ull;
    }
};

template <typename K, typename V, typename Hash = hash<K>>
class unordered_map {
public:
    unordered_map() noexcept : buckets_(nullptr), bucket_count_(0), size_(0), used_(0) {}

    unordered_map(const unordered_map& other) : unordered_map() {
        reserve_for(other.size_);
        for (size_t i = 0; i < other.bucket_count_; ++i) {
            if (other.buckets_[i].state == state::occupied) {
                insert(other.buckets_[i].key(), other.buckets_[i].value());
            }
        }
    }

    unordered_map(unordered_map&& other) noexcept
        : buckets_(other.buckets_), bucket_count_(other.bucket_count_), size_(other.size_), used_(other.used_) {
        other.buckets_ = nullptr;
        other.bucket_count_ = 0;
        other.size_ = 0;
        other.used_ = 0;
    }

    ~unordered_map() {
        clear();
        std::free(buckets_);
    }

    unordered_map& operator=(const unordered_map& other) {
        if (this != &other) {
            clear();
            reserve_for(other.size_);
            for (size_t i = 0; i < other.bucket_count_; ++i) {
                if (other.buckets_[i].state == state::occupied) {
                    insert(other.buckets_[i].key(), other.buckets_[i].value());
                }
            }
        }
        return *this;
    }

    unordered_map& operator=(unordered_map&& other) noexcept {
        if (this != &other) {
            clear();
            std::free(buckets_);
            buckets_ = other.buckets_;
            bucket_count_ = other.bucket_count_;
            size_ = other.size_;
            used_ = other.used_;
            other.buckets_ = nullptr;
            other.bucket_count_ = 0;
            other.size_ = 0;
            other.used_ = 0;
        }
        return *this;
    }

    bool insert(const K& key, const V& value) {
        ensure_capacity();
        bool found = false;
        size_t index = find_slot(key, &found);
        if (found) {
            buckets_[index].value() = value;
            return false;
        }
        construct_bucket(index, key, value);
        return true;
    }

    bool erase(const K& key) {
        if (bucket_count_ == 0) {
            return false;
        }
        bool found = false;
        size_t index = find_slot(key, &found);
        if (!found) {
            return false;
        }
        buckets_[index].destroy();
        buckets_[index].state = state::deleted;
        --size_;
        return true;
    }

    [[nodiscard]] V* find(const K& key) noexcept {
        if (bucket_count_ == 0) {
            return nullptr;
        }
        bool found = false;
        size_t index = find_slot(key, &found);
        return found ? &buckets_[index].value() : nullptr;
    }

    [[nodiscard]] const V* find(const K& key) const noexcept {
        if (bucket_count_ == 0) {
            return nullptr;
        }
        bool found = false;
        size_t index = find_slot(key, &found);
        return found ? &buckets_[index].value() : nullptr;
    }

    [[nodiscard]] bool contains(const K& key) const noexcept {
        return find(key) != nullptr;
    }

    V& operator[](const K& key) {
        ensure_capacity();
        bool found = false;
        size_t index = find_slot(key, &found);
        if (!found) {
            construct_bucket(index, key, V{});
        }
        return buckets_[index].value();
    }

    [[nodiscard]] size_t size() const noexcept { return size_; }
    [[nodiscard]] bool empty() const noexcept { return size_ == 0; }

    void clear() noexcept {
        for (size_t i = 0; i < bucket_count_; ++i) {
            if (buckets_[i].state == state::occupied) {
                buckets_[i].destroy();
            }
            buckets_[i].state = state::empty;
        }
        size_ = 0;
        used_ = 0;
    }

private:
    // Open addressing keeps the implementation allocation-light for small
    // freestanding programs: one contiguous bucket array, no per-node mallocs.
    enum class state : unsigned char {
        empty,
        occupied,
        deleted,
    };

    struct bucket {
        state state = state::empty;
        alignas(K) unsigned char key_storage[sizeof(K)];
        alignas(V) unsigned char value_storage[sizeof(V)];

        [[nodiscard]] K& key() noexcept { return *reinterpret_cast<K*>(key_storage); }
        [[nodiscard]] const K& key() const noexcept { return *reinterpret_cast<const K*>(key_storage); }
        [[nodiscard]] V& value() noexcept { return *reinterpret_cast<V*>(value_storage); }
        [[nodiscard]] const V& value() const noexcept { return *reinterpret_cast<const V*>(value_storage); }

        void destroy() noexcept {
            value().~V();
            key().~K();
        }
    };

    void ensure_capacity() {
        if (bucket_count_ == 0) {
            rehash(16);
            return;
        }
        if ((used_ + 1) * 10 >= bucket_count_ * 7) {
            rehash(bucket_count_ * 2);
        }
    }

    void reserve_for(size_t desired_size) {
        size_t count = 16;
        while (desired_size * 10 >= count * 7) {
            count *= 2;
        }
        if (count > bucket_count_) {
            rehash(count);
        }
    }

    void rehash(size_t new_count) {
        bucket* old_buckets = buckets_;
        size_t old_count = bucket_count_;

        buckets_ = static_cast<bucket*>(std::malloc(sizeof(bucket) * new_count));
        STD_LIBC_ASSERT(buckets_ != nullptr);
        for (size_t i = 0; i < new_count; ++i) {
            new (buckets_ + i) bucket();
        }
        bucket_count_ = new_count;
        size_ = 0;
        used_ = 0;

        for (size_t i = 0; i < old_count; ++i) {
            if (old_buckets[i].state == state::occupied) {
                insert(std::move(old_buckets[i].key()), std::move(old_buckets[i].value()));
                old_buckets[i].destroy();
            }
        }
        std::free(old_buckets);
    }

    size_t find_slot(const K& key, bool* found) const noexcept {
        size_t first_deleted = static_cast<size_t>(-1);
        size_t index = hasher_(key) & (bucket_count_ - 1);
        for (;;) {
            const bucket& b = buckets_[index];
            if (b.state == state::empty) {
                *found = false;
                return first_deleted != static_cast<size_t>(-1) ? first_deleted : index;
            }
            if (b.state == state::deleted) {
                if (first_deleted == static_cast<size_t>(-1)) {
                    first_deleted = index;
                }
            } else if (b.key() == key) {
                *found = true;
                return index;
            }
            index = (index + 1) & (bucket_count_ - 1);
        }
    }

    template <typename KeyArg, typename ValueArg>
    void construct_bucket(size_t index, KeyArg&& key, ValueArg&& value) {
        if (buckets_[index].state == state::empty) {
            ++used_;
        }
        new (buckets_[index].key_storage) K(std::forward<KeyArg>(key));
        new (buckets_[index].value_storage) V(std::forward<ValueArg>(value));
        buckets_[index].state = state::occupied;
        ++size_;
    }

    bucket* buckets_;
    size_t bucket_count_;
    size_t size_;
    size_t used_;
    Hash hasher_;
};
}
