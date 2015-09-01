#include "bitmap.h"
#include "super.h"
#include "alloc.h"
#include "buffer.h"
#include "logging.h"


ext4_fsblk_t ext4_new_meta_blocks(struct inode *inode,
            ext4_fsblk_t goal,
            unsigned int flags,
            unsigned long *count, int *errp)
{
    int err = 0;
    int bitmap_len, index = 0, len = 0, wanted;
    ext4_fsblk_t group_goal = 0, bitmap_blk = 0, ret;
    ext4_group_t block_group;
    struct buffer_head *bh = NULL;

    if (count)
        wanted = *count;
    else
        wanted = 1;

    if (goal)
        group_goal = (goal - super_first_data_block()) / super_blocks_per_group();
    block_group = group_goal;

again:
    bitmap_blk = ext4_block_bitmap(block_group);

    /*group_desc->bg_checksum = ext4_group_desc_csum(EXT3_SB(sb), Group, group_desc);*/
    err = ext4_try_to_init_block_bitmap(block_group);
    if (err)
        goto out;
    if (bh) {
        fs_brelse(bh);
        bh = NULL;
    }
    bh = fs_bread(bitmap_blk, &err);
    if (err)
        goto out;

    if (ext4_free_blks_count(block_group)) {
        if (block_group == super_n_block_groups() - 1) {
            bitmap_len = ext4_blocks_count() % super_blocks_per_group();

            /* s_blocks_count is integer multiple of s_blocks_per_group */
            if (bitmap_len == 0) {
                bitmap_len = super_blocks_per_group();
            }
        } else {
            bitmap_len = super_blocks_per_group();
        }

        /* try to find a clear bit range */
        index = mb_find_next_zero_bit(bh->b_data, bitmap_len, 0);
        len = mb_find_zero_run_len(bh->b_data, bitmap_len, index);

        if (len < 0) {
            WARNING("Possible bugs... at alloc.c:%d\n", __LINE__);
        }

        /* We could not get new block in the prefered group */
        if (!len) {
                /* no blocks found: set bg_free_blocks_count to 0 */
                ext4_free_blks_set(block_group, 0);

                /* will try next group */
                goto no_space;
        } else {
            /* we got free blocks */
            if (len > wanted)
                len = wanted;
            mb_set_bits(bh->b_data, index, len);
            fs_mark_buffer_dirty(bh);
            ext4_free_blks_set(block_group, ext4_free_blks_count(block_group) - len);
            ext4_free_blocks_count_set(ext4_free_blocks_count() - len);
            ext4_set_inode_blocks(inode, ext4_inode_blocks(inode->raw_inode) + len);
        }
    } else {
no_space:
        /* try next group */
        block_group = (block_group + 1) % super_n_block_groups();
        if (block_group != group_goal)
            goto again;

        err = -ENOSPC;
    }
out:
    if (bh) {
        fs_brelse(bh);
        bh = NULL;
    }
    if (errp)
        *errp = err;
    if (count)
        *count = len;

    ret = (super_first_data_block() +
            block_group * super_blocks_per_group() + index);

    DEBUG("err: %d, len: %d, ret_block: %llu", err, len, ret);
    DEBUG("bitmap_blk: %llu, first_data_block: %llu, block_group: %lu, index: %d",
                bitmap_blk, super_first_data_block(), block_group, index);

    return (err) ? 0 : ret;
}

/*
 * FIXME: error detection is required.
 */
void ext4_ext_free_blocks(struct inode *inode,
                ext4_fsblk_t block, int count, int flags)
{
    int err = 0;
    int index;
    ext4_fsblk_t bitmap_blk = 0;
    ext4_group_t block_group;
    struct buffer_head *bh = NULL;

    block_group = (block - super_first_data_block()) / super_blocks_per_group();

    if (!ext4_is_block_bitmap_inited(block_group))
        goto out;

    /*group_desc->bg_checksum = ext4_group_desc_csum(EXT3_SB(sb), Group, group_desc);*/
    bitmap_blk = ext4_block_bitmap(block_group);
    bh = fs_bread(bitmap_blk, &err);
    if (err)
        goto out;

    index = block - super_first_data_block() -
            block_group * super_blocks_per_group();
    mb_clear_bits(bh->b_data, index, count);
    fs_mark_buffer_dirty(bh);
    ext4_free_blks_set(block_group, ext4_free_blks_count(block_group) + count);
    ext4_free_blocks_count_set(ext4_free_blocks_count() + count);
    ext4_set_inode_blocks(inode, ext4_inode_blocks(inode->raw_inode) - count);
out:
    DEBUG("bitmap_blk: %llu, first_data_block: %llu, block_group: %lu, index: %d, blockno: %llu, count: %d",
                bitmap_blk, super_first_data_block(), block_group, index, block, count);
    if (bh) {
        fs_brelse(bh);
        bh = NULL;
    }
}
