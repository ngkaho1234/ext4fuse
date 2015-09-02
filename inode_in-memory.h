#ifndef INODE_IN_MEMORY_H
#define INODE_IN_MEMORY_H

struct inode {
	int i_data_dirty;
	uint32_t i_ino;
	uint32_t *i_data;
	struct ext4_inode *raw_inode;
};

struct inode *inode_get(uint32_t ino, struct ext4_inode *raw_inode);
static inline void inode_mark_dirty(struct inode *inode)
{
	inode->i_data_dirty = 1;
}

void inode_put(struct inode *inode);

static inline void inode_set_size(struct inode *inode, uint64_t size)
{
    inode->raw_inode->i_size_lo = (uint32_t)size;
    inode->raw_inode->i_size_high = (uint32_t)(size >> 32);
    inode_mark_dirty(inode);
}

#endif
