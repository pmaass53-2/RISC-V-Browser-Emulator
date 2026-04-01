#ifndef RAM_HPP
#define RAM_HPP

#include <cstdint>
#include <cstring>

#define MEMORY_SIZE 0x04000000

class RAM {
    // 16-byte alignment for SIMD
    uint8_t *data;

    public:
        RAM() : data(new uint8_t[MEMORY_SIZE]()) {}
        ~RAM() { delete[] data; }
        inline void load(const uint8_t *value, uint32_t size, uint32_t offset = 0) noexcept {
            if (offset >= MEMORY_SIZE) return;
            if (offset + size > MEMORY_SIZE) size = MEMORY_SIZE - offset;
            std::memcpy(data + offset, value, size);
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