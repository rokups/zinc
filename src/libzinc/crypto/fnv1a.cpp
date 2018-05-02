/**
 *  Implementation taken from http://www.isthe.com/chongo/tech/comp/fnv/ and is in public domain.
 */
#include "fnv1a.h"


namespace zinc
{

// Implementation taken from http://www.isthe.com/chongo/tech/comp/fnv/
uint64_t fnv1a64(const void* data, size_t dlen)
{
    const uint8_t* p = reinterpret_cast<const uint8_t*>(data);
    uint64_t hash = 0xcbf29ce484222325;

    for (size_t i = 0; i < dlen; i++)
    {
        hash = hash ^ p[i];
//        hash = hash * 0x100000001b3;
        hash += (hash << 1) + (hash << 4) + (hash << 5) +
                (hash << 7) + (hash << 8) + (hash << 40);
    }
    return hash;
}

}
