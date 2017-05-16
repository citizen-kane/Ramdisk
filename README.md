RAMDISK Filesystem
------------------

An in-memory file system that works in user space and uses FUSE.

System calls supported:

	open, close
	read, write
	creat [sic], mkdir
	unlink, rmdir
	opendir, readdir
	

File System does not support:
	
	Access control
	Links
	Symbolic links

