#ifndef ALLOC_H
#define ALLOC_H

#include "types/ext4_basic.h"
#include "inode.h"

ext4_fsblk_t ext4_new_meta_blocks(struct inode *inode,
            ext4_fsblk_t goal,
            unsigned int flags,
            unsigned long *count, int *errp);
void ext4_ext_free_blocks(struct inode *inode,
                ext4_fsblk_t block, int count, int flags);

#endif
