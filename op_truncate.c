#include <errno.h>

#include "common.h"
#include "super.h"
#include "inode.h"
#include "logging.h"
#include "ops.h"

int op_truncate (const char *path, off_t length)
{
    int ret;
    uint32_t ino;
    struct ext4_inode raw_inode;
    ext4_lblk_t from;

    ino = inode_get_idx_by_path(path);
    if (ino <= 0) {
        return -ENOENT;
    }

    int inode_get_ret = inode_get_by_number(ino, &raw_inode);
    if (inode_get_ret < 0) {
        return inode_get_ret;
    }

    struct inode *inode = inode_get(ino, &raw_inode);
    ASSERT(inode);
    from = (length + super_block_size() - 1) / super_block_size();
    ret = inode_remove_data_pblock(inode, from);
    inode_set_size(inode, length);
    inode_put(inode);

    return ret;
}

int op_ftruncate (const char *path, off_t length, struct fuse_file_info *fi)
{
    int ret;
    uint32_t ino;
    struct ext4_inode raw_inode;
    ext4_lblk_t from;

    int inode_get_ret = inode_get_by_number(ino = fi->fh, &raw_inode);
    if (inode_get_ret < 0) {
        return inode_get_ret;
    }

    struct inode *inode = inode_get(ino, &raw_inode);
    ASSERT(inode);
    from = (length + super_block_size() - 1) / super_block_size();
    ret = inode_remove_data_pblock(inode, from);
    inode_set_size(inode, length);
    inode_put(inode);

    return ret;
}
