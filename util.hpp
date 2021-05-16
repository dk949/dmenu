/* See LICENSE file for copyright and license details. */

#include <cstddef>
constexpr auto MAX(auto A, auto B) {
    return (static_cast<long>(A) > static_cast<long>(B)) ? static_cast<long>(A) : static_cast<long>(B);
}

constexpr auto MIN(auto A, auto B) {
    return (static_cast<long>(A) < static_cast<long>(B)) ? static_cast<long>(A) : static_cast<long>(B);
}

constexpr auto BETWEEN(auto X, auto A, auto B) {
    return static_cast<long>(A) <= static_cast<long>(X) && static_cast<long>(X) <= static_cast<long>(B);
}

void die(const char *fmt, ...);
void *ecalloc(size_t nmemb, size_t size);
