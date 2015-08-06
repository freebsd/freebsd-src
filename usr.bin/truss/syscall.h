/*
 * See i386-fbsd.c for copyright and license terms.
 *
 * System call arguments come in several flavours:
 * Hex -- values that should be printed in hex (addresses)
 * Octal -- Same as above, but octal
 * Int -- normal integer values (file descriptors, for example)
 * LongHex -- long value that should be printed in hex
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
 * Sigset -- a pointer to a sigset_t.  Prints the signals that are set.
 * Sigprocmask -- the first argument to sigprocmask().  Prints the name.
 * Kevent -- a pointer to an array of struct kevents.  Prints all elements.
 * Pathconf -- the 2nd argument of pathconf().
 *
 * In addition, the pointer types (String, Ptr) may have OUT masked in --
 * this means that the data is set on *return* from the system call -- or
 * IN (meaning that the data is passed *into* the system call).
 */
/*
 * $FreeBSD$
 */

enum Argtype { None = 1, Hex, Octal, Int, LongHex, Name, Ptr, Stat, Ioctl, Quad,
	Signal, Sockaddr, StringArray, Timespec, Timeval, Itimerval, Pollfd,
	Fd_set, Sigaction, Fcntl, Mprot, Mmapflags, Whence, Readlinkres,
	Sigset, Sigprocmask, Kevent, Sockdomain, Socktype, Open,
	Fcntlflag, Rusage, BinString, Shutdown, Resource, Rlimit, Timeval2,
	Pathconf, Rforkflags, ExitStatus, Waitoptions, Idtype, Procctl,
	LinuxSockArgs, Umtxop, Atfd, Atflags, Timespec2, Accessmode };

#define	ARG_MASK	0xff
#define	OUT	0x100
#define	IN	/*0x20*/0

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
	struct timespec time; /* Time spent for this call */
	int ncalls;	/* Number of calls */
	int nerror;	/* Number of calls that returned with error */
};

struct syscall *get_syscall(const char*);
char *print_arg(struct syscall_args *, unsigned long*, long, struct trussinfo *);

/*
 * Linux Socket defines
 */
#define LINUX_SOCKET		1
#define LINUX_BIND		2
#define LINUX_CONNECT		3
#define LINUX_LISTEN		4
#define LINUX_ACCEPT		5
#define LINUX_GETSOCKNAME	6
#define LINUX_GETPEERNAME	7
#define LINUX_SOCKETPAIR	8
#define LINUX_SEND		9
#define LINUX_RECV		10
#define LINUX_SENDTO		11
#define LINUX_RECVFROM		12
#define LINUX_SHUTDOWN		13
#define LINUX_SETSOCKOPT	14
#define LINUX_GETSOCKOPT	15
#define LINUX_SENDMSG		16
#define LINUX_RECVMSG		17 

#define PAD_(t) (sizeof(register_t) <= sizeof(t) ? \
    0 : sizeof(register_t) - sizeof(t))
    
#if BYTE_ORDER == LITTLE_ENDIAN
#define PADL_(t)	0
#define PADR_(t)	PAD_(t)
#else
#define PADL_(t)	PAD_(t)
#define PADR_(t)	0
#endif

typedef int     l_int;
typedef uint32_t    l_ulong;

struct linux_socketcall_args {
    char what_l_[PADL_(l_int)]; l_int what; char what_r_[PADR_(l_int)];
    char args_l_[PADL_(l_ulong)]; l_ulong args; char args_r_[PADR_(l_ulong)];
};

void print_syscall(struct trussinfo *, const char *, int, char **);
void print_syscall_ret(struct trussinfo *, const char *, int, char **, int,
    long, struct syscall *);
void print_summary(struct trussinfo *trussinfo);
