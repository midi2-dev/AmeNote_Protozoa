#ifndef UMPRINGBUFFER_H
#define UMPRINGBUFFER_H

#include <midi/universal_packet.h>

template <uint16_t capacity>
struct UMPRingBuffer
{
    inline void write(const midi::universal_packet &p)
    {
        data[writePtr++] = p;
        if (writePtr >= capacity)
            writePtr = 0;
    }
    inline bool itemsAvail(uint16_t readPtr) const { return (readPtr != writePtr); }
    inline bool read(uint16_t &readPtr, midi::universal_packet &p) const
    {
        if (itemsAvail(readPtr))
        {
            p = data[readPtr];
            if (++readPtr == capacity)
                readPtr = 0;
            return true;
        }

        return false;
    }
    inline void resetReadPtr(uint16_t &readPtr) const { readPtr = writePtr; }

private:
    uint16_t writePtr = 0;
    midi::universal_packet data[capacity];
};

#endif // UMPRINGBUFFER_H
