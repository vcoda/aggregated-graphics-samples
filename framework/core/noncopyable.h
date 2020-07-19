#pragma once

namespace core
{
    class NonCopyable
    {
    public:
        NonCopyable() = default;
        virtual ~NonCopyable() = default;

    private:
        NonCopyable(const NonCopyable&) = delete;
        NonCopyable& operator=(const NonCopyable&) = delete;
    };
} // namespace core
