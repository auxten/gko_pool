/*
 * memory.cpp
 *
 *  Created on: Jun 8, 2012
 *      Author: auxten
 *
 *  only support little endian : x86
 */

//#define MEM_TEST
#define _XOPEN_SOURCE 600
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <sys/types.h>

#include "memory.h"

#include "log.h"

gkoAlloc::gkoAlloc(void)
{
    memset((void *) m_map, 0, M_MAP_SIZE * sizeof(u_int8_t));
    memset((void *) bucket_s, 0, BUCKET_COUNT * sizeof(void *));
    memset((void *) bucket_used, 0, BUCKET_COUNT * sizeof(int16_t));
    latest_bucket = 0;
}

void * gkoAlloc::id2addr(int block_id)
{
    if (block_id < 0)
        return NULL;

    int bucket_no = block_id / BUCKET_CAPACITY;
    int bucket_offset = SLOT_SIZE * (block_id % BUCKET_CAPACITY);

    return ((char *)bucket_s[bucket_no]) + bucket_offset;
}

int gkoAlloc::get_bit(u_int8_t * b)
{
    /**
     *  idx     01234567
     *  byte    11111010
     *
     *  return 5 and
     *  byte    11111110
     */

    int i;
    for (i = 0; i < 8; i++)
    {
        if ((u_int8_t)((*b >> 7 - i) << 7) == (u_int8_t)0u)
            break;
    }

    *b |= (u_int8_t)( 1u << 7 - i);

    return i;
}

int gkoAlloc::free_bit(u_int8_t * b, int index)
{
    /**
     *  idx     01234567
     *  byte    11111110
     *
     *  return 5 and
     *  byte    11111010
     */

    *b ^= (u_int8_t)( 1u << 7 - index);

    return index;
}

int gkoAlloc::get_block(void)
{
    int i;
    int the_bucket;
    int idx;

    for (i = 0; i < BUCKET_COUNT; i++)
    {
        the_bucket = (latest_bucket + i) % BUCKET_COUNT;
        if (bucket_used[the_bucket] < BUCKET_CAPACITY)
        {
            latest_bucket = the_bucket;
            break;
        }
    }

    if (i == BUCKET_COUNT)
    {
        fprintf(stderr, "out of memory in pool\n");
//        GKOLOG(FATAL, "out of memory in pool");
        return INVILID_BLOCK;
    }

    if (!bucket_s[the_bucket])
    {
        void * ptr;
        if (!posix_memalign(&ptr, SLOT_SIZE, BUCKET_SIZE))
        {
            bucket_s[the_bucket] = ptr;
            bucket_used[the_bucket] = 0;
        }
        else
        {
            fprintf(stderr, "posix_memalign fail\n");
//            GKOLOG(FATAL, "posix_memalign fail");
            return INVILID_BLOCK;
        }
    }

    u_int8_t * p_idx;
    u_int64_t * bucket_start_idx = (u_int64_t *) &(this->m_map[the_bucket * BUCKET_CAPACITY / 8]);
    u_int64_t * bucket_end_idx = (u_int64_t *) &(this->m_map[(the_bucket + 1) * BUCKET_CAPACITY / 8]);
    for (u_int64_t * bucket_idx = bucket_start_idx;
            bucket_idx < bucket_end_idx;
            bucket_idx++)
    {
        if (*(u_int64_t *) bucket_idx != ~0uLL)
        {
            if (*(u_int32_t *) bucket_idx != ~0u)
            {
                if (*((u_int16_t *) bucket_idx) != (u_int16_t) ~0u)
                {
                    if (*(u_int8_t *) bucket_idx != (u_int8_t) ~0u)
                    {
                        p_idx = (u_int8_t *) bucket_idx + 0;
                    }
                    else
                    {
                        p_idx = (u_int8_t *) bucket_idx + 1;
                    }
                }
                else
                {
                    if (*((u_int8_t *) bucket_idx + 2) != (u_int8_t) ~0u)
                    {
                        p_idx = (u_int8_t *) bucket_idx + 2;
                    }
                    else
                    {
                        p_idx = (u_int8_t *) bucket_idx + 3;
                    }

                }
            }
            else
            {
                if (*((u_int16_t *) bucket_idx + 2) != (u_int16_t) ~0u)
                {
                    if (*((u_int8_t *) bucket_idx + 4) != (u_int8_t) ~0u)
                    {
                        p_idx = (u_int8_t *) bucket_idx + 4;
                    }
                    else
                    {
                        p_idx = (u_int8_t *) bucket_idx + 5;
                    }
                }
                else
                {
                    if (*((u_int8_t *) bucket_idx + 6) != (u_int8_t) ~0u)
                    {
                        p_idx = (u_int8_t *) bucket_idx + 6;
                    }
                    else
                    {
                        p_idx = (u_int8_t *) bucket_idx + 7;
                    }

                }
            }
            idx = get_bit(p_idx) +
                    8 * (p_idx - (u_int8_t *) bucket_start_idx) +
                    the_bucket * BUCKET_CAPACITY;
            bucket_used[the_bucket] ++;
            break;
        }
        else
        {
            continue;
        }
    }
    return idx;
}

void gkoAlloc::free_block(int block_id)
{
    if (block_id < 0)
        return;

    int bucket_no = block_id / BUCKET_CAPACITY;
    free_bit(&m_map[block_id / 8], block_id % 8);

    if(--bucket_used[bucket_no] == 0)
    {
        free(bucket_s[bucket_no]);
        bucket_s[bucket_no] = NULL;
    }

}

#ifdef MEM_TEST
int main()
{
    gkoAlloc mem;
    for (int i = 0; i < BUCKET_CAPACITY - 1; i++)
    {
        int k = mem.get_block();
        printf("%d, %d\n", i, k);
        if (i != k)
        {
            break;
        }
    }
    int blk1 = mem.get_block();
    int blk2 = mem.get_block();
    int blk3 = mem.get_block();
    printf("%p\n", mem.id2addr(blk1));
    printf("%p\n", mem.id2addr(blk2));
    printf("%p\n", mem.id2addr(blk3));
    mem.free_block(blk1);
    mem.free_block(blk2);
    mem.free_block(blk3);
    return 0;
}
#endif
