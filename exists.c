int exists(char *filename) {
	struct stat statbuf;

// Description of stat field /*{{{*/
/*
           struct stat {
               dev_t     st_dev;     / ID of device containing file /
               ino_t     st_ino;     / inode number /
               mode_t    st_mode;    / protection /
               nlink_t   st_nlink;   / number of hard links /
               uid_t     st_uid;     / user ID of owner /
               gid_t     st_gid;     / group ID of owner /
               dev_t     st_rdev;    / device ID (if special file) /
               off_t     st_size;    / total size, in bytes /
               blksize_t st_blksize; / blocksize for file system I/O /
               blkcnt_t  st_blocks;  / number of blocks allocated /
               time_t    st_atime;   / time of last access /
               time_t    st_mtime;   / time of last modification /
               time_t    st_ctime;   / time of last status change /
           };

 The following POSIX macros are defined to check the file type using the st_mode field:

           S_ISREG(m)  is it a regular file?

           S_ISDIR(m)  directory?

           S_ISCHR(m)  character device?

           S_ISBLK(m)  block device?

           S_ISFIFO(m) FIFO (named pipe)?

           S_ISLNK(m)  symbolic link? (Not in POSIX.1-1996.)

           S_ISSOCK(m) socket? (Not in POSIX.1-1996.)

       The following flags are defined for the st_mode field:

           S_IFMT     0170000   bit mask for the file type bit fields
           S_IFSOCK   0140000   socket
           S_IFLNK    0120000   symbolic link
           S_IFREG    0100000   regular file
           S_IFBLK    0060000   block device
           S_IFDIR    0040000   directory
           S_IFCHR    0020000   character device
           S_IFIFO    0010000   FIFO
           S_ISUID    0004000   set UID bit
           S_ISGID    0002000   set-group-ID bit (see below)
           S_ISVTX    0001000   sticky bit (see below)
           S_IRWXU    00700     mask for file owner permissions
           S_IRUSR    00400     owner has read permission
           S_IWUSR    00200     owner has write permission
           S_IXUSR    00100     owner has execute permission
           S_IRWXG    00070     mask for group permissions
           S_IRGRP    00040     group has read permission
           S_IWGRP    00020     group has write permission
           S_IXGRP    00010     group has execute permission
           S_IRWXO    00007     mask for permissions for others (not in group)
           S_IROTH    00004     others have read permission
           S_IWOTH    00002     others have write permission
           S_IXOTH    00001     others have execute permission
*//*}}}*/

	int r = stat(filename, &statbuf);
	return (r == 0) ? stat.st_mode : 0;
}
