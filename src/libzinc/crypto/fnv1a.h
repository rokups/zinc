/**
 *  Implementation taken from http://www.isthe.com/chongo/tech/comp/fnv/ and is in public domain.
 */
#pragma once


#include <stdint.h>
#include <string.h>


namespace zinc
{

uint64_t fnv1a64(const void* data, size_t dlen);

}
