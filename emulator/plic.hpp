#ifndef PLIC_HPP
#define PLIC_HPP

#include <cstdint>

class PLIC {
    public:
        static constexpr uint32_t ID_UART = 0x0000000A;

        uint32_t MEIP = 0;
        uint32_t SEIP = 0;
        // "global" registers
        uint32_t priority[1024] = {0};
        uint32_t pending[32] = {0};
        uint32_t active[32] = {0};
        // machine registers
        uint32_t m_enable[32] = {0};
        uint32_t m_threshold = 0;
        uint32_t m_mailbox = 0;
        // supervisor registers
        uint32_t s_enable[32] = {0};
        uint32_t s_threshold = 0;
        uint32_t s_mailbox = 0;

        inline uint32_t read_reg(uint32_t offset) {
            if (offset <= 0x000FFF) {
                return priority[offset >> 2];
            } else if (offset >= 0x001000 && offset <= 0x00107F) {
                // pending bits
                return pending[(offset - 0x1000) >> 2];
            } else if (offset >= 0x002000 && offset <= 0x1FFFFF) {
                // enable bits
                uint32_t contextid = (offset - 0x002000) >> 7;
                switch (contextid) {
                    case 0:
                        // Hart 0 Privilege M
                        return m_enable[((offset - 0x002000) & 0x7F) >> 2];
                    case 1:
                        // Hart 0 Privilege S
                        return s_enable[((offset - 0x002000) & 0x7F) >> 2];
                }
            } else if (offset >= 0x200000) {
                // threshold/mailbox
                uint32_t contextid = (offset - 0x200000) >> 12;
                switch (contextid) {
                    case 0:
                        // Hart 0 Privilege M
                        if (offset % 0x1000 == 0) {
                            return m_threshold;
                        } else {
                            uint32_t claim_id = 0;
                            uint32_t max_priority = 0;
                            for (uint32_t i = 0; i < 32; i++) {
                                uint32_t ready = pending[i] & ~active[i] & m_enable[i];
                                if (ready == 0) {
                                    continue;
                                }
                                for (uint32_t bit = 0; bit < 32; bit++) {
                                    if (ready & (1 << bit)) {
                                        uint32_t id = (i * 32) + bit;
                                        if (id > 0 && priority[id] > m_threshold) {
                                            if (priority[id] > max_priority) {
                                                max_priority = priority[id];
                                                claim_id = id;
                                            }
                                        }
                                    }
                                }
                            }
                            // Mark the interrupt as active
                            if (claim_id > 0) {
                                active[claim_id / 32] |= (1 << (claim_id % 32));
                            }
                            update();
                            return claim_id;
                        }
                    case 1:
                        // Hart 0 Privilege S
                        if (offset % 0x1000 == 0) {
                            return s_threshold;
                        } else {
                            uint32_t claim_id = 0;
                            uint32_t max_priority = 0;
                            for (uint32_t i = 0; i < 32; i++) {
                                uint32_t ready = pending[i] & ~active[i] & s_enable[i];
                                if (ready == 0) {
                                    continue;
                                }
                                for (uint32_t bit = 0; bit < 32; bit++) {
                                    if (ready & (1 << bit)) {
                                        uint32_t id = (i * 32) + bit;
                                        if (id > 0 && priority[id] > s_threshold) {
                                            if (priority[id] > max_priority) {
                                                max_priority = priority[id];
                                                claim_id = id;
                                            }
                                        }
                                    }
                                }
                            }
                            // Mark the interrupt as active
                            if (claim_id > 0) {
                                active[claim_id / 32] |= (1 << (claim_id % 32));
                            }
                            update();
                            return claim_id;
                        }
                }
            }
            return 0; 
        }
        inline void write_reg(uint32_t offset, uint32_t val) {
            if (offset <= 0x000FFF) {
                priority[offset >> 2] = val;
            } else if (offset >= 0x002000 && offset <= 0x1FFFFF) {
                // enable bits
                uint32_t contextid = (offset - 0x002000) >> 7;
                switch (contextid) {
                    case 0:
                        // Hart 0 Privilege M
                        m_enable[((offset - 0x002000) & 0x7F) >> 2] = val;
                        break;
                    case 1:
                        // Hart 0 Privilege S
                        s_enable[((offset - 0x002000) & 0x7F) >> 2] = val;
                        break;
                }
            } else if (offset >= 0x200000) {
                // threshold/mailbox
                uint32_t contextid = (offset - 0x200000) >> 12;
                switch (contextid) {
                    case 0:
                        // Hart 0 Privilege M
                        if (offset % 0x1000 == 0) {
                            m_threshold = val;
                        } else {
                            // Completion
                            if (val > 0 && val < 1024) {
                                active[val / 32] &= ~(1 << (val % 32));
                                pending[val / 32] &= ~(1 << (val % 32));
                            }
                        }
                        break;
                    case 1:
                        // Hart 0 Privilege S
                        if (offset % 0x1000 == 0) {
                            s_threshold = val;
                        } else {
                            // Completion
                            if (val > 0 && val < 1024) {
                                active[val / 32] &= ~(1 << (val % 32));
                                pending[val / 32] &= ~(1 << (val % 32));
                            }
                        }
                        break;
                }
            }
            update();
        }
        inline void update() {
            MEIP = 0;
            SEIP = 0;
            for (uint32_t i = 0; i < 32; i++) {
                uint32_t m_ready = pending[i] & ~active[i] & m_enable[i];
                uint32_t s_ready = pending[i] & ~active[i] & s_enable[i];
                if (m_ready != 0) {
                    for (uint32_t bit = 0; bit < 32; bit++) {
                        if (m_ready & (1 << bit)) {
                            uint32_t id = (i * 32) + bit;
                            if (id > 0 && priority[id] > m_threshold) {
                                MEIP = 1;
                                break;
                            }
                        }
                    }
                }
                if (s_ready != 0) {
                    for (uint32_t bit = 0; bit < 32; bit++) {
                        if (s_ready & (1 << bit)) {
                            uint32_t id = (i * 32) + bit;
                            if (id > 0 && priority[id] > s_threshold) {
                                SEIP = 1;
                                break;
                            }
                        }
                    }
                }
            }
        }
        inline void set_pending(uint32_t id) {
            pending[id/32] |= (1 << (id%32));
            update();
        }
};

#endif