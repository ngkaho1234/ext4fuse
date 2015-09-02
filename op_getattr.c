/*
 * Copyright (c) 2010, Gerard Lledó Vives, gerard.lledo@gmail.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 */


#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>

#include "inode.h"
#include "logging.h"

int op_getattr(const char *path, struct stat *stbuf)
{
    struct ext4_inode raw_inode;
    struct inode *inode;
    int ret = 0;
    uint64_t size;

    DEBUG("getattr(%s)", path);

    memset(stbuf, 0, sizeof(struct stat));
    ret = inode_get_by_path(path, &raw_inode);

    if (ret < 0) {
        return ret;
    }

    inode = inode_get(0, &raw_inode);
    if (!inode)
        return -ENOMEM;

    size = inode_get_size(inode);
    inode_put(inode);


    DEBUG("getattr done");

    stbuf->st_mode = raw_inode.i_mode;
    stbuf->st_nlink = raw_inode.i_links_count;
    stbuf->st_size = size;
    stbuf->st_blocks = raw_inode.i_blocks_lo;
    stbuf->st_uid = raw_inode.i_uid;
    stbuf->st_gid = raw_inode.i_gid;
    stbuf->st_atime = raw_inode.i_atime;
    stbuf->st_mtime = raw_inode.i_mtime;
    stbuf->st_ctime = raw_inode.i_ctime;

    return 0;
}
