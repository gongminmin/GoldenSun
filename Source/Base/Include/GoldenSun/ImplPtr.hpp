#pragma once

#include <memory>

namespace GoldenSun
{
    template <typename T>
    class ImplPtr final
    {
        template <typename U>
        friend class ImplPtr;

        DISALLOW_COPY_AND_ASSIGN(ImplPtr)

    public:
        ImplPtr();

        template <typename... Args>
        ImplPtr(Args&&...);

        template <typename U>
        explicit ImplPtr(std::unique_ptr<U> ptr);

        ~ImplPtr() noexcept;

        ImplPtr(ImplPtr&& other) noexcept;
        ImplPtr& operator=(ImplPtr&& other) noexcept;

        template <typename U>
        ImplPtr(ImplPtr<U>&& other) noexcept;
        template <typename U>
        ImplPtr& operator=(ImplPtr<U>&& other) noexcept;

        explicit operator bool() const noexcept;

        T& operator*() const noexcept;
        T* operator->() const noexcept;

    private:
        std::unique_ptr<T> ptr_;
    };
} // namespace GoldenSun
