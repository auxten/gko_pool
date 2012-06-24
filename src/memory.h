/*
 * memory.h
 *
 *  Created on: Jun 8, 2012
 *      Author: auxten
 */

#ifndef GKO_MEMORY_H_
#define GKO_MEMORY_H_

#include <sys/types.h>

static const u_int32_t      SLOT_SIZE       =   4 * 1024;                   /// one page is good
static const u_int32_t      SLOT_COUNT      =   1 * 1024 * 1024;
static const u_int32_t      M_MAP_SIZE      =   SLOT_COUNT / sizeof(u_int8_t);  /// bitmap
static const u_int32_t      BUCKET_SIZE     =   16 * 1024 * 1024;
static const int32_t        BUCKET_CAPACITY =   BUCKET_SIZE / SLOT_SIZE; /// capacity, 4096
static const int32_t        BUCKET_COUNT    =   SLOT_COUNT / BUCKET_CAPACITY; /// 256
static const int            INVILID_BLOCK   =   -1;


class gkoAlloc
{
private:
    u_int8_t m_map[M_MAP_SIZE]; /// 1MB can fit L2 cache
    void *  bucket_s[BUCKET_COUNT];
    int16_t bucket_used[BUCKET_COUNT];
    int latest_bucket;
    int get_bit(u_int8_t * b);
    int free_bit(u_int8_t * b, int index);

public:
    gkoAlloc(void);
    int get_block(void);
    int get_clear_block(void);
    int get2x_block(int block_id);
    void free_block(int block_id);
    int clear_block(void *block, int c, size_t size);
    void * id2addr(int block_id);
};

#endif /* GKO_MEMORY_H_ */
