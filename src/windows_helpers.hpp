#ifndef WINDOWS_HELPERS_HPP_INCLUDED
#define WINDOWS_HELPERS_HPP_INCLUDED

#if defined(_MSC_VER)
#define strtoll _strtoi64

inline double round(double r) {
    return (r > 0.0) ? floor(r + 0.5) : ceil(r - 0.5);
}
#endif

#endif