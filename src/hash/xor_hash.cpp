/**
 * xor_hash.cpp
 *
 *  Created on: Mar 9, 2012
 *      Author: auxten
 **/

#include <pthread.h>
#include "xor_hash.h"
#include "../gingko.h"
#include "../log.h"
#include "../limit.h"

/**
 * @brief xor hash a given length buf
 *
 * @see
 * @note
 *     if hval is not 0, use it as the init hash value
 * @author auxten  <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
unsigned xor_hash(const void *key, int len, unsigned hval)
{
#if defined(ROT_XOR_HASH)
    u_char *p = (u_char *) key;
    hval = hval ? hval : 2166136261;
#if defined(HASH_BYTE_NUM_ONCE)
    for (int i = 0; i <= len - HASH_BYTE_NUM_ONCE; i += HASH_BYTE_NUM_ONCE)
    {
        hval = ROLL(hval) ^ p[i];
        hval = ROLL(hval) ^ p[i + 1];
        hval = ROLL(hval) ^ p[i + 2];
        hval = ROLL(hval) ^ p[i + 3];
#if HASH_BYTE_NUM_ONCE == 8
        hval = ROLL(hval) ^ p[i + 4];
        hval = ROLL(hval) ^ p[i + 5];
        hval = ROLL(hval) ^ p[i + 6];
        hval = ROLL(hval) ^ p[i + 7];
#endif /** HASH_BYTE_NUM_ONCE == 8 **/
    }
    /**
     * hash the remained bytes
     **/
    for (int i = len - len % HASH_BYTE_NUM_ONCE; i < len; i++)
    {
        hval = ROLL(hval) ^ p[i];
    }
#else
    for (int i = 0; i < len; i++)
    {
        hval = ROLL(hval) ^ p[i];
    }
#endif

#elif defined(FNV_XOR_HASH)
    u_char *p = (u_char *) key;
    hval = hval ? hval : 2166136261;

    for (int i = 0; i < len; i++)
    {
#if defined(NO_SO_CALLED_FNV_OPTIMIZE)
        hval = (hval * 16777619) ^ p[i];
#else
        hval += (hval << 1) + (hval << 4) + (hval << 7) + (hval << 8) + (hval
                << 24);
        hval ^= p[i];
#endif
    }

    return hval;
#endif /** ROT_XOR_HASH **/
    return hval;
}

