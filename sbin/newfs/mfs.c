#ifdef MFS

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/errno.h>
#include <sys/signal.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/mount.h>



static void
sighandler()
{
	/*
	 * kernel notifies us that the FS has been mounted successfully.
	 */
	exit(0);
}


void
mfs_mount(addr, len, name, dir, flags)
caddr_t		addr;
unsigned long	len;
char		*name, *dir;
int		flags;
{
	struct mfs_args	margs;
	char		nmbuf[16];
	int		pfeife[2];
	char		buf[1024];
	int		red;



	signal(SIGUSR1, sighandler);
	if (pipe(pfeife) == -1)
		fatal("cannot create pipe: %s", strerror(errno));
	switch (fork()) {
		case -1:
			fatal("cannot fork: %s", strerror(errno));
		case 0:
			/*
			 * child: disassociate from controlling terminal,
			 * and mount the filesystem.
			 */
			dup2(pfeife[1], 2);
			close(0);
			close(1);
			if (pfeife[0] != 2) close(pfeife[0]);
			if (pfeife[1] != 2) close(pfeife[1]);
			setsid();
			(void)chdir("/");
			if (name == 0) {
				sprintf(nmbuf, "MFS:%d", getpid());
				name = nmbuf;
			}
			margs.name  = name;
			margs.base  = addr;
			margs.size  = len;
			margs.flags = MFSMNT_SIGPPID;
			if (mount(MOUNT_MFS, dir, flags, &margs) == -1)
				fatal("mounting MFS: %s", strerror(errno));
		default:
			/*
			 * parent; if the mount system call fails, the
			 * child will write error messages to the pipe.
			 * We duplicate those messages to our stdout.
			 * If the mount succeedet, we will receive a SIGUSR1
			 * (and exit with status 0).
			 */
			close(pfeife[1]);
			while ((red = read(pfeife[0], buf, sizeof(buf))) > 0)
				write(2, buf, red);
			exit(1);
	}
	/* NOTREACHED */
}


caddr_t
mfs_malloc(size)
unsigned long	size;
{
	caddr_t	addr;

	addr = mmap(0, size, PROT_READ | PROT_WRITE,
			MAP_ANON | MAP_SHARED, -1, 0);
	if (addr == (caddr_t)-1)
		fatal("cannot allocate memory: %s", strerror(errno));
	return(addr);
}

void
mfs_mountfile(file, dir, flags)
char		*file, *dir;
int		flags;
{
	caddr_t		addr;
	int		fd;
	struct stat	st;


	fd = open(file, O_RDWR | O_EXLOCK);
	if ((fd == -1) || (fstat(fd, &st) == -1))
		fatal("%s: %s", file, strerror(errno));
	addr = mmap(0, st.st_size, PROT_READ | PROT_WRITE,
			MAP_FILE | MAP_SHARED, fd, 0);
	if (addr == (caddr_t) -1)
		fatal("cannot mmap file: %s", strerror(errno));
	mfs_mount(addr, st.st_size, file, dir, flags);
}

#else
caddr_t
mfs_malloc()
{
	fatal("compiled without MFS support");
	return(0);
}
void mfs_mount() {}
void mfs_mountfile() {mfs_malloc();}
#endif
