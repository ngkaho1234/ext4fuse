#include "bitmap.h"
#include "super.h"
#include "inode_in-memory.h"
#include "logging.h"


ext4_fsblk_t ext4_new_meta_blocks(struct inode *inode,
            ext4_fsblk_t goal,
            unsigned int flags,
            unsigned long *count, int *errp)
{
    int err = ext4_try_to_init_block_bitmap(block_group)
    if (err)
        goto out;
out:
    if (errp)
        *errp = err;
}
