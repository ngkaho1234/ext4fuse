
int op_ftruncate (const char *path, off_t length, struct fuse_file_info *fi)
{
	(void) fi;
	return op_truncate(path, length);
}
