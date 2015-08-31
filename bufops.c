#include <stdio.h>
#include <sys/mman.h>
#include <assert.h>

#include "super.h"
#include "buffer.h"

static int fs_bh_alloc = 0;
static int fs_bh_freed = 0;

static struct block_device *block_device = NULL;

int fs_cache_init(void)
{
	block_device = bdev_alloc(disk_get_fd(), super_block_size_bits());
	if (block_device)
		return 0;

	return -1;
}

void fs_cache_cleanup(void)
{
	if (block_device)
		bdev_free(block_device);
}

struct buffer_head *fs_bread(ext4_fsblk_t block, int *ret)
{
	int err = 0;
	struct buffer_head *bh;
	assert(block_device);
	bh = sb_getblk(block_device->bd_super, block);
	err = bh_submit_read(bh);
out:
	if (ret)
		*ret = err;
	if (bh)
		fs_bh_alloc++;

	return bh;
}

struct buffer_head *fs_bwrite(ext4_fsblk_t block, int *ret)
{
	int err = 0;
	struct buffer_head *bh;
	assert(block_device);
	bh = sb_getblk(block_device->bd_super, block);
out:
	if (ret)
		*ret = err;
	if (bh)
		fs_bh_alloc++;

	return bh;
}

void fs_brelse(struct buffer_head *bh)
{
	fs_bh_freed++;
	brelse(bh);
}

void fs_mark_buffer_dirty(struct buffer_head *bh)
{
	set_buffer_uptodate(bh);
	set_buffer_dirty(bh);
}

void fs_bforget(struct buffer_head *bh)
{
	clear_buffer_uptodate(bh);
	clear_buffer_dirty(bh);
	fs_brelse(bh);
}

void fs_bh_showstat(void)
{
	printf("fs_bh_alloc: %d, fs_bh_freed: %d\n", fs_bh_alloc,
		fs_bh_freed);
}
