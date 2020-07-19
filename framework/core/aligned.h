#pragma once
#include <xmmintrin.h>
#include <string>

namespace core
{
    template<size_t alignment>
    class alignas(alignment) Aligned
    {
    public:
        void *operator new(size_t size)
        {
            void *ptr = _mm_malloc(size, alignment);
            if (!ptr)
                throw std::bad_alloc();
            return ptr;
        }

        void operator delete(void *ptr) noexcept
        {
            _mm_free(ptr);
        }
    };
} // namespace core
