#pragma once
#include <string>

#ifndef TEXT
#ifdef UNICODE
#define TEXT(s) L##s
#else
#define TEXT(s) s
#endif
#endif // !TEXT

namespace core
{
#ifdef UNICODE
    typedef std::basic_string<wchar_t> tstring;
#else
    typedef std::basic_string<char> tstring;
#endif
    template <typename T>
    inline tstring to_tstring(T val)
    {
#ifdef UNICODE
        return std::to_wstring(val);
#else
        return std::to_string(val);
#endif
    }
} // namespace core

