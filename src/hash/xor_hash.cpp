/**
 * xor_hash.cpp
 *
 *  Created on: 2011-5-9
 *      Author: auxten
 **/
#include <pthread.h>
#include "xor_hash.h"
#include "../gingko.h"
#include "../log.h"

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

/**
 * @brief check if the fnv check sum is OK
 *
 * @see
 * @note
 * @author auxten  <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
char digest_ok(void * buf, s_block_t * b)
{
    return (xor_hash(buf, b->size, 0) == b->digest);
}

/**
 * @brief xor hash specified block
 *
 * @see
 * @note
 * @author auxten  <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
unsigned xor_hash_block(s_job_t * jo, GKO_INT64 block_id, u_char * buf)
{
    s_file_t * files = jo->files;
    s_block_t * blocks = jo->blocks;
    GKO_INT64 read_counter = 0;
    GKO_INT64 file_i = (blocks + block_id)->start_f;
    GKO_INT64 offset = 0;
    int fd;
    unsigned tmp_hash = 0;

    if (FAIL_CHECK(-1 == (fd = open((files + ((blocks + block_id)->start_f))->name,
                            O_RDONLY | O_NOFOLLOW))))
    {
        GKOLOG(WARNING, "file open() error!");
    }
    memset(buf, 0, BLOCK_SIZE);
    offset = (blocks + block_id)->start_off;
    while (read_counter < (blocks + block_id)->size)
    {
        GKO_INT64 tmp = pread(fd, buf + read_counter,
                (blocks + block_id)->size - read_counter, offset);
        if (FAIL_CHECK(tmp < 0))
        {
            GKOLOG(WARNING, "pread failed");
        }
        if (LIKELY(tmp))
        {
            ///printf("read: %ld\n", tmp);
            tmp_hash = xor_hash(buf + read_counter, (int) tmp, tmp_hash);
            read_counter += tmp;
            offset += tmp;
        }
        else
        {
            close(fd);
            ///if the next if a nonfile then next
            file_i = next_f(jo, file_i);
            if (FAIL_CHECK(-1
                    == (fd = open(
                                    (files + ((blocks + block_id)->start_f)
                                            + file_i)->name, O_RDONLY | O_NOFOLLOW))))
            {
                fprintf(stderr, "filename: %s\n",
                        (files + ((blocks + block_id)->start_f) + file_i)->name);
                GKOLOG(WARNING, "filename: %s",
                        (files + ((blocks + block_id)->start_f) + file_i)->name);
            }
            offset = 0;

        }
    }
    (blocks + block_id)->digest = tmp_hash;
    ///    printf("buf: %d\n", sizeof(buf));
    ///    memset(buf, 0, sizeof(buf));
    ///    printf("buf: %d\n", sizeof(buf));
    close(fd);
    return tmp_hash;
}

/**
 * @brief xor hash the file given
 *
 * @see
 * @note
 * @author auxten  <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
unsigned xor_hash_file(unsigned value, FILE * fd, off_t * off, size_t * count,
        u_char * buf)
{
    fseeko(fd, *off, SEEK_SET);
    if (FAIL_CHECK(*count != fread(buf, sizeof(char), *count, fd)))
    {
        GKOLOG(FATAL, "fread error");
    }
    ///fprintf(stderr, "#######################buf: %s\n", buf);
    return xor_hash(buf, *count, value);
}

