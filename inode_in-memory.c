#include <stdlib.h>

#include "inode.h"

struct inode *inode_get(uint32_t ino, struct ext4_inode *raw_inode)
{
	struct inode *inode = malloc(sizeof(struct inode));
	if (!inode || !raw_inode)
		return NULL;
	inode->i_ino = ino;
	inode->i_data_dirty = 0;
	inode->raw_inode = raw_inode;
	inode->i_data = raw_inode->i_block;
	return inode;
}

void inode_put(struct inode *inode)
{
	if (inode->i_data_dirty) {
		if (inode->i_ino)
			inode_set_by_number(inode->i_ino, inode->raw_inode);

		inode->i_data_dirty = 0;
	}
	free(inode);
}
