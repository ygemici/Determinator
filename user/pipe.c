#if LAB >= 6
#include "lib.h"

#define debug 0

static int pipeclose(struct Fd*);
static int piperead(struct Fd *fd, void *buf, u_int n, u_int offset);
static int pipestat(struct Fd*, struct Stat*);
static int pipewrite(struct Fd *fd, const void *buf, u_int n, u_int offset);

struct Dev devpipe =
{
.dev_id=	'p',
.dev_name=	"pipe",
.dev_read=	piperead,
.dev_write=	pipewrite,
.dev_close=	pipeclose,
.dev_stat=	pipestat,
};

#define BY2PIPE 32		// small to provoke races

struct Pipe {
	u_int p_rpos;		// read position
	u_int p_wpos;		// write position
	u_char p_buf[BY2PIPE];	// data buffer
};

int
pipe(int pfd[2])
{
	int r, va;
	struct Fd *fd0, *fd1;

	// allocate the file descriptor table entries
	if ((r = fd_alloc(&fd0)) < 0
	||  (r = sys_mem_alloc(0, (u_int)fd0, PTE_P|PTE_W|PTE_U|PTE_LIBRARY)) < 0)
		goto err;

	if ((r = fd_alloc(&fd1)) < 0
	||  (r = sys_mem_alloc(0, (u_int)fd1, PTE_P|PTE_W|PTE_U|PTE_LIBRARY)) < 0)
		goto err1;

	// allocate the pipe structure as first data page in both
	va = fd2data(fd0);
	if ((r = sys_mem_alloc(0, va, PTE_P|PTE_W|PTE_U|PTE_LIBRARY)) < 0)
		goto err2;
	if ((r = sys_mem_map(0, va, 0, fd2data(fd1), PTE_P|PTE_W|PTE_U|PTE_LIBRARY)) < 0)
		goto err3;

	// set up fd structures
	fd0->fd_dev_id = devpipe.dev_id;
	fd0->fd_omode = O_RDONLY;

	fd1->fd_dev_id = devpipe.dev_id;
	fd1->fd_omode = O_WRONLY;

	if (debug) printf("[%08x] pipecreate %08x\n", env->env_id, vpt[VPN(va)]);

	pfd[0] = fd2num(fd0);
	pfd[1] = fd2num(fd1);
	return 0;

err3:	sys_mem_unmap(0, va);
err2:	sys_mem_unmap(0, (u_int)fd1);
err1:	sys_mem_unmap(0, (u_int)fd0);
err:	return r;
}

static int
piperead(struct Fd *fd, void *vbuf, u_int n, u_int offset)
{
#if SOL >= 6
	u_char *buf;
	u_int i;
	struct Pipe *p;

	(void)offset;	// shut up compiler

	p = (struct Pipe*)fd2data(fd);
	if (debug) printf("[%08x] piperead %08x %d rpos %d wpos %d\n",
		env->env_id, vpt[VPN(p)], n, p->p_rpos, p->p_wpos);

	buf = vbuf;
	for (i=0; i<n; i++) {
		while (p->p_rpos == p->p_wpos) {
			// pipe is empty
			// if we got any data, return it
			if (i > 0)
				return i;
			// if all the writers are gone, note eof
			if (pageref(fd) == pageref(p))
				return 0;
			// yield and see what happens
			if (debug) printf("piperead yield\n");
			sys_yield();
		}
		// there's a byte.  take it.
		buf[i] = p->p_buf[p->p_rpos++%BY2PIPE];
	}
	return i;
#else
	// Your code here.  See the lab text for a description of
	// what piperead needs to do.  Write a loop that 
	// transfers one byte at a time.  If you decide you need
	// to yield (because the pipe is empty), only yield if
	// you have not yet copied any bytes.  (If you have copied
	// some bytes, return what you have instead of yielding.)
	// If the pipe is empty and closed and you didn't copy any data out, return 0.
	panic("piperead not implemented");
	return -E_INVAL;
#endif
}

static int
pipewrite(struct Fd *fd, const void *vbuf, u_int n, u_int offset)
{
#if SOL >= 6
	const u_char *buf;
	u_int i;
	struct Pipe *p;

	(void)offset;	// shut up compiler

	p = (struct Pipe*)fd2data(fd);
	if (debug) printf("[%08x] pipewrite %08x %d rpos %d wpos %d\n",
			env->env_id, vpt[VPN(p)], n, p->p_rpos, p->p_wpos);

	buf = vbuf;
	for (i=0; i<n; i++) {
		while (p->p_wpos >= p->p_rpos+sizeof(p->p_buf)) {
			// pipe is full
			// if all the readers are gone (it's only writers like us now), note eof
			if (pageref(fd) == pageref(p))
				return 0;
			// yield and see what happens
			if (debug) printf("pipewrite yield\n");
			sys_yield();
		}
		// there's room for a byte.  store it.
		p->p_buf[p->p_wpos++%BY2PIPE] = buf[i];
	}
	return i;
#else
	// Your code here.  See the lab text for a description of what 
	// pipewrite needs to do.  Write a loop that transfers one byte
	// at a time.  Unlike in read, it is not okay to write only some
	// of the data.  If the pipe fills and you've only copied some of
	// the data, wait for the pipe to empty and then keep copying.
	// If the pipe is full and closed, return 0.

	panic("pipewrite not implemented");
	return -E_INVAL;
#endif
}

static int
pipestat(struct Fd *fd, struct Stat *stat)
{
	struct Pipe *p;

	p = (struct Pipe*)fd2data(fd);
	strcpy(stat->st_name, "<pipe>");
	stat->st_size = p->p_wpos - p->p_rpos;
	stat->st_isdir = 0;
	stat->st_dev = &devpipe;
	return 0;
}

static int
pipeclose(struct Fd *fd)
{
	sys_mem_unmap(0, fd2data(fd));
	return 0;
}

#endif /* LAB >= 6 */