#pragma once
#include <cassert>
#include <cstdlib>
#include <new>
#include <utility>
#include <memory>
#include <algorithm>
#include <iterator>

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

    RawMemory(RawMemory&& other) noexcept {
        Swap(other);
    }

    RawMemory& operator=(RawMemory&& rhs) noexcept {
        if (buffer_ != rhs.buffer_) {
            Swap(rhs);
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
    // Allocates raw memory for n elements and returns a pointer to it
    static T* Allocate(size_t n) {
        return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
    }

    // Frees the raw memory allocated earlier at the buf address using Allocate
    static void Deallocate(T* buf) noexcept {
        operator delete(buf);
    }

    T* buffer_ = nullptr;
    size_t capacity_ = 0;
};


template <typename T>
class Vector {
public:
    using iterator = T*;
    using const_iterator = const T*;

    iterator begin() noexcept {
        return data_.GetAddress();
    }
    iterator end() noexcept {
        return data_ + size_;
    }

    const_iterator begin() const noexcept {
        return cbegin();
    }
    const_iterator end() const noexcept {
        return cend();
    }

    const_iterator cbegin() const noexcept {
        return data_.GetAddress();
    }
    const_iterator cend() const noexcept {
        return data_ + size_;
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

    Vector() = default;

    explicit Vector(size_t size)
        : data_(size)
        , size_(size) {
        std::uninitialized_value_construct_n(data_.GetAddress(), size);
    }

    Vector(const Vector& other)
        : data_(other.size_)
        , size_(other.size_) {
        std::uninitialized_copy_n(other.data_.GetAddress(), size_, data_.GetAddress());
    }

    ~Vector() noexcept {
        std::destroy_n(data_.GetAddress(), size_);
    }

    void Reserve(size_t new_capacity);

    Vector(Vector&& other) noexcept {
        Swap(other);
    }

    Vector& operator=(const Vector& rhs);

    Vector& operator=(Vector&& rhs) noexcept;

    void Swap(Vector& other) noexcept {
        std::swap(size_, other.size_);
        data_.Swap(other.data_);
    }

    void Resize(size_t new_size);

    void PushBack(const T& value) {
        EmplaceBack(value);
    }

    void PushBack(T&& value) {
        EmplaceBack(std::move(value));
    }

    void PopBack() noexcept;

    template <typename... Args>
    T& EmplaceBack(Args&&... args);

    template <typename... Args>
    iterator Emplace(const_iterator pos, Args&&... args);

    iterator Erase(const_iterator pos) noexcept(std::is_nothrow_move_assignable_v<T>);

    iterator Insert(const_iterator pos, const T& value) {
        return Emplace(pos, value);
    }
    iterator Insert(const_iterator pos, T&& value) {
        return Emplace(pos, std::move(value));
    }

private:
    void SwapRealocation(RawMemory<T>& new_data);

    template <typename... Args>
    void Realocation(size_t position_new_element, Args&&... args);

    template <typename... Args>
    void Moving(size_t position_new_element, Args&&... args);

private:
    RawMemory<T> data_;
    size_t size_ = 0;
};


template <typename T>
void Vector<T>::Reserve(size_t new_capacity) {
    if (new_capacity <= data_.Capacity()) {
        return;
    }
    RawMemory<T> new_data(new_capacity);
    SwapRealocation(new_data);
}

template <typename T>
Vector<T>& Vector<T>::operator=(const Vector& rhs) {
    if (this == &rhs) {
        return *this;
    }
    if (rhs.size_ > data_.Capacity()) {
        Vector rhs_copy(rhs);
        Swap(rhs_copy);
    } else {
        if (size_ == rhs.size_) {
            std::copy(rhs.data_.GetAddress(), rhs.data_ + size_, data_.GetAddress());
            return *this;
        }
        size_t size_for_copy = std::min(size_, rhs.size_);
        std::copy(rhs.data_.GetAddress(), rhs.data_ + size_for_copy, data_.GetAddress());
        if (size_ > rhs.size_) {
            std::destroy_n(data_ + size_for_copy, size_ - rhs.size_);
        } else {
            std::uninitialized_copy_n(rhs.data_ + size_for_copy, rhs.size_ - size_, data_ + size_for_copy);
        }
        size_ = rhs.size_;
    }
    return *this;
}

template <typename T>
Vector<T>& Vector<T>::operator=(Vector&& rhs) noexcept {
    if (this != &rhs) {
        Swap(rhs);
    }
    return *this;
}

template <typename T>
void Vector<T>::Resize(size_t new_size) {
    if (size_ == new_size) {
        return;
    }
    if (size_ > new_size) {
        std::destroy_n(data_ + new_size, size_ - new_size);
    } else {
        Reserve(new_size);
        std::uninitialized_value_construct_n(data_ + size_, new_size - size_);
    }
    size_ = new_size;
}

template <typename T>
void Vector<T>::PopBack() noexcept {
    if (size_ != 0) {
        std::destroy_at(data_ + (size_ - 1));
        --size_;
    }
}

template <typename T>
template <typename... Args>
T& Vector<T>::EmplaceBack(Args&&... args) {
    if (size_ == Capacity()) {
        RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
        new (new_data + size_) T(std::forward<Args>(args)...);
        SwapRealocation(new_data);
    } else {
        new (data_ + size_) T(std::forward<Args>(args)...);
    }
    ++size_;
    return data_[size_ - 1];
}

template <typename T>
template <typename... Args>
typename Vector<T>::iterator Vector<T>::Emplace(const_iterator pos, Args&&... args) {
    // Alternatively, it would be possible to call "Emplace Back" to "Emplace" by passing end() to it,
    // but the current implementation reduces the load on this method
    if (pos == cend()) {
        return &EmplaceBack(std::forward<Args>(args)...);
    }
    size_t position_new_element = pos - cbegin();
    if (size_ == Capacity()) {
        Realocation(position_new_element, std::forward<Args>(args)...);
    } else {
        Moving(position_new_element, std::forward<Args>(args)...);
    }
    ++size_;
    return &data_[position_new_element];
}

template <typename T>
typename Vector<T>::iterator Vector<T>::Erase(const_iterator pos) noexcept(std::is_nothrow_move_assignable_v<T>) {
    assert(pos >= begin() && pos < end());
    size_t index_erase_element = pos - cbegin();
    iterator position_erase_element = data_ + index_erase_element;
    if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
        std::move(position_erase_element + 1, end(), position_erase_element);
    } else {
        std::copy(position_erase_element + 1, end(), position_erase_element);
    }
    std::destroy_at(std::prev(end()));
    --size_;
    return position_erase_element;
}

template <typename T>
void Vector<T>::SwapRealocation(RawMemory<T>& new_data) {
    // If the move constructor of type T does not throw exceptions or type T does not have a copy constructor
    if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
        std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
    } else {
        std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
    }
    std::destroy_n(data_.GetAddress(), size_);
    data_.Swap(new_data);
}

template <typename T>
template <typename... Args>
void Vector<T>::Realocation(size_t position_new_element, Args&&... args) {
    RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
    new (new_data + position_new_element) T(std::forward<Args>(args)...);

    if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
        std::uninitialized_move_n(data_.GetAddress(), position_new_element, new_data.GetAddress());
        std::uninitialized_move_n(data_ + position_new_element, size_ - position_new_element, new_data + position_new_element + 1);
    } else {
        std::uninitialized_copy_n(data_.GetAddress(), position_new_element, new_data.GetAddress());
        std::uninitialized_copy_n(data_ + position_new_element, size_ - position_new_element, new_data + position_new_element + 1);
    }

    std::destroy_n(data_.GetAddress(), size_);
    data_.Swap(new_data);
}

template <typename T>
template <typename... Args>
void Vector<T>::Moving(size_t position_new_element, Args&&... args) {
    T tmp(std::forward<Args>(args)...);
    new (data_ + size_) T(std::move(*std::prev(end())));
    std::move_backward(data_ + position_new_element, end() - 1, end());
    data_[position_new_element] = std::move(tmp);
}


namespace tests {
    void Test1();
    void Test2();
    void Test3();
    void Test4();
    void Test5();
    void Test6();
    void Benchmark();
    void RunAllTests();
} // namespace tests
