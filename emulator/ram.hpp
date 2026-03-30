#ifndef RAM_HPP
#define RAM_HPP

#include <cstdint>
#include <cstring>

#define MEMORY_SIZE 0x02000000

class RAM {
    // 16-byte alignment for SIMD
    alignas(16) uint8_t data[MEMORY_SIZE];

    public:
        RAM() {
            std::memset(data, 0, MEMORY_SIZE);
        }
        inline void load(const uint8_t *value, uint32_t size) noexcept {
            if (size > MEMORY_SIZE) size = MEMORY_SIZE;
            std::memcpy(data, value, size);
        }
        template <typename T>
        inline T read(uint32_t offset) const noexcept {
            if (offset + sizeof(T) <= MEMORY_SIZE) {
                T val;
                std::memcpy(&val, &data[offset], sizeof(T));
                return val;
            }
            return 0; 
        }
        template<typename T>
        inline void write(uint32_t offset, T value) noexcept {
            if (offset + sizeof(T) <= MEMORY_SIZE) {
                std::memcpy(&data[offset], &value, sizeof(T));
            }
        }
};
#endif