#pragma once
#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <memory>
#include <new>
#include <utility>

template <typename T>
class RawMemory {
public:
    RawMemory() = default;

    explicit RawMemory(size_t capacity)
        : buffer_(Allocate(capacity))
        , capacity_(capacity) {
    }
    
    RawMemory(const RawMemory&) = delete;
    RawMemory& operator=(const RawMemory& rhs) = delete;
    
    RawMemory(RawMemory&& other) noexcept 
        : buffer_(std::exchange(other.buffer_, nullptr))
        , capacity_(std::exchange(other.capacity_, 0)) { 
    }
    
    RawMemory& operator=(RawMemory&& rhs) noexcept {
        if (this != &rhs) {
            Swap(rhs);
            rhs.Deallocate(rhs.buffer_);
            rhs.buffer_ = nullptr;
            rhs.capacity_ = 0;
        }
        return *this;
    }

    ~RawMemory() {
        Deallocate(buffer_);
    }

    T* operator+(size_t offset) noexcept {
        // Разрешается получать адрес ячейки памяти, следующей за последним элементом массива
        assert(offset <= capacity_);
        return buffer_ + offset;
    }

    const T* operator+(size_t offset) const noexcept {
        return const_cast<RawMemory&>(*this) + offset;
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<RawMemory&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < capacity_);
        return buffer_[index];
    }

    void Swap(RawMemory& other) noexcept {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

    const T* GetAddress() const noexcept {
        return buffer_;
    }

    T* GetAddress() noexcept {
        return buffer_;
    }

    size_t Capacity() const {
        return capacity_;
    }

private:
    // Выделяет сырую память под n элементов и возвращает указатель на неё
    static T* Allocate(size_t n) {
        return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
    }

    // Освобождает сырую память, выделенную ранее по адресу buf при помощи Allocate
    static void Deallocate(T* buf) noexcept {
        operator delete(buf);
    }

    T* buffer_ = nullptr;
    size_t capacity_ = 0;
};

template <typename T>
class Vector {
public:
    
    Vector() = default;

    explicit Vector(size_t size)
        : data_(size)
        , size_(size)
    {
        std::uninitialized_value_construct_n(data_.GetAddress(), size);
    }
    
    Vector(const Vector& other)
        : data_(other.size_)
        , size_(other.size_)
    {
        std::uninitialized_copy_n(other.data_.GetAddress(), other.size_, data_.GetAddress());
    }
    
    Vector(Vector&& other) noexcept {
        Swap(other);
    }

    Vector& operator=(const Vector& rhs) {
        if (this != &rhs) {
            if (rhs.size_ > data_.Capacity()) {
                Vector rhs_copy(rhs);
                Swap(rhs_copy);
            } else {
                std::copy(rhs.data_.GetAddress(), rhs.data_.GetAddress() + std::min(rhs.size_, size_), data_.GetAddress());
                if (rhs.size_ < size_) {
                    std::destroy_n(data_.GetAddress() + rhs.size_, size_ - rhs.size_);
                } else {
                    std::uninitialized_copy_n(rhs.data_.GetAddress() + size_, rhs.size_ - size_, data_.GetAddress() + size_);
                }
                size_ = rhs.size_;
            }
        }
        return *this;
    }
    
    Vector& operator=(Vector&& rhs) noexcept {
        if (this != &rhs) {
            Swap(rhs);
        }
        return *this;
    }
    
    ~Vector() {
        std::destroy_n(data_.GetAddress(), size_);
    }
    
    using iterator = T*;
    using const_iterator = const T*;
    
    iterator begin() noexcept {
        return data_.GetAddress();
    }
    
    iterator end() noexcept {
        return data_.GetAddress() + size_;
    }
    
    const_iterator begin() const noexcept {
        return const_iterator(data_.GetAddress());
    }
    
    const_iterator end() const noexcept {
        return const_iterator(data_.GetAddress() + size_);
    }
    
    const_iterator cbegin() const noexcept {
        return data_.GetAddress();
    }
    
    const_iterator cend() const noexcept {
        return data_.GetAddress() + size_;
    }
    
    void Swap(Vector& other) noexcept {
        data_.Swap(other.data_);
        std::swap(size_, other.size_);
    }
    
    size_t Size() const noexcept {
        return size_;
    }

    size_t Capacity() const noexcept {
        return data_.Capacity();
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<Vector&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < size_);
        return data_[index];
    }
    
    void Reserve(size_t new_capacity) {
        if (new_capacity <= data_.Capacity()) {
            return;
        }
        
        RawMemory<T> new_data(new_capacity);
        IsNothrowMoveOrCopyConstructible(std::move(new_data));
        std::destroy_n(data_.GetAddress(), size_);
        data_.Swap(new_data);
    }
    
    void Resize(size_t new_size) {
        if (new_size < size_) {
            std::destroy_n(data_.GetAddress() + new_size, size_ - new_size);
        } else {
            Reserve(new_size);
            std::uninitialized_value_construct_n(data_.GetAddress() + size_, new_size - size_);
        }
        size_ = new_size;
    }
    
    void PushBack(const T& value) {
        EmplaceBack(std::forward<const T&>(value));
    }
 
    void PushBack(T&& value) {
        EmplaceBack(std::forward<T&&>(value));
    }
    
    void PopBack() noexcept {
        assert(size_);
        std::destroy_at(data_.GetAddress() + size_ - 1);
        --size_;
    }
    
    template <typename... Args>
    T& EmplaceBack(Args&&... args) {
        return *Emplace(end(), std::forward<Args&&>(args)...);
    }
    
    template <typename... Args>
    iterator Emplace(const_iterator pos, Args&&... args) {
        assert(pos >= begin() && pos <= end());
        size_t shift = pos - begin();
        if (size_ == Capacity()) {
            EmplaceWithRealocation(shift, std::forward<Args>(args)...);
        } else {
            EmplaceWithoutRealocation(shift, pos, std::forward<Args>(args)...);
        }
        ++size_;
        return begin() + shift;
    }
    
    iterator Erase(const_iterator pos) noexcept(std::is_nothrow_move_assignable_v<T>) {
        assert(pos >= begin() && pos <= end());
        size_t shift = pos - begin();
        std::move(begin() + shift + 1, end(), begin() + shift);
        PopBack();
        return begin() + shift;
    }
    
    iterator Insert(const_iterator pos, const T& value) {
        return Emplace(pos, value);
    }
    
    iterator Insert(const_iterator pos, T&& value) {
        return Emplace(pos, std::move(value));
    }

private:
    RawMemory<T> data_;
    size_t size_ = 0;
    
    void IsNothrowMoveOrCopyConstructible(RawMemory<T>&& new_data) {
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
        } else {
            std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
        }
    }

    template <typename... Args>
    void EmplaceWithRealocation(size_t shift, Args&&... args) {
        RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
        new (new_data.GetAddress() + shift) T(std::forward<Args>(args)...);
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(data_.GetAddress(), shift, new_data.GetAddress());
            std::uninitialized_move_n(data_.GetAddress() + shift, size_ - shift, new_data.GetAddress() + shift + 1);
        } else {
            std::uninitialized_copy_n(data_.GetAddress(), shift, new_data.GetAddress());
            std::uninitialized_copy_n(data_.GetAddress() + shift, size_ - shift, new_data.GetAddress() + shift + 1);
        }
        std::destroy_n(data_.GetAddress(), size_);
        data_.Swap(new_data);
    }
    
    template <typename... Args>
    void EmplaceWithoutRealocation(size_t shift, const_iterator pos, Args&&... args) {
        if (pos == end()) {
            new (end()) T(std::forward<Args>(args)...);
        } else {
            T tmp(std::forward<Args>(args)...);
            new (end()) T(std::forward<T>(data_[size_ - 1]));
            std::move_backward(begin() + shift, end() - 1, end());
            *(begin() + shift) = std::forward<T>(tmp);
        }
    } 
};