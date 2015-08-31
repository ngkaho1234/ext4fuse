/*
 * Copyright (c) 2010, Gerard Lledó Vives, gerard.lledo@gmail.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 */


#include <string.h>
#include <sys/types.h>
#include <errno.h>
#include <inttypes.h>

#include "common.h"
#include "disk.h"
#include "super.h"
#include "inode_in-memory.h"
#include "logging.h"
#include "ops.h"

/* This function write all necessary data until the offset is aligned */
static size_t first_write(uint32_t ino, struct ext4_inode *inode, const char *buf, size_t size, off_t offset)
{
    /* Reason for the -1 is that offset = 0 and size = BLOCK_SIZE is all on the
     * same block.  Meaning that byte at offset + size is not actually read. */
    uint32_t end_lblock = (offset + (size - 1)) / BLOCK_SIZE;
    uint32_t start_lblock = offset / BLOCK_SIZE;
    uint32_t start_block_off = offset % BLOCK_SIZE;

    /* If the size is zero, or we are already aligned, skip over this */
    if (size == 0) return 0;
    if (start_block_off == 0) return 0;

    uint64_t start_pblock = inode_get_data_pblock(inode, start_lblock, NULL, ino);

    /* Check if all the read request lays on the same block */
    if (start_lblock == end_lblock) {
        disk_write(BLOCKS2BYTES(start_pblock) + start_block_off, size, buf);
        return size;
    } else {
        size_t size_to_block_end = ALIGN_TO_BLOCKSIZE(offset) - offset;
        ASSERT((offset + size_to_block_end) % BLOCK_SIZE == 0);

        disk_write(BLOCKS2BYTES(start_pblock) + start_block_off, size_to_block_end, buf);
        return size_to_block_end;
    }
}

int op_write(const char *path, const char *buf, size_t size, off_t offset,
            struct fuse_file_info *fi)
{
    struct ext4_inode raw_inode;
    size_t ret = 0;
    uint32_t extent_len, ino;

    /* Not sure if this is possible at all... */
    ASSERT(offset >= 0);

    DEBUG("write(%s, buf, %zd, %llu, fi->fh=%d)", path, size, offset, fi->fh);
    int inode_get_ret = inode_get_by_number(ino = fi->fh, &raw_inode);

    if (inode_get_ret < 0) {
        return inode_get_ret;
    }

    ret = first_write(ino, &raw_inode, buf, size, offset);

    buf += ret;
    offset += ret;

    for (ext4_lblk_t lblock = offset / BLOCK_SIZE; size > ret; lblock += extent_len) {
        uint64_t pblock;
        extent_len = (size - ret + super_block_size() - 1) / super_block_size();
        pblock = inode_get_data_pblock(&raw_inode, lblock, &extent_len, ino);
        size_t bytes;

        if (pblock) {
            struct disk_ctx write_ctx;

            disk_ctx_create(&write_ctx, BLOCKS2BYTES(pblock), BLOCK_SIZE, extent_len);
            bytes = disk_ctx_write(&write_ctx, size - ret, buf);
        } else {
            DEBUG("Allocating block failed.");
            break;
        }
        ret += bytes;
        buf += bytes;
        DEBUG("Write %zd/%zd bytes from %d consecutive blocks", ret, size, extent_len);
    }

    struct inode *inode = inode_get(ino, &raw_inode);
    if (inode) {
        inode_set_size(inode, inode_get_size(&raw_inode) + ret);
        inode_put(inode);
    }

    /* We always read as many bytes as requested (after initial truncation) */
    ASSERT(size == ret);
    return ret;
}
