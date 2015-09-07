/*
 * Copyright (c) 2010, Gerard Lledó Vives, gerard.lledo@gmail.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 */


#define _XOPEN_SOURCE 500
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>

#include "buffer.h"
#include "logging.h"

#ifdef __FreeBSD__
#include <string.h>
#endif


static int disk_fd = -1;


static int pread_buffered(void *p, size_t size, off_t where)
{
    /* FreeBSD needs to read aligned whole blocks.
     * TODO: Check what is a safe block size.
     */
    off_t first_offset = where % PREAD_BLOCK_SIZE;
    struct buffer_head *bh;
    int ret = 0, bread_ret;

    if (first_offset) {
        /* This is the case if the read doesn't start on a block boundary.
         * We still need to read the whole block and we do, but we only copy to
         * the out pointer the bytes that where actually asked for.  In this
         * case first_offset is the offset into the block. */
        bh = fs_bread((where - first_offset) / PREAD_BLOCK_SIZE, &bread_ret);
        if (!bh) return bread_ret;

        size_t first_size = MIN(size, (size_t)(PREAD_BLOCK_SIZE - first_offset));
        memcpy(p, bh->b_data + first_offset, first_size);
        p += first_size;
        size -= first_size;
        where += first_size;
        ret += first_size;

        fs_brelse(bh);

        if (!size) return ret;
    }

    ASSERT(where % PREAD_BLOCK_SIZE == 0);

    size_t mid_read_size = (size / PREAD_BLOCK_SIZE) * PREAD_BLOCK_SIZE;
    if (mid_read_size) {
        bh = fs_bread(where / PREAD_BLOCK_SIZE, &bread_ret);
        if (!bh) return bread_ret;

        memcpy(p, bh->b_data, mid_read_size);
        p += mid_read_size;
        size -= mid_read_size;
        where += mid_read_size;
        ret += mid_read_size;

        fs_brelse(bh);

        if (!size) return ret;
    }

    ASSERT(size < PREAD_BLOCK_SIZE);

    bh = fs_bread(where / PREAD_BLOCK_SIZE, &bread_ret);
    if (!bh) return bread_ret;

    memcpy(p, bh->b_data, size);
    fs_brelse(bh);

    return ret + size;
}

static int pwrite_buffered(const void *p, size_t size, off_t where)
{
    /* FreeBSD needs to read aligned whole blocks.
     * TODO: Check what is a safe block size.
     */
    off_t first_offset = where % PREAD_BLOCK_SIZE;
    struct buffer_head *bh;
    int ret = 0, pwrite_ret;

    if (first_offset) {
        /* This is the case if the read doesn't start on a block boundary.
         * We still need to read the whole block and we do, but we only copy to
         * the out pointer the bytes that where actually asked for.  In this
         * case first_offset is the offset into the block. */
        bh = fs_bread((where - first_offset) / PREAD_BLOCK_SIZE, &pwrite_ret);
        if (!bh) return pwrite_ret;

        size_t first_size = MIN(size, (size_t)(PREAD_BLOCK_SIZE - first_offset));
        memcpy(bh->b_data + first_offset, p, first_size);

        p += first_size;
        size -= first_size;
        where += first_size;
        ret += first_size;

        fs_mark_buffer_dirty(bh);
        fs_brelse(bh);

        if (!size) return ret;
    }

    ASSERT(where % PREAD_BLOCK_SIZE == 0);

    size_t mid_write_size = (size / PREAD_BLOCK_SIZE) * PREAD_BLOCK_SIZE;
    if (mid_write_size) {
        bh = fs_bwrite(where / PREAD_BLOCK_SIZE, &pwrite_ret);
        if (!bh) return pwrite_ret;

        memcpy(bh->b_data, p, mid_write_size);
        p += mid_write_size;
        size -= mid_write_size;
        where += mid_write_size;
        ret += mid_write_size;

        fs_mark_buffer_dirty(bh);
        fs_brelse(bh);

        if (!size) return ret;
    }

    ASSERT(size < PREAD_BLOCK_SIZE);

    bh = fs_bread(where / PREAD_BLOCK_SIZE, &pwrite_ret);
    if (!bh) return pwrite_ret;

    memcpy(bh->b_data, p, size);
    fs_mark_buffer_dirty(bh);
    fs_brelse(bh);

    return ret + size;
}

int disk_open(const char *path)
{
    disk_fd = open(path, O_RDWR);
    if (disk_fd < 0) {
        return -errno;
    }

    return 0;
}

int disk_get_fd()
{
    return disk_fd;
}

int __disk_read(off_t where, size_t size, void *p, const char *func, int line)
{
    static pthread_mutex_t read_lock = PTHREAD_MUTEX_INITIALIZER;
    ssize_t pread_ret;

    ASSERT(disk_fd >= 0);

    pthread_mutex_lock(&read_lock);
    DEBUG("Disk Read: 0x%jx +0x%zx [%s:%d]", where, size, func, line);
    pread_ret = pread_buffered(p, size, where);
    pthread_mutex_unlock(&read_lock);
    if (size == 0) WARNING("Read operation with 0 size");

    ASSERT((size_t)pread_ret == size);

    return pread_ret;
}

int __disk_write(off_t where, size_t size, const void *p, const char *func, int line)
{
    static pthread_mutex_t write_lock = PTHREAD_MUTEX_INITIALIZER;
    ssize_t pwrite_ret;

    ASSERT(disk_fd >= 0);

    pthread_mutex_lock(&write_lock);
    DEBUG("Disk Write: 0x%jx +0x%zx [%s:%d]", where, size, func, line);
    pwrite_ret = pwrite_buffered(p, size, where);
    pthread_mutex_unlock(&write_lock);
    if (size == 0) WARNING("Write operation with 0 size");

    ASSERT((size_t)pwrite_ret == size);

    return pwrite_ret;
}

int disk_ctx_create(struct disk_ctx *ctx, off_t where, size_t size, uint32_t len)
{
    ASSERT(ctx);        /* Should be user allocated */
    ASSERT(size);

    ctx->cur = where;
    ctx->size = size * len;
    DEBUG("New disk context: 0x%jx +0x%jx", ctx->cur, ctx->size);

    return 0;
}

int __disk_ctx_read(struct disk_ctx *ctx, size_t size, void *p, const char *func, int line)
{
    int ret = 0;

    ASSERT(ctx->size);
    if (ctx->size == 0) {
        WARNING("Using a context with no bytes left");
        return ret;
    }

    /* Truncate if there are too many bytes requested */
    if (size > ctx->size) {
        size = ctx->size;
    }

    ret = __disk_read(ctx->cur, size, p, func, line);
    ctx->size -= ret;
    ctx->cur += ret;

    return ret;
}

int __disk_ctx_write(struct disk_ctx *ctx, size_t size, const void *p, const char *func, int line)
{
    int ret = 0;

    ASSERT(ctx->size);
    if (ctx->size == 0) {
        WARNING("Using a context with no bytes left");
        return ret;
    }

    /* Truncate if there are too many bytes requested */
    if (size > ctx->size) {
        size = ctx->size;
    }

    ret = __disk_write(ctx->cur, size, p, func, line);
    ctx->size -= ret;
    ctx->cur += ret;

    return ret;
}
