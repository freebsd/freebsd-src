/*
 * See i386-fbsd.c for copyright and license terms.
 *
 * System call arguments come in several flavours:
 * Hex -- values that should be printed in hex (addresses)
 * Octal -- Same as above, but octal
 * Int -- normal integer values (file descriptors, for example)
 * Name -- pointer to a NULL-terminated string.
 * BinString -- pointer to an array of chars, printed via strvisx().
 * Ptr -- pointer to some unspecified structure.  Just print as hex for now.
 * Stat -- a pointer to a stat buffer.  Prints a couple fields.
 * Ioctl -- an ioctl command.  Woefully limited.
 * Quad -- a double-word value.  e.g., lseek(int, offset_t, int)
 * Signal -- a signal number.  Prints the signal name (SIGxxx)
 * Sockaddr -- a pointer to a struct sockaddr.  Prints symbolic AF, and IP:Port
 * StringArray -- a pointer to an array of string pointers.
 * Timespec -- a pointer to a struct timespec.  Prints both elements.
 * Timeval -- a pointer to a struct timeval.  Prints both elements.
 * Timeval2 -- a pointer to two struct timevals.  Prints both elements of both.
 * Itimerval -- a pointer to a struct itimerval.  Prints all elements.
 * Pollfd -- a pointer to an array of struct pollfd.  Prints .fd and .events.
 * Fd_set -- a pointer to an array of fd_set.  Prints the fds that are set.
 * Sigaction -- a pointer to a struct sigaction.  Prints all elements.
 * Umtx -- a pointer to a struct umtx.  Prints the value of owner.
 * Sigset -- a pointer to a sigset_t.  Prints the signals that are set.
 * Sigprocmask -- the first argument to sigprocmask().  Prints the name.
 * Kevent -- a pointer to an array of struct kevents.  Prints all elements.
 * Pathconf -- the 2nd argument of patchconf().
 *
 * In addition, the pointer types (String, Ptr) may have OUT masked in --
 * this means that the data is set on *return* from the system call -- or
 * IN (meaning that the data is passed *into* the system call).
 */
/*
 * $FreeBSD$
 */

enum Argtype { None = 1, Hex, Octal, Int, Name, Ptr, Stat, Ioctl, Quad,
	Signal, Sockaddr, StringArray, Timespec, Timeval, Itimerval, Pollfd,
	Fd_set, Sigaction, Fcntl, Mprot, Mmapflags, Whence, Readlinkres,
	Umtx, Sigset, Sigprocmask, Kevent, Sockdomain, Socktype, Open,
	Fcntlflag, Rusage, BinString, Shutdown, Resource, Rlimit, Timeval2,
	Pathconf };

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
char *print_arg(int, struct syscall_args *, unsigned long*, long, struct trussinfo *);
void print_syscall(struct trussinfo *, const char *, int, char **);
void print_syscall_ret(struct trussinfo *, const char *, int, char **, int,
    long);
