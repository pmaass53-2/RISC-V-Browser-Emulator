#ifndef CLINT_HPP
#define CLINT_HPP

class CLINT {
    public:
        uint32_t MSIP = 0;
        uint32_t MTIP = 0;
        uint64_t MTIMECMP = 0;
        uint64_t MTIME = 0;

        inline uint32_t read_reg(uint32_t offset) const {
            switch (offset) {
                case 0x0000:
                    return MSIP;
                    break;
                case 0x4000:
                    return MTIMECMP & 0xFFFFFFFF;
                    break;
                case 0x4004:
                    return (MTIMECMP >> 32) & 0xFFFFFFFF;
                    break;
                case 0xBFF8:
                    return MTIME & 0xFFFFFFFF;
                    break;
                case 0xBFFC:
                    return (MTIME >> 32) & 0xFFFFFFFF;
                    break;
                default:
                    return 0;
                    break;
            }
        }
        inline void write_reg(uint32_t offset, uint32_t val) {
            switch (offset) {
                case 0x0000:
                    // MSIP
                    MSIP = val & 1;
                    break;
                case 0x4000:
                    // MTIMECMP LOW
                    MTIMECMP = (MTIMECMP & 0xFFFFFFFF00000000ULL) | static_cast<uint64_t>(val);
                    break;
                case 0x4004:
                    // MTIMECMP HIGH
                    MTIMECMP = (MTIMECMP & 0xFFFFFFFFULL) | (static_cast<uint64_t>(val) << 32);
                    break;
                case 0xBFF8:
                    // MTIME LOW
                    MTIME = (MTIME & 0xFFFFFFFF00000000ULL) | static_cast<uint64_t>(val);
                    break;
                case 0xBFFC:
                    // MTIME HIGH
                    MTIME = (MTIME & 0xFFFFFFFFULL) | (static_cast<uint64_t>(val) << 32);
                    break;
                default:
                    // unrecognised instruction
                    break;
            }
        }
        void tick() {
            MTIME++;
            if (MTIME >= MTIMECMP) {
                MTIP = 1;
            } else {
                MTIP = 0;
            }
        }
};

#endif