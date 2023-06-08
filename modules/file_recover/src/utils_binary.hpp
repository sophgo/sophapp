#pragma once

#include <cstdint>

namespace cvi_file_recover {
namespace utils {

/**
 * @brief Read intergal from data pointer by big endian
 *
 * @param p is pointer to data
 * @return T value
 */

template <typename T, std::enable_if_t<std::is_integral<T>::value, bool> = true>
inline static T read(const uint8_t *p)
{
    T value = 0;
    constexpr size_t size = sizeof(T);
    for (size_t i = 0; i < size; ++i) {
        value |= (p[i]) << ((size - i - 1)*8);
    }

    return value;
}

/**
 * @brief Read intergal from data pointer by big endian
 *
 * @param iterator is iterator to container
 * @return T value
 */
template <typename T, typename IteratorType, std::enable_if_t<std::is_integral<T>::value, bool> = true>
inline static T read(IteratorType&& iterator)
{
    T value = 0;
    constexpr size_t size = sizeof(T);
    for (size_t i = 0; i < size; ++i) {
        value |= *(iterator + i) << ((size - i - 1)*8);
    }

    return value;
}

/**
 * @brief Write intergal from data pointer by big endian
 *
 * @param p is pointer to data
 * @param T value is the value to write
 */
template <typename T, std::enable_if_t<std::is_integral<T>::value, bool> = true>
inline static void write(uint8_t *p, T value)
{
    constexpr size_t size = sizeof(T);
    for (size_t i = 0; i < size; ++i) {
        p[i] = static_cast<uint8_t>(value >> ((size - i - 1)*8));
    }
}

inline static void write(uint8_t *p, char* value, size_t size)
{
    for (size_t i = 0; i < size; ++i) {
        p[i] = value[i];
    }
}

} // namespace utils
} // namespace cvi_file_recover
