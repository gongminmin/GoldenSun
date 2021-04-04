#pragma once

#include <stdexcept>
#include <string_view>

namespace GoldenSun
{
    std::string CombineFileLine(std::string_view file, uint32_t line);
    std::string CombineFileLine(HRESULT hr, std::string_view file, uint32_t line);
    void Verify(bool value);

    class HrException : public std::runtime_error
    {
    public:
        HrException(HRESULT hr, std::string_view file, uint32_t line)
            : std::runtime_error(CombineFileLine(hr, std::move(file), line)), hr_(hr)
        {
        }

        HRESULT Error() const noexcept
        {
            return hr_;
        }

    private:
        HRESULT const hr_;
    };
} // namespace GoldenSun


#define TIFHR(hr)                                                 \
    {                                                             \
        if (FAILED(hr))                                           \
        {                                                         \
            throw GoldenSun::HrException(hr, __FILE__, __LINE__); \
        }                                                         \
    }
