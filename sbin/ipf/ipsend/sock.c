/*
 * sock.c (C) 1995-1998 Darren Reed
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 */
#include <sys/param.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#if defined(__NetBSD__) && defined(__vax__)
/*
 * XXX need to declare boolean_t for _KERNEL <sys/files.h>
 * which ends up including <sys/device.h> for vax.  See PR#32907
 * for further details.
 */
typedef int     boolean_t;
#endif
#include <fcntl.h>
# include <sys/dirent.h>
# ifdef __NetBSD__
#  include <machine/lock.h>
# endif
# ifdef __FreeBSD__
#  define _WANT_FILE
# else
#  define _KERNEL
#  define	KERNEL
# endif
# include <sys/file.h>
# ifdef __FreeBSD__
#  undef _WANT_FILE
# else
#  undef  _KERNEL
#  undef  KERNEL
# endif
#include <nlist.h>
#include <sys/user.h>
#include <sys/socket.h>
#define	_WANT_SOCKET
#include <sys/socketvar.h>
#include <sys/proc.h>
# include <kvm.h>
#ifdef sun
#include <sys/systm.h>
#include <sys/session.h>
#endif
#include <sys/sysctl.h>
#include <sys/filedesc.h>
#include <paths.h>
#include <math.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <net/if.h>
# include <net/route.h>
#include <netinet/ip_var.h>
#define	_WANT_INPCB
#include <netinet/in_pcb.h>
#define	_WANT_TCPCB
#include <netinet/tcp_var.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <pwd.h>
#include "ipsend.h"


int	nproc;
struct	proc	*proc;

#ifndef	KMEM
# ifdef	_PATH_KMEM
#  define	KMEM	_PATH_KMEM
# endif
#endif
#ifndef	KERNEL
# ifdef	_PATH_UNIX
#  define	KERNEL	_PATH_UNIX
# endif
#endif
#ifndef	KMEM
# define	KMEM	"/dev/kmem"
#endif
#ifndef	KERNEL
# define	KERNEL	"/vmunix"
#endif


static	struct	kinfo_proc	*getproc(void);


int
kmemcpy(char *buf, void *pos, int n)
{
	static	int	kfd = -1;
	off_t	offset = (u_long)pos;

	if (kfd == -1)
		kfd = open(KMEM, O_RDONLY);

	if (lseek(kfd, offset, SEEK_SET) == -1)
	    {
		perror("lseek");
		return (-1);
	    }
	if (read(kfd, buf, n) == -1)
	    {
		perror("read");
		return (-1);
	    }
	return (n);
}

struct	nlist	names[4] = {
	{ "_proc" },
	{ "_nproc" },
	{ NULL },
	{ NULL }
	};

static struct
kinfo_proc *getproc(void)
{
	static	struct	kinfo_proc kp;
	pid_t	pid = getpid();
	int	mib[4];
	size_t	n;

	mib[0] = CTL_KERN;
	mib[1] = KERN_PROC;
	mib[2] = KERN_PROC_PID;
	mib[3] = pid;

	n = sizeof(kp);
	if (sysctl(mib, 4, &kp, &n, NULL, 0) == -1)
	    {
		perror("sysctl");
		return (NULL);
	    }
	return (&kp);
}


struct tcpcb *
find_tcp(int tfd, struct  tcpiphdr *ti)
{
	struct	tcpcb	*t;
	struct	inpcb	*i;
	struct	socket	*s;
	struct	filedesc	*fd;
	struct	kinfo_proc	*p;
	struct	file	*f, **o;

	if (!(p = getproc()))
		return (NULL);

	fd = (struct filedesc *)malloc(sizeof(*fd));
	if (fd == NULL)
		return (NULL);
#if defined( __FreeBSD__)
	if (KMCPY(fd, p->ki_fd, sizeof(*fd)) == -1)
	    {
		fprintf(stderr, "read(%#lx,%#lx) failed\n",
			(u_long)p, (u_long)p->ki_fd);
		free(fd);
		return (NULL);
	    }
#else
	if (KMCPY(fd, p->kp_proc.p_fd, sizeof(*fd)) == -1)
	    {
		fprintf(stderr, "read(%#lx,%#lx) failed\n",
			(u_long)p, (u_long)p->kp_proc.p_fd);
		free(fd);
		return (NULL);
	    }
#endif

	o = NULL;
	f = NULL;
	s = NULL;
	i = NULL;
	t = NULL;

	o = (struct file **)calloc(fd->fd_lastfile + 1, sizeof(*o));
	if (KMCPY(o, fd->fd_ofiles, (fd->fd_lastfile + 1) * sizeof(*o)) == -1)
	    {
		fprintf(stderr, "read(%#lx,%#lx,%lu) - u_ofile - failed\n",
			(u_long)fd->fd_ofiles, (u_long)o, (u_long)sizeof(*o));
		goto finderror;
	    }
	f = (struct file *)calloc(1, sizeof(*f));
	if (KMCPY(f, o[tfd], sizeof(*f)) == -1)
	    {
		fprintf(stderr, "read(%#lx,%#lx,%lu) - o[tfd] - failed\n",
			(u_long)o[tfd], (u_long)f, (u_long)sizeof(*f));
		goto finderror;
	    }

	s = (struct socket *)calloc(1, sizeof(*s));
	if (KMCPY(s, f->f_data, sizeof(*s)) == -1)
	    {
		fprintf(stderr, "read(%#lx,%#lx,%lu) - f_data - failed\n",
			(u_long)f->f_data, (u_long)s, (u_long)sizeof(*s));
		goto finderror;
	    }

	i = (struct inpcb *)calloc(1, sizeof(*i));
	if (KMCPY(i, s->so_pcb, sizeof(*i)) == -1)
	    {
		fprintf(stderr, "kvm_read(%#lx,%#lx,%lu) - so_pcb - failed\n",
			(u_long)s->so_pcb, (u_long)i, (u_long)sizeof(*i));
		goto finderror;
	    }

	t = (struct tcpcb *)calloc(1, sizeof(*t));
	if (KMCPY(t, i->inp_ppcb, sizeof(*t)) == -1)
	    {
		fprintf(stderr, "read(%#lx,%#lx,%lu) - inp_ppcb - failed\n",
			(u_long)i->inp_ppcb, (u_long)t, (u_long)sizeof(*t));
		goto finderror;
	    }
	return (struct tcpcb *)i->inp_ppcb;

finderror:
	if (o != NULL)
		free(o);
	if (f != NULL)
		free(f);
	if (s != NULL)
		free(s);
	if (i != NULL)
		free(i);
	if (t != NULL)
		free(t);
	return (NULL);
}

int
do_socket(char *dev, int mtu, struct  tcpiphdr *ti, struct  in_addr gwip)
{
	struct	sockaddr_in	rsin, lsin;
	struct	tcpcb	*t, tcb;
	int	fd, nfd;
	socklen_t len;

	printf("Dest. Port: %d\n", ti->ti_dport);

	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd == -1)
	    {
		perror("socket");
		return (-1);
	    }

	if (fcntl(fd, F_SETFL, FNDELAY) == -1)
	    {
		perror("fcntl");
		return (-1);
	    }

	bzero((char *)&lsin, sizeof(lsin));
	lsin.sin_family = AF_INET;
	bcopy((char *)&ti->ti_src, (char *)&lsin.sin_addr,
	      sizeof(struct in_addr));
	if (bind(fd, (struct sockaddr *)&lsin, sizeof(lsin)) == -1)
	    {
		perror("bind");
		return (-1);
	    }
	len = sizeof(lsin);
	(void) getsockname(fd, (struct sockaddr *)&lsin, &len);
	ti->ti_sport = lsin.sin_port;
	printf("sport %d\n", ntohs(lsin.sin_port));

	nfd = initdevice(dev, 1);
	if (nfd == -1)
		return (-1);

	if (!(t = find_tcp(fd, ti)))
		return (-1);

	bzero((char *)&rsin, sizeof(rsin));
	rsin.sin_family = AF_INET;
	bcopy((char *)&ti->ti_dst, (char *)&rsin.sin_addr,
	      sizeof(struct in_addr));
	rsin.sin_port = ti->ti_dport;
	if (connect(fd, (struct sockaddr *)&rsin, sizeof(rsin)) == -1 &&
	    errno != EINPROGRESS)
	    {
		perror("connect");
		return (-1);
	    }
	KMCPY(&tcb, t, sizeof(tcb));
	ti->ti_win = tcb.rcv_adv;
	ti->ti_seq = tcb.snd_nxt - 1;
	ti->ti_ack = tcb.rcv_nxt;

	if (send_tcp(nfd, mtu, (ip_t *)ti, gwip) == -1)
		return (-1);
	(void)write(fd, "Hello World\n", 12);
	sleep(2);
	close(fd);
	return (0);
}
