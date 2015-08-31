#ifndef SUPER_H
#define SUPER_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "types/ext4_basic.h"
#include "common.h"

#define BLOCK_SIZE                              (super_block_size())
#define BLOCKS2BYTES(__blks)                    (((uint64_t)(__blks)) * BLOCK_SIZE)
#define BYTES2BLOCKS(__bytes)                   ((__bytes) / BLOCK_SIZE + ((__bytes) % BLOCK_SIZE ? 1 : 0))

#define MALLOC_BLOCKS(__blks)                   (malloc(BLOCKS2BYTES(__blks)))
#define ALIGN_TO_BLOCKSIZE(__n)                 (ALIGN_TO(__n, BLOCK_SIZE))

#define BOOT_SECTOR_SIZE            0x400


/* struct ext4_super */
uint32_t super_block_size(void);
int super_block_size_bits(void);
uint32_t super_inodes_per_group(void);
uint32_t super_inode_size(void);
int super_fill(void);
int super_writeback(void);
void super_uninit(void);

/* struct ext4_group_desc */
off_t super_group_inode_table_offset(uint32_t inode_num);
int super_group_fill(void);
int super_group_writeback(void);
void super_group_uninit(void);

ext4_fsblk_t ext4_block_bitmap(ext4_group_t block_group);
ext4_fsblk_t ext4_inode_bitmap(ext4_group_t block_group);
ext4_fsblk_t ext4_inode_table(ext4_group_t block_group);
__u32 ext4_free_blks_count(ext4_group_t block_group);
__u32 ext4_free_inodes_count(ext4_group_t block_group);
__u32 ext4_used_dirs_count(ext4_group_t block_group);
__u32 ext4_itable_unused_count(ext4_group_t block_group);
void ext4_block_bitmap_set(ext4_group_t block_group, ext4_fsblk_t blk);
void ext4_inode_bitmap_set(ext4_group_t block_group, ext4_fsblk_t blk);
void ext4_inode_table_set(ext4_group_t block_group, ext4_fsblk_t blk);
void ext4_free_blks_set(ext4_group_t block_group, __u32 count);
void ext4_free_inodes_set(ext4_group_t block_group, __u32 count);
void ext4_used_dirs_set(ext4_group_t block_group, __u32 count);
void ext4_itable_unused_set(ext4_group_t block_group, __u32 count);

int ext4_try_to_init_block_bitmap(ext4_group_t block_group);

#endif
