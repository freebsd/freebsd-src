/*
 * See i386-fbsd.c for copyright and license terms.
 *
 * System call arguments come in several flavours:
 * Hex -- values that should be printed in hex (addresses)
 * Octal -- Same as above, but octal
 * Int -- normal integer values (file descriptors, for example)
 * String -- pointers to sensible data.  Note that we treat read() and
 *	write() arguments as such, even though they may *not* be
 *	printable data.
 * Ptr -- pointer to some specific structure.  Just print as hex for now.
 * Stat -- a pointer to a stat buffer.  Currently unused.
 * Ioctl -- an ioctl command.  Woefully limited.
 * Quad -- a double-word value.  e.g., lseek(int, offset_t, int)
 * Signal -- a signal number.  Prints the signal name (SIGxxx)
 * Sockaddr -- a pointer to a struct sockaddr.  Prints symbolic AF, and IP:Port
 * StringArray -- a pointer to an array of string pointers.
 * Timespec -- a pointer to a struct timespec.  Prints both elements.
 * Timeval -- a pointer to a struct timeval.  Prints both elements.
 * Itimerval -- a pointer to a struct itimerval.  Prints all elements.
 * Pollfd -- a pointer to an array of struct pollfd.  Prints .fd and .events.
 * Fd_set -- a pointer to an array of fd_set.  Prints the fds that are set.
 * Sigaction -- a pointer to a struct sigaction.  Prints all elements.
 *
 * In addition, the pointer types (String, Ptr) may have OUT masked in --
 * this means that the data is set on *return* from the system call -- or
 * IN (meaning that the data is passed *into* the system call).
 */
/*
 * $FreeBSD$
 */

enum Argtype { None = 1, Hex, Octal, Int, String, Ptr, Stat, Ioctl, Quad,
	Signal, Sockaddr, StringArray, Timespec, Timeval, Itimerval, Pollfd, 
	Fd_set, Sigaction, Fcntl, Mprot, Mmapflags, Whence, Readlinkres };

#define ARG_MASK	0xff
#define OUT	0x100
#define IN	/*0x20*/0

struct syscall_args {
	enum Argtype type;
	int offset;
};

struct syscall {
	const char *name;
	int ret_type;	/* 0, 1, or 2 return values */
	int nargs;	/* actual number of meaningful arguments */
			/* Hopefully, no syscalls with > 10 args */
	struct syscall_args args[10];
};

struct syscall *get_syscall(const char*);
char *get_string(int, void*, int);
char *print_arg(int, struct syscall_args *, unsigned long*, long);
void print_syscall(struct trussinfo *, const char *, int, char **);
void print_syscall_ret(struct trussinfo *, const char *, int, char **, int,
    long);
