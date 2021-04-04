#pragma once

#include <utility>

namespace GoldenSun
{
    template <typename T>
    ImplPtr<T>::ImplPtr() : ptr_(std::make_unique<T>())
    {
    }

    template <typename T>
    template <typename... Args>
    ImplPtr<T>::ImplPtr(Args&&... args) : ptr_(std::make_unique<T>(std::forward<Args>(args)...))
    {
    }

    template <typename T>
    template <typename U>
    ImplPtr<T>::ImplPtr(std::unique_ptr<U> ptr) : ptr_(std::move(ptr))
    {
    }

    template <typename T>
    ImplPtr<T>::~ImplPtr() noexcept = default;

    template <typename T>
    ImplPtr<T>::ImplPtr(ImplPtr&& other) noexcept = default;

    template <typename T>
    ImplPtr<T>& ImplPtr<T>::operator=(ImplPtr&& other) noexcept = default;

    template <typename T>
    template <typename U>
    ImplPtr<T>::ImplPtr(ImplPtr<U>&& other) noexcept : ptr_(std::move(other.ptr_))
    {
    }

    template <typename T>
    template <typename U>
    ImplPtr<T>& ImplPtr<T>::operator=(ImplPtr<U>&& other) noexcept
    {
        if (ptr_ != other.ptr_)
        {
            ptr_ = std::move(other.ptr_);
        }
        return *this;
    }

    template <typename T>
    ImplPtr<T>::operator bool() const noexcept
    {
        return ptr_ ? true : false;
    }

    template <typename T>
    T& ImplPtr<T>::operator*() const noexcept
    {
        return *ptr_;
    }

    template <typename T>
    T* ImplPtr<T>::operator->() const noexcept
    {
        return ptr_.get();
    }
} // namespace GoldenSun
