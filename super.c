/*
 * Copyright (c) 2010, Gerard Lledó Vives, gerard.lledo@gmail.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 */


#include "types/ext4_super.h"

#include "logging.h"
#include "bitmap.h"
#include "super.h"
#include "inode.h"
#include "buffer.h"

static struct ext4_super_block super_block;
static int super_block_dirty = 0;

static struct group_desc_info {
    struct ext4_group_desc gdesc;
    int dirty:1;
} *gdesc_table;

# define EXT4_DESC_PER_BLOCK		(super_block_size() / super_group_desc_size())


ext4_fsblk_t ext4_blocks_count(void)
{
    return ((ext4_fsblk_t)le32_to_cpu(super_block.s_blocks_count_hi) << 32) |
           le32_to_cpu(super_block.s_blocks_count_lo);
}

ext4_fsblk_t ext4_r_blocks_count(void)
{
    return ((ext4_fsblk_t)le32_to_cpu(super_block.s_r_blocks_count_hi) << 32) |
           le32_to_cpu(super_block.s_r_blocks_count_lo);
}

ext4_fsblk_t ext4_free_blocks_count(void)
{
    return ((ext4_fsblk_t)le32_to_cpu(super_block.s_free_blocks_count_hi) << 32) |
           le32_to_cpu(super_block.s_free_blocks_count_lo);
}

void ext4_blocks_count_set(ext4_fsblk_t blk)
{
    super_block.s_blocks_count_lo = cpu_to_le32((__u32)blk);
    super_block.s_blocks_count_hi = cpu_to_le32(blk >> 32);
    super_block_dirty = 1;
}

void ext4_free_blocks_count_set(ext4_fsblk_t blk)
{
    super_block.s_free_blocks_count_lo = cpu_to_le32((__u32)blk);
    super_block.s_free_blocks_count_hi = cpu_to_le32(blk >> 32);
    super_block_dirty = 1;
}

void ext4_r_blocks_count_set(ext4_fsblk_t blk)
{
    super_block.s_r_blocks_count_lo = cpu_to_le32((__u32)blk);
    super_block.s_r_blocks_count_hi = cpu_to_le32(blk >> 32);
    super_block_dirty = 1;
}

uint64_t super_blocks_per_group(void)
{
    return super_block.s_blocks_per_group;
}

ext4_fsblk_t super_first_data_block(void)
{
    return le32_to_cpu(super_block.s_first_data_block);
}

/* calculate the first block number of the group */
static inline ext4_fsblk_t
ext4_group_first_block_no(ext4_group_t group_no)
{
    return group_no * (ext4_fsblk_t)super_blocks_per_group() +
           le32_to_cpu(super_block.s_first_data_block);
}

static uint64_t super_block_group_size(void)
{
    return BLOCKS2BYTES(super_block.s_blocks_per_group);
}

ext4_group_t super_n_block_groups(void)
{
    return (ext4_blocks_count() + super_blocks_per_group() - 1) /
	    super_blocks_per_group();
}

static uint32_t super_group_desc_size(void)
{
    if (!super_block.s_desc_size) return EXT4_MIN_DESC_SIZE;
    else return sizeof(struct ext4_group_desc);
}

uint32_t super_block_size(void) {
    return ((uint64_t)1) << (super_block.s_log_block_size + 10);
}

int super_block_size_bits(void) {
    return super_block.s_log_block_size + 10;
}

uint32_t super_inodes_per_group(void)
{
    return super_block.s_inodes_per_group;
}

uint32_t super_inode_size(void)
{
    return super_block.s_inode_size;
}

uint32_t super_inodes_per_block(void)
{
    return super_block_size() / super_inode_size();
}

uint32_t super_itb_per_group(void)
{
    return super_inodes_per_group() /
           super_inodes_per_block();
}

uint32_t super_n_gdb(void)
{
    return (super_n_block_groups() + EXT4_DESC_PER_BLOCK - 1) /
                               EXT4_DESC_PER_BLOCK;
}

ext4_fsblk_t ext4_block_bitmap(ext4_group_t block_group)
{
    struct ext4_group_desc *bg = &gdesc_table[block_group].gdesc;
    return le32_to_cpu(bg->bg_block_bitmap_lo) |
           (super_group_desc_size() >= EXT4_MIN_DESC_SIZE_64BIT ?
            (ext4_fsblk_t)le32_to_cpu(bg->bg_block_bitmap_hi) << 32 : 0);
}

ext4_fsblk_t ext4_inode_bitmap(ext4_group_t block_group)
{
    struct ext4_group_desc *bg = &gdesc_table[block_group].gdesc;
    return le32_to_cpu(bg->bg_inode_bitmap_lo) |
           (super_group_desc_size() >= EXT4_MIN_DESC_SIZE_64BIT ?
            (ext4_fsblk_t)le32_to_cpu(bg->bg_inode_bitmap_hi) << 32 : 0);
}

ext4_fsblk_t ext4_inode_table(ext4_group_t block_group)
{
    struct ext4_group_desc *bg = &gdesc_table[block_group].gdesc;
    return le32_to_cpu(bg->bg_inode_table_lo) |
           (super_group_desc_size() >= EXT4_MIN_DESC_SIZE_64BIT ?
            (ext4_fsblk_t)le32_to_cpu(bg->bg_inode_table_hi) << 32 : 0);
}

__u32 ext4_free_blks_count(ext4_group_t block_group)
{
    struct ext4_group_desc *bg = &gdesc_table[block_group].gdesc;
    return le16_to_cpu(bg->bg_free_blocks_count_lo) |
           (super_group_desc_size() >= EXT4_MIN_DESC_SIZE_64BIT ?
            (__u32)le16_to_cpu(bg->bg_free_blocks_count_hi) << 16 : 0);
}

__u32 ext4_free_inodes_count(ext4_group_t block_group)
{
    struct ext4_group_desc *bg = &gdesc_table[block_group].gdesc;
    return le16_to_cpu(bg->bg_free_inodes_count_lo) |
           (super_group_desc_size() >= EXT4_MIN_DESC_SIZE_64BIT ?
            (__u32)le16_to_cpu(bg->bg_free_inodes_count_hi) << 16 : 0);
}

__u32 ext4_used_dirs_count(ext4_group_t block_group)
{
    struct ext4_group_desc *bg = &gdesc_table[block_group].gdesc;
    return le16_to_cpu(bg->bg_used_dirs_count_lo) |
           (super_group_desc_size() >= EXT4_MIN_DESC_SIZE_64BIT ?
            (__u32)le16_to_cpu(bg->bg_used_dirs_count_hi) << 16 : 0);
}

__u32 ext4_itable_unused_count(ext4_group_t block_group)
{
    struct ext4_group_desc *bg = &gdesc_table[block_group].gdesc;
    return le16_to_cpu(bg->bg_itable_unused_lo) |
           (super_group_desc_size() >= EXT4_MIN_DESC_SIZE_64BIT ?
            (__u32)le16_to_cpu(bg->bg_itable_unused_hi) << 16 : 0);
}

void ext4_block_bitmap_set(ext4_group_t block_group, ext4_fsblk_t blk)
{
    struct ext4_group_desc *bg = &gdesc_table[block_group].gdesc;
    gdesc_table[block_group].dirty = 1;
    bg->bg_block_bitmap_lo = cpu_to_le32((__u32)blk);
    if (super_group_desc_size() >= EXT4_MIN_DESC_SIZE_64BIT)
        bg->bg_block_bitmap_hi = cpu_to_le32(blk >> 32);
}

void ext4_inode_bitmap_set(ext4_group_t block_group, ext4_fsblk_t blk)
{
    struct ext4_group_desc *bg = &gdesc_table[block_group].gdesc;
    gdesc_table[block_group].dirty = 1;
    bg->bg_inode_bitmap_lo = cpu_to_le32((__u32)blk);
    if (super_group_desc_size() >= EXT4_MIN_DESC_SIZE_64BIT)
        bg->bg_inode_bitmap_hi = cpu_to_le32(blk >> 32);
}

void ext4_inode_table_set(ext4_group_t block_group, ext4_fsblk_t blk)
{
    struct ext4_group_desc *bg = &gdesc_table[block_group].gdesc;
    gdesc_table[block_group].dirty = 1;
    bg->bg_inode_table_lo = cpu_to_le32((__u32)blk);
    if (super_group_desc_size() >= EXT4_MIN_DESC_SIZE_64BIT)
        bg->bg_inode_table_hi = cpu_to_le32(blk >> 32);
}

void ext4_free_blks_set(ext4_group_t block_group, __u32 count)
{
    struct ext4_group_desc *bg = &gdesc_table[block_group].gdesc;
    gdesc_table[block_group].dirty = 1;
    bg->bg_free_blocks_count_lo = cpu_to_le16((__u16)count);
    if (super_group_desc_size() >= EXT4_MIN_DESC_SIZE_64BIT)
        bg->bg_free_blocks_count_hi = cpu_to_le16(count >> 16);
}

void ext4_free_inodes_set(ext4_group_t block_group, __u32 count)
{
    struct ext4_group_desc *bg = &gdesc_table[block_group].gdesc;
    gdesc_table[block_group].dirty = 1;
    bg->bg_free_inodes_count_lo = cpu_to_le16((__u16)count);
    if (super_group_desc_size() >= EXT4_MIN_DESC_SIZE_64BIT)
        bg->bg_free_inodes_count_hi = cpu_to_le16(count >> 16);
}

void ext4_used_dirs_set(ext4_group_t block_group, __u32 count)
{
    struct ext4_group_desc *bg = &gdesc_table[block_group].gdesc;
    gdesc_table[block_group].dirty = 1;
    bg->bg_used_dirs_count_lo = cpu_to_le16((__u16)count);
    if (super_group_desc_size() >= EXT4_MIN_DESC_SIZE_64BIT)
        bg->bg_used_dirs_count_hi = cpu_to_le16(count >> 16);
}

void ext4_itable_unused_set(ext4_group_t block_group, __u32 count)
{
    struct ext4_group_desc *bg = &gdesc_table[block_group].gdesc;
    gdesc_table[block_group].dirty = 1;
    bg->bg_itable_unused_lo = cpu_to_le16((__u16)count);
    if (super_group_desc_size() >= EXT4_MIN_DESC_SIZE_64BIT)
        bg->bg_itable_unused_hi = cpu_to_le16(count >> 16);
}

/*
 * Calculate the block group number and offset, given a block number
 */
void ext4_get_group_no_and_offset(ext4_fsblk_t blocknr,
                                  ext4_group_t *blockgrpp, ext4_fsblk_t *offsetp)
{
    ext4_fsblk_t offset;

    blocknr = blocknr - le32_to_cpu(super_block.s_first_data_block);
    offset = blocknr / super_blocks_per_group();
    if (offsetp)
        *offsetp = offset;
    if (blockgrpp)
        *blockgrpp = (ext4_fsblk_t)blocknr;

}

static int ext4_block_in_group(ext4_fsblk_t block,
                               ext4_group_t block_group)
{
    ext4_group_t actual_group;
    ext4_get_group_no_and_offset(block, &actual_group, NULL);
    if (actual_group == block_group)
        return 1;
    return 0;
}

static inline int test_root(ext4_group_t a, ext4_group_t b)
{
    ext4_group_t num = b;

    while (a > num)
        num *= b;
    return num == a;
}

static int ext4_group_sparse(ext4_group_t group)
{
    if (group <= 1)
        return 1;
    if (!(group & 1))
        return 0;
    return (test_root(group, 7) || test_root(group, 5) ||
            test_root(group, 3));
}

/**
 *	ext4_bg_has_super - number of blocks used by the superblock in group
 *	@sb: superblock for filesystem
 *	@group: group number to check
 *
 *	Return the number of blocks used by the superblock (primary or backup)
 *	in this group.  Currently this will be only 0 or 1.
 */
int ext4_bg_has_super(ext4_group_t group)
{
    if (EXT4_HAS_RO_COMPAT_FEATURE(&super_block,
                                   EXT4_FEATURE_RO_COMPAT_SPARSE_SUPER) &&
            !ext4_group_sparse(group))
        return 0;
    return 1;
}

static unsigned long ext4_bg_num_gdb_meta(ext4_group_t group)
{
    unsigned long metagroup = group / EXT4_DESC_PER_BLOCK;
    ext4_group_t first = metagroup * EXT4_DESC_PER_BLOCK;
    ext4_group_t last = first + EXT4_DESC_PER_BLOCK - 1;

    if (group == first || group == first + 1 || group == last)
        return 1;
    return 0;
}

static unsigned long ext4_bg_num_gdb_nometa(ext4_group_t group)
{
    return ext4_bg_has_super(group) ? super_n_gdb() : 0;
}

/**
 *	ext4_bg_num_gdb - number of blocks used by the group table in group
 *	@sb: superblock for filesystem
 *	@group: group number to check
 *
 *	Return the number of blocks used by the group descriptor table
 *	(primary or backup) in this group.  In the future there may be a
 *	different number of descriptor blocks in each group.
 */
unsigned long ext4_bg_num_gdb(ext4_group_t group)
{
    unsigned long first_meta_bg =
        le32_to_cpu(super_block.s_first_meta_bg);
    unsigned long metagroup = group / EXT4_DESC_PER_BLOCK;

    if (!EXT4_HAS_INCOMPAT_FEATURE(&super_block, EXT4_FEATURE_INCOMPAT_META_BG) ||
            metagroup < first_meta_bg)
        return ext4_bg_num_gdb_nometa(group);

    return ext4_bg_num_gdb_meta(group);

}

static int ext4_group_used_meta_blocks(ext4_group_t block_group)
{
    ext4_fsblk_t tmp;
    /* block bitmap, inode bitmap, and inode table blocks */
    int used_blocks = super_itb_per_group() + 2;

    if (EXT4_HAS_INCOMPAT_FEATURE(&super_block, EXT4_FEATURE_INCOMPAT_FLEX_BG)) {
        if (!ext4_block_in_group(ext4_block_bitmap(block_group),
                                 block_group))
            used_blocks--;

        if (!ext4_block_in_group(ext4_inode_bitmap(block_group),
                                 block_group))
            used_blocks--;

        tmp = ext4_inode_table(block_group);
        for (; tmp < ext4_inode_table(block_group) +
                super_itb_per_group(); tmp++) {
            if (!ext4_block_in_group(tmp, block_group))
                used_blocks -= 1;
        }
    }
    return used_blocks;
}

/*
 * To avoid calling the atomic setbit hundreds or thousands of times, we only
 * need to use it within a single byte (to ensure we get endianness right).
 * We can use memset for the rest of the bitmap as there are no other users.
 */
static void mark_bitmap_end(int start_bit, int end_bit, void *bitmap)
{
    int i;

    if (start_bit >= end_bit)
        return;

    for (i = start_bit; (unsigned)i < ((start_bit + 7) & ~7UL); i++)
        ext4_set_bit(i, bitmap);
    if (i < end_bit)
        memset(bitmap + (i >> 3), 0xff, (end_bit - i) >> 3);
}

int ext4_is_block_bitmap_inited(ext4_group_t block_group)
{
    struct group_desc_info *gdesc_info = gdesc_table + block_group;
    return !(gdesc_info->gdesc.bg_flags & cpu_to_le16(EXT4_BG_BLOCK_UNINIT));
}

/* Initializes an uninitialized block bitmap if given, and returns the
 * number of blocks free in the group. */
ext4_fsblk_t ext4_init_block_bitmap(struct buffer_head *bh,
                                ext4_group_t block_group)
{
    int bit, bit_max;
    unsigned free_blocks, group_blocks;
    struct group_desc_info *gdesc_info = gdesc_table + block_group;
    struct ext4_super_block *sb = &super_block;
    if (bh) {
        fs_mark_buffer_dirty(bh);
        memset(bh->b_data, 0, super_block_size());
    }

    /* Check for superblock and gdt backups in this group */
    bit_max = ext4_bg_has_super(block_group);

    if (!EXT4_HAS_INCOMPAT_FEATURE(sb, EXT4_FEATURE_INCOMPAT_META_BG) ||
            block_group < le32_to_cpu(sb->s_first_meta_bg) *
            EXT4_DESC_PER_BLOCK) {
        if (bit_max) {
            bit_max += ext4_bg_num_gdb(block_group);
            bit_max +=
                le16_to_cpu(sb->s_reserved_gdt_blocks);
        }
    } else { /* For META_BG_BLOCK_GROUPS */
        bit_max += ext4_bg_num_gdb(block_group);
    }

    if (block_group == super_n_block_groups() - 1) {
        /*
         * Even though mke2fs always initialize first and last group
         * if some other tool enabled the EXT4_BG_BLOCK_UNINIT we need
         * to make sure we calculate the right free blocks
         */
        group_blocks = (unsigned int)(ext4_blocks_count() -
                                      le32_to_cpu(super_block.s_first_data_block) -
                                      (super_blocks_per_group() * (super_n_block_groups() - 1)));
    } else {
        group_blocks = super_blocks_per_group();
    }

    free_blocks = group_blocks - bit_max;

    if (bh) {
        ext4_fsblk_t start, tmp;
        int flex_bg = 0;

        for (bit = 0; bit < bit_max; bit++)
            ext4_set_bit(bit, (void *)bh->b_data);

        start = ext4_group_first_block_no(block_group);

        if (EXT4_HAS_INCOMPAT_FEATURE(sb,
                                      EXT4_FEATURE_INCOMPAT_FLEX_BG))
            flex_bg = 1;

        /* Set bits for block and inode bitmaps, and inode table */
        tmp = ext4_block_bitmap(block_group);
        if (!flex_bg || ext4_block_in_group(tmp, block_group))
            ext4_set_bit(tmp - start, (void *)bh->b_data);

        tmp = ext4_inode_bitmap(block_group);
        if (!flex_bg || ext4_block_in_group(tmp, block_group))
            ext4_set_bit(tmp - start, (void *)bh->b_data);

        tmp = ext4_inode_table(block_group);
        for (; tmp < ext4_inode_table(block_group) +
                super_itb_per_group(); tmp++) {
            if (!flex_bg ||
                    ext4_block_in_group(tmp, block_group))
                ext4_set_bit(tmp - start, (void *)bh->b_data);
        }
        /*
         * Also if the number of blocks within the group is
         * less than the blocksize * 8 ( which is the size
         * of bitmap ), set rest of the block bitmap to 1
         */
        mark_bitmap_end(group_blocks, super_block_size() * 8, bh->b_data);
    }
    gdesc_info->gdesc.bg_flags &= cpu_to_le16(~EXT4_BG_BLOCK_UNINIT);
    gdesc_info->dirty = 1;
    return free_blocks - ext4_group_used_meta_blocks(block_group);
}

int ext4_try_to_init_block_bitmap(ext4_group_t block_group)
{
    struct group_desc_info *gdesc_info = gdesc_table + block_group;
    if (gdesc_info->gdesc.bg_flags & cpu_to_le16(EXT4_BG_BLOCK_UNINIT)) {
        int ret = 0;
        ext4_fsblk_t bitmap_blk = ext4_block_bitmap(block_group);
        struct buffer_head *bh = fs_bwrite(bitmap_blk, &ret);
        if (!bh)
            return ret;
        ext4_init_block_bitmap(bh, block_group);
        fs_brelse(bh);
    }
    return 0;
}

ext4_fsblk_t ext4_inode_blocks(struct ext4_inode *inode)
{
    ext4_fsblk_t i_blocks;

    if (EXT4_HAS_RO_COMPAT_FEATURE(&super_block,
                                   EXT4_FEATURE_RO_COMPAT_HUGE_FILE)) {
        /* we are using combined 48 bit field */
        i_blocks = ((uint64_t)le16_to_cpu(inode->osd2.linux2.l_i_blocks_high)) << 32 |
                   le32_to_cpu(inode->i_blocks_lo);
        if (inode->i_flags & EXT4_HUGE_FILE_FL) {
            /* i_blocks represent file system block size */
            return i_blocks;
        } else
            return i_blocks >> (super_block_size_bits() - 9);

    } else
        return le32_to_cpu(inode->i_blocks_lo) >> (super_block_size_bits() - 9);

}

void ext4_set_inode_blocks(struct inode *inode, ext4_fsblk_t blocks)
{
    if (EXT4_HAS_RO_COMPAT_FEATURE(&super_block,
                                   EXT4_FEATURE_RO_COMPAT_HUGE_FILE)) {
        /* we are using combined 48 bit field */
        if (cpu_to_le32(inode->raw_inode->i_flags) & EXT4_HUGE_FILE_FL) {
            /* i_blocks represent file system block size */
            inode->raw_inode->i_blocks_lo = cpu_to_le32((uint32_t)blocks);
            inode->raw_inode->osd2.linux2.l_i_blocks_high = cpu_to_le16(blocks >> 32);
        } else {
            blocks <<= (super_block_size_bits() - 9);
            inode->raw_inode->i_blocks_lo = cpu_to_le32((uint32_t)blocks);
            inode->raw_inode->osd2.linux2.l_i_blocks_high = cpu_to_le16(blocks >> 32);
        }
    } else {
        blocks <<= (super_block_size_bits() - 9);
        inode->raw_inode->i_blocks_lo = cpu_to_le32((uint32_t)blocks);
    }
    inode_mark_dirty(inode);
}

int super_fill(void)
{
    disk_read(BOOT_SECTOR_SIZE, sizeof(struct ext4_super_block), &super_block);

    INFO("BLOCK SIZE: %i", super_block_size());
    INFO("BLOCK GROUP SIZE: %i", super_block_group_size());
    INFO("N BLOCK GROUPS: %i", super_n_block_groups());
    INFO("INODE SIZE: %i", super_inode_size());
    INFO("INODES PER GROUP: %i", super_inodes_per_group());

    return fs_cache_init();
}

int super_writeback(void)
{
    if (super_block_dirty)
        disk_write(BOOT_SECTOR_SIZE, sizeof(struct ext4_super_block), &super_block);
    super_block_dirty = 0;
    return 0;
}

void super_uninit(void)
{
    super_writeback();
}

/* FIXME: Handle bg_inode_table_hi when size > EXT4_MIN_DESC_SIZE */
off_t super_group_inode_table_offset(uint32_t inode_num)
{
    uint32_t n_group = inode_num / super_inodes_per_group();
    ASSERT(n_group < super_n_block_groups());
    DEBUG("Inode table offset: 0x%llp", ext4_inode_table(n_group));
    return BLOCKS2BYTES(ext4_inode_table(n_group));
}

ext4_fsblk_t descriptor_loc(ext4_fsblk_t logical_sb_block, unsigned int nr)
{
    ext4_group_t bg, first_meta_bg;
    int has_super = 0;

    first_meta_bg = le32_to_cpu(super_block.s_first_meta_bg);

    if (!EXT4_HAS_INCOMPAT_FEATURE(&super_block, EXT4_FEATURE_INCOMPAT_META_BG) ||
            nr < first_meta_bg)
        return logical_sb_block + nr + 1;
    bg = EXT4_DESC_PER_BLOCK * nr;
    if (ext4_bg_has_super(bg))
        has_super = 1;
    return (has_super + ext4_group_first_block_no(bg));
}

/* struct ext4_group_desc might be bigger than on disk structure, if we are not
 * using big ones.  That info is in the superblock.  Be careful when allocating
 * or manipulating this pointers. */
int super_group_fill(void)
{
    ext4_fsblk_t sb_block = 1;
    gdesc_table = malloc(sizeof(struct group_desc_info) * super_n_block_groups());
    memset(gdesc_table, 0, sizeof(struct group_desc_info) * super_n_block_groups());

    if (super_block_size() != EXT4_MIN_BLOCK_SIZE)
        sb_block = EXT4_MIN_BLOCK_SIZE / super_block_size();

    for (ext4_group_t i = 0; i < super_n_block_groups(); i++) {
        off_t bg_off = descriptor_loc(sb_block, i / EXT4_DESC_PER_BLOCK)
                         << super_block_size_bits();
        bg_off += super_group_desc_size() * (i & (EXT4_DESC_PER_BLOCK - 1));

        /* disk advances super_group_desc_size(), pointer sizeof(struct...).
         * These values might be different!!! */
        disk_read(bg_off, super_group_desc_size(), &gdesc_table[i].gdesc);
    }

    return 0;
}

int super_group_writeback(void)
{
    ext4_fsblk_t sb_block = 1;

    if (super_block_size() != EXT4_MIN_BLOCK_SIZE)
        sb_block = EXT4_MIN_BLOCK_SIZE / super_block_size();

    for (ext4_group_t i = 0; i < super_n_block_groups(); i++) {
        off_t bg_off = descriptor_loc(sb_block, i / EXT4_DESC_PER_BLOCK)
                         << super_block_size_bits();
        bg_off += super_group_desc_size() * (i & (EXT4_DESC_PER_BLOCK - 1));

        /* disk advances super_group_desc_size(), pointer sizeof(struct...).
         * These values might be different!!! */
        if (gdesc_table[i].dirty) {
                disk_write(bg_off, super_group_desc_size(), &gdesc_table[i].gdesc);
                gdesc_table[i].dirty = 0;
        }
    }

    return 0;
}

void super_group_uninit(void)
{
    super_group_writeback();
    free(gdesc_table);
    gdesc_table = NULL;
}
