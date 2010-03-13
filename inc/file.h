#if LAB >= 4
// File storage and I/O definitions.
// In PIOS, file I/O is implemented mostly in user space,
// with files kept within each process's own address space
// in the virtual address range 2GB-3GB (0x80000000-0xc0000000).
// Each file's contents occupies a 4MB area of this address region
// (regardless of the file's actual size), for up to 256 files total.
// The first 4MB region, for "file 0", is reserved for a process state area.
//
// Output streams such as the display are represented by append-mode files,
// for which data appended by child processes gets merged together
// into a composite output stream when the child synchronizes with its parent.
// Input streams such as the console are represented by "partial files"
// provided by the parent process (or ultimately the kernel):
// if a child reads to the current end of a partial file,
// it returns to its parent (or the kernel) to wait for more input.
#ifndef PIOS_INC_FILE_H
#define PIOS_INC_FILE_H 1

#include <inc/types.h>


// These definitions should really be in limits.h according to POSIX
#define OPEN_MAX	256	// Max number of open files per process
#define NAME_MAX	63	// Max length of a filename, not inluding null
#define PATH_MAX	1024	// Max length of a full pathname, incl. null

// File open flags - these belong in fcntl.h according to POSIX
#define	O_RDONLY	0x0001		// open for reading only
#define	O_WRONLY	0x0002		// open for writing only
#define	O_RDWR		0x0003		// open for reading and writing
#define	O_ACCMODE	0x0003		// mask for above modes

#define O_APPEND	0x0010		// writes always append to file
#define	O_CREAT		0x0020		// create if nonexistent
#define	O_TRUNC		0x0040		// truncate to zero length
#define	O_EXCL		0x0080		// error if already exists
#if LAB >= 99
#define O_MKDIR		0x0100		// create directory, not regular file
#endif

#define FILE_INODES	256		// Max number of files or "inodes"
#define	FILE_MAXSIZE	(1<<22)		// Max size of a single file - 4MB

#define FILESVA	0x80000000		// Virtual address of file state area
#define FILEDATA(ino)	((void*)FILESVA + ((ino) << 22)) // File data area

struct stat;

struct dirent {				// Directory entry - should be 64 bytes
#if LAB >= 99
	int	d_ino;			// File inode number, 0 if free dirent
#endif
	char	d_name[NAME_MAX+1];	// Entry name
};

typedef struct filedesc {		// Per-open file descriptor state
	ino_t	ino;			// Opened file's inode number
	int	flags;			// File open flags - O_*, above
	off_t	ofs;			// Current seek pointer in file or dir
	int	err;			// Last error on this file descriptor
} filedesc;

typedef struct fileinode {		// Per-file state - like an "inode"
	int	dino;			// Directory this entry lives in
#if LAB >= 99
	int	refs;			// Reference count (FDs, children)
#endif
	struct dirent de;		// Entry name, "" if free entry
	int	ver;			// Version - bumped on every change
	mode_t	mode;			// File mode (stat.h), 0 if deleted
	size_t	size;			// Current size if regular file
} fileinode;


// Information record we keep for child processes forked via Unix fork(),
// for the use of wait() and waitpid().
typedef struct procinfo {
	int	state;			// Current state of this child process
	int	rver[FILE_INODES];	// Last reconcile ver, by parent inode
	int	rlen[FILE_INODES];	// Last reconcile len, by parent inode
	int	ip2c[FILE_INODES];	// Parent to child inode map
	int	ic2p[FILE_INODES];	// Child to parent inode map
} procinfo;

// Values for procinfo.state
#define PROC_FREE	0		// Unused child, available for fork()
#define PROC_RESERVED	(-1)		// Child reserved for special purpose
#define PROC_FORKED	1		// This child forked and running


// User-space Unix process state.
typedef struct filestate {
	int		err;		// This process/thread's errno variable
	int		cwd;		// Ref to inode for current directory
	bool		exited;		// Set to true when this process exits
	int		status;		// Exit status - set on exit
	filedesc	fd[OPEN_MAX];	// File descriptor table
	fileinode	fi[FILE_INODES]; // "Inodes" describing actual files
	procinfo	child[256]; 	// Unix state of all child processes
} filestate;

#define files		((filestate *) FILESVA)

// Special file inode numbers
#define FILEINO_NULL	0		// Inode 0 is never used
#define FILEINO_CONSIN	1		// Inode 1 holds console input
#define FILEINO_CONSOUT	2		// Inode 2 holds console output
#define FILEINO_ROOTDIR	3		// Inode 3 is the root dir
#define FILEINO_GENERAL	4		// First general-purpose inode

#if LAB >= 99
// File inode flags
#define FILEIF_PARTIAL	0x01	// Wait when we read to the end
#define FILEIF_RANDWR	0x02		// Has been randomly written to
#define FILEIF_CONFLICT	0x04	// Write/write conflict detected
#endif


#define filedesc_isvalid(f) \
	((f) >= &files->fd[0] && (f) < &files->fd[OPEN_MAX])
#define filedesc_isopen(f) \
	(filedesc_isvalid(f) && (f)->ino != FILEINO_NULL)
#define filedesc_isreadable(f) \
	(filedesc_isopen(f) && (f)->flags & O_RDONLY)
#define filedesc_iswritable(f) \
	(filedesc_isopen(f) && (f)->flags & O_WRONLY)

#define fileino_isvalid(ino) \
	((ino) > 0 && (ino) < FILE_INODES)
#define fileino_alloced(ino) \
	(fileino_isvalid(ino) && files->fi[ino].de.d_name[0] != 0)
#define fileino_exists(ino) \
	(fileino_alloced(ino) && files->fi[ino].mode != 0)
#define fileino_isreg(ino)	\
	(fileino_alloced(ino) && S_ISREG(files->fi[ino].mode))
#define fileino_isdir(ino)	\
	(fileino_alloced(ino) && S_ISDIR(files->fi[ino].mode))


int fileino_alloc(void);
ssize_t fileino_write(int ino, off_t ofs, const void *buf, int len);
int fileino_stat(int ino, struct stat *statbuf);
int fileino_truncate(int ino, off_t newsize);

filedesc *filedesc_alloc(void);
filedesc *filedesc_open(filedesc *fd, const char *path, int flags, mode_t mode);
int filedesc_read(filedesc *fd, void *buf, size_t eltsize, size_t count);
int filedesc_write(filedesc *fd, const void *buf, size_t eltsize, size_t count);
off_t filedesc_seek(filedesc *fd, off_t ofs, int whence);
void filedesc_close(filedesc *fd);

#endif	// !PIOS_INC_FILE_H
#endif	// LAB >= 4