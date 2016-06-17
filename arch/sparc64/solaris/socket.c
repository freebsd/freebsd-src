/* $Id: socket.c,v 1.5 2001/02/13 01:16:44 davem Exp $
 * socket.c: Socket syscall emulation for Solaris 2.6+
 *
 * Copyright (C) 1998 Jakub Jelinek (jj@ultra.linux.cz)
 *
 * 1999-08-19 Fixed socketpair code 
 *            Jason Rappleye (rappleye@ccr.buffalo.edu)
 */

#include <linux/types.h>
#include <linux/smp_lock.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/socket.h>
#include <linux/file.h>

#include <asm/uaccess.h>
#include <asm/string.h>
#include <asm/oplib.h>
#include <asm/idprom.h>

#include "conv.h"

#define SOCK_SOL_STREAM		2
#define SOCK_SOL_DGRAM		1
#define SOCK_SOL_RAW		4
#define SOCK_SOL_RDM		5
#define SOCK_SOL_SEQPACKET	6

#define SOL_SO_SNDLOWAT		0x1003
#define SOL_SO_RCVLOWAT		0x1004
#define SOL_SO_SNDTIMEO		0x1005
#define SOL_SO_RCVTIMEO		0x1006
#define SOL_SO_STATE		0x2000

#define SOL_SS_NDELAY		0x040
#define SOL_SS_NONBLOCK		0x080
#define SOL_SS_ASYNC		0x100

#define SO_STATE		0x000e

static int socket_check(int family, int type)
{
	if (family != PF_UNIX && family != PF_INET)
		return -ESOCKTNOSUPPORT;
	switch (type) {
	case SOCK_SOL_STREAM: type = SOCK_STREAM; break;
	case SOCK_SOL_DGRAM: type = SOCK_DGRAM; break;
	case SOCK_SOL_RAW: type = SOCK_RAW; break;
	case SOCK_SOL_RDM: type = SOCK_RDM; break;
	case SOCK_SOL_SEQPACKET: type = SOCK_SEQPACKET; break;
	default: return -EINVAL;
	}
	return type;
}

static int solaris_to_linux_sockopt(int optname) 
{
	switch (optname) {
	case SOL_SO_SNDLOWAT: optname = SO_SNDLOWAT; break;
	case SOL_SO_RCVLOWAT: optname = SO_RCVLOWAT; break;
	case SOL_SO_SNDTIMEO: optname = SO_SNDTIMEO; break;
	case SOL_SO_RCVTIMEO: optname = SO_RCVTIMEO; break;
	case SOL_SO_STATE: optname = SO_STATE; break;
	};
	
	return optname;
}
	
asmlinkage int solaris_socket(int family, int type, int protocol)
{
	int (*sys_socket)(int, int, int) =
		(int (*)(int, int, int))SYS(socket);

	type = socket_check (family, type);
	if (type < 0) return type;
	return sys_socket(family, type, protocol);
}

asmlinkage int solaris_socketpair(int *usockvec)
{
	int (*sys_socketpair)(int, int, int, int *) =
		(int (*)(int, int, int, int *))SYS(socketpair);

	/* solaris socketpair really only takes one arg at the syscall
	 * level, int * usockvec. The libs apparently take care of 
	 * making sure that family==AF_UNIX and type==SOCK_STREAM. The 
	 * pointer we really want ends up residing in the first (and
	 * supposedly only) argument.
	 */

	return sys_socketpair(AF_UNIX, SOCK_STREAM, 0, (int *)usockvec);
}

asmlinkage int solaris_bind(int fd, struct sockaddr *addr, int addrlen)
{
	int (*sys_bind)(int, struct sockaddr *, int) =
		(int (*)(int, struct sockaddr *, int))SUNOS(104);

	return sys_bind(fd, addr, addrlen);
}

asmlinkage int solaris_setsockopt(int fd, int level, int optname, u32 optval, int optlen)
{
	int (*sunos_setsockopt)(int, int, int, u32, int) =
		(int (*)(int, int, int, u32, int))SUNOS(105);

	optname = solaris_to_linux_sockopt(optname);
	if (optname < 0)
		return optname;
	if (optname == SO_STATE)
		return 0;

	return sunos_setsockopt(fd, level, optname, optval, optlen);
}

asmlinkage int solaris_getsockopt(int fd, int level, int optname, u32 optval, u32 optlen)
{
	int (*sunos_getsockopt)(int, int, int, u32, u32) =
		(int (*)(int, int, int, u32, u32))SUNOS(118);

	optname = solaris_to_linux_sockopt(optname);
	if (optname < 0)
		return optname;

	if (optname == SO_STATE)
		optname = SOL_SO_STATE;

	return sunos_getsockopt(fd, level, optname, optval, optlen);
}

asmlinkage int solaris_connect(int fd, struct sockaddr *addr, int addrlen)
{
	int (*sys_connect)(int, struct sockaddr *, int) =
		(int (*)(int, struct sockaddr *, int))SYS(connect);

	return sys_connect(fd, addr, addrlen);
}

asmlinkage int solaris_accept(int fd, struct sockaddr *addr, int *addrlen)
{
	int (*sys_accept)(int, struct sockaddr *, int *) =
		(int (*)(int, struct sockaddr *, int *))SYS(accept);

	return sys_accept(fd, addr, addrlen);
}

asmlinkage int solaris_listen(int fd, int backlog)
{
	int (*sys_listen)(int, int) =
		(int (*)(int, int))SUNOS(106);

	return sys_listen(fd, backlog);
}

asmlinkage int solaris_shutdown(int fd, int how)
{
	int (*sys_shutdown)(int, int) =
		(int (*)(int, int))SYS(shutdown);

	return sys_shutdown(fd, how);
}

#define MSG_SOL_OOB		0x1
#define MSG_SOL_PEEK		0x2
#define MSG_SOL_DONTROUTE	0x4
#define MSG_SOL_EOR		0x8
#define MSG_SOL_CTRUNC		0x10
#define MSG_SOL_TRUNC		0x20
#define MSG_SOL_WAITALL		0x40
#define MSG_SOL_DONTWAIT	0x80

static int solaris_to_linux_msgflags(int flags)
{
	int fl = flags & (MSG_OOB|MSG_PEEK|MSG_DONTROUTE);
	
	if (flags & MSG_SOL_EOR) fl |= MSG_EOR;
	if (flags & MSG_SOL_CTRUNC) fl |= MSG_CTRUNC;
	if (flags & MSG_SOL_TRUNC) fl |= MSG_TRUNC;
	if (flags & MSG_SOL_WAITALL) fl |= MSG_WAITALL;
	if (flags & MSG_SOL_DONTWAIT) fl |= MSG_DONTWAIT;
	return fl;
}

static int linux_to_solaris_msgflags(int flags)
{
	int fl = flags & (MSG_OOB|MSG_PEEK|MSG_DONTROUTE);
	
	if (flags & MSG_EOR) fl |= MSG_SOL_EOR;
	if (flags & MSG_CTRUNC) fl |= MSG_SOL_CTRUNC;
	if (flags & MSG_TRUNC) fl |= MSG_SOL_TRUNC;
	if (flags & MSG_WAITALL) fl |= MSG_SOL_WAITALL;
	if (flags & MSG_DONTWAIT) fl |= MSG_SOL_DONTWAIT;
	return fl;
}

asmlinkage int solaris_recvfrom(int s, char *buf, int len, int flags, u32 from, u32 fromlen)
{
	int (*sys_recvfrom)(int, void *, size_t, unsigned, struct sockaddr *, int *) =
		(int (*)(int, void *, size_t, unsigned, struct sockaddr *, int *))SYS(recvfrom);
	
	return sys_recvfrom(s, buf, len, solaris_to_linux_msgflags(flags), (struct sockaddr *)A(from), (int *)A(fromlen));
}

asmlinkage int solaris_recv(int s, char *buf, int len, int flags)
{
	int (*sys_recvfrom)(int, void *, size_t, unsigned, struct sockaddr *, int *) =
		(int (*)(int, void *, size_t, unsigned, struct sockaddr *, int *))SYS(recvfrom);
	
	return sys_recvfrom(s, buf, len, solaris_to_linux_msgflags(flags), NULL, NULL);
}

asmlinkage int solaris_sendto(int s, char *buf, int len, int flags, u32 to, u32 tolen)
{
	int (*sys_sendto)(int, void *, size_t, unsigned, struct sockaddr *, int *) =
		(int (*)(int, void *, size_t, unsigned, struct sockaddr *, int *))SYS(sendto);
	
	return sys_sendto(s, buf, len, solaris_to_linux_msgflags(flags), (struct sockaddr *)A(to), (int *)A(tolen));
}

asmlinkage int solaris_send(int s, char *buf, int len, int flags)
{
	int (*sys_sendto)(int, void *, size_t, unsigned, struct sockaddr *, int *) =
		(int (*)(int, void *, size_t, unsigned, struct sockaddr *, int *))SYS(sendto);
	
	return sys_sendto(s, buf, len, solaris_to_linux_msgflags(flags), NULL, NULL);
}

asmlinkage int solaris_getpeername(int fd, struct sockaddr *addr, int *addrlen)
{
	int (*sys_getpeername)(int, struct sockaddr *, int *) =
		(int (*)(int, struct sockaddr *, int *))SYS(getpeername);

	return sys_getpeername(fd, addr, addrlen);
}

asmlinkage int solaris_getsockname(int fd, struct sockaddr *addr, int *addrlen)
{
	int (*sys_getsockname)(int, struct sockaddr *, int *) =
		(int (*)(int, struct sockaddr *, int *))SYS(getsockname);

	return sys_getsockname(fd, addr, addrlen);
}

/* XXX This really belongs in some header file... -DaveM */
#define MAX_SOCK_ADDR	128		/* 108 for Unix domain - 
					   16 for IP, 16 for IPX,
					   24 for IPv6,
					   about 80 for AX.25 */

/* XXX These as well... */
extern __inline__ struct socket *socki_lookup(struct inode *inode)
{
	return &inode->u.socket_i;
}

extern __inline__ struct socket *sockfd_lookup(int fd, int *err)
{
	struct file *file;
	struct inode *inode;

	if (!(file = fget(fd))) {
		*err = -EBADF;
		return NULL;
	}

	inode = file->f_dentry->d_inode;
	if (!inode->i_sock || !socki_lookup(inode)) {
		*err = -ENOTSOCK;
		fput(file);
		return NULL;
	}

	return socki_lookup(inode);
}

extern __inline__ void sockfd_put(struct socket *sock)
{
	fput(sock->file);
}

struct sol_nmsghdr {
	u32		msg_name;
	int		msg_namelen;
	u32		msg_iov;
	u32		msg_iovlen;
	u32		msg_control;
	u32		msg_controllen;
	u32		msg_flags;
};

struct sol_cmsghdr {
	u32		cmsg_len;
	int		cmsg_level;
	int		cmsg_type;
	unsigned char	cmsg_data[0];
};

struct iovec32 {
	u32		iov_base;
	u32 iov_len;
};

static inline int iov_from_user32_to_kern(struct iovec *kiov,
					  struct iovec32 *uiov32,
					  int niov)
{
	int tot_len = 0;

	while(niov > 0) {
		u32 len, buf;

		if(get_user(len, &uiov32->iov_len) ||
		   get_user(buf, &uiov32->iov_base)) {
			tot_len = -EFAULT;
			break;
		}
		tot_len += len;
		kiov->iov_base = (void *)A(buf);
		kiov->iov_len = (__kernel_size_t) len;
		uiov32++;
		kiov++;
		niov--;
	}
	return tot_len;
}

static inline int msghdr_from_user32_to_kern(struct msghdr *kmsg,
					     struct sol_nmsghdr *umsg)
{
	u32 tmp1, tmp2, tmp3;
	int err;

	err = get_user(tmp1, &umsg->msg_name);
	err |= __get_user(tmp2, &umsg->msg_iov);
	err |= __get_user(tmp3, &umsg->msg_control);
	if (err)
		return -EFAULT;

	kmsg->msg_name = (void *)A(tmp1);
	kmsg->msg_iov = (struct iovec *)A(tmp2);
	kmsg->msg_control = (void *)A(tmp3);

	err = get_user(kmsg->msg_namelen, &umsg->msg_namelen);
	err |= get_user(kmsg->msg_controllen, &umsg->msg_controllen);
	err |= get_user(kmsg->msg_flags, &umsg->msg_flags);
	
	kmsg->msg_flags = solaris_to_linux_msgflags(kmsg->msg_flags);
	
	return err;
}

/* I've named the args so it is easy to tell whose space the pointers are in. */
static int verify_iovec32(struct msghdr *kern_msg, struct iovec *kern_iov,
			  char *kern_address, int mode)
{
	int tot_len;

	if(kern_msg->msg_namelen) {
		if(mode==VERIFY_READ) {
			int err = move_addr_to_kernel(kern_msg->msg_name,
						      kern_msg->msg_namelen,
						      kern_address);
			if(err < 0)
				return err;
		}
		kern_msg->msg_name = kern_address;
	} else
		kern_msg->msg_name = NULL;

	if(kern_msg->msg_iovlen > UIO_FASTIOV) {
		kern_iov = kmalloc(kern_msg->msg_iovlen * sizeof(struct iovec),
				   GFP_KERNEL);
		if(!kern_iov)
			return -ENOMEM;
	}

	tot_len = iov_from_user32_to_kern(kern_iov,
					  (struct iovec32 *)kern_msg->msg_iov,
					  kern_msg->msg_iovlen);
	if(tot_len >= 0)
		kern_msg->msg_iov = kern_iov;
	else if(kern_msg->msg_iovlen > UIO_FASTIOV)
		kfree(kern_iov);

	return tot_len;
}

asmlinkage int solaris_sendmsg(int fd, struct sol_nmsghdr *user_msg, unsigned user_flags)
{
	struct socket *sock;
	char address[MAX_SOCK_ADDR];
	struct iovec iov[UIO_FASTIOV];
	unsigned char ctl[sizeof(struct cmsghdr) + 20];
	unsigned char *ctl_buf = ctl;
	struct msghdr kern_msg;
	int err, total_len;

	if(msghdr_from_user32_to_kern(&kern_msg, user_msg))
		return -EFAULT;
	if(kern_msg.msg_iovlen > UIO_MAXIOV)
		return -EINVAL;
	err = verify_iovec32(&kern_msg, iov, address, VERIFY_READ);
	if (err < 0)
		goto out;
	total_len = err;

	if(kern_msg.msg_controllen) {
		struct sol_cmsghdr *ucmsg = (struct sol_cmsghdr *)kern_msg.msg_control;
		unsigned long *kcmsg;
		__kernel_size_t32 cmlen;

		if(kern_msg.msg_controllen > sizeof(ctl) &&
		   kern_msg.msg_controllen <= 256) {
			err = -ENOBUFS;
			ctl_buf = kmalloc(kern_msg.msg_controllen, GFP_KERNEL);
			if(!ctl_buf)
				goto out_freeiov;
		}
		__get_user(cmlen, &ucmsg->cmsg_len);
		kcmsg = (unsigned long *) ctl_buf;
		*kcmsg++ = (unsigned long)cmlen;
		err = -EFAULT;
		if(copy_from_user(kcmsg, &ucmsg->cmsg_level,
				  kern_msg.msg_controllen - sizeof(__kernel_size_t32)))
			goto out_freectl;
		kern_msg.msg_control = ctl_buf;
	}
	kern_msg.msg_flags = solaris_to_linux_msgflags(user_flags);

	lock_kernel();
	sock = sockfd_lookup(fd, &err);
	if (sock != NULL) {
		if (sock->file->f_flags & O_NONBLOCK)
			kern_msg.msg_flags |= MSG_DONTWAIT;
		err = sock_sendmsg(sock, &kern_msg, total_len);
		sockfd_put(sock);
	}
	unlock_kernel();

out_freectl:
	/* N.B. Use kfree here, as kern_msg.msg_controllen might change? */
	if(ctl_buf != ctl)
		kfree(ctl_buf);
out_freeiov:
	if(kern_msg.msg_iov != iov)
		kfree(kern_msg.msg_iov);
out:
	return err;
}

asmlinkage int solaris_recvmsg(int fd, struct sol_nmsghdr *user_msg, unsigned int user_flags)
{
	struct iovec iovstack[UIO_FASTIOV];
	struct msghdr kern_msg;
	char addr[MAX_SOCK_ADDR];
	struct socket *sock;
	struct iovec *iov = iovstack;
	struct sockaddr *uaddr;
	int *uaddr_len;
	unsigned long cmsg_ptr;
	int err, total_len, len = 0;

	if(msghdr_from_user32_to_kern(&kern_msg, user_msg))
		return -EFAULT;
	if(kern_msg.msg_iovlen > UIO_MAXIOV)
		return -EINVAL;

	uaddr = kern_msg.msg_name;
	uaddr_len = &user_msg->msg_namelen;
	err = verify_iovec32(&kern_msg, iov, addr, VERIFY_WRITE);
	if (err < 0)
		goto out;
	total_len = err;

	cmsg_ptr = (unsigned long) kern_msg.msg_control;
	kern_msg.msg_flags = 0;

	lock_kernel();
	sock = sockfd_lookup(fd, &err);
	if (sock != NULL) {
		if (sock->file->f_flags & O_NONBLOCK)
			user_flags |= MSG_DONTWAIT;
		err = sock_recvmsg(sock, &kern_msg, total_len, user_flags);
		if(err >= 0)
			len = err;
		sockfd_put(sock);
	}
	unlock_kernel();

	if(uaddr != NULL && err >= 0)
		err = move_addr_to_user(addr, kern_msg.msg_namelen, uaddr, uaddr_len);
	if(err >= 0) {
		err = __put_user(linux_to_solaris_msgflags(kern_msg.msg_flags), &user_msg->msg_flags);
		if(!err) {
			/* XXX Convert cmsg back into userspace 32-bit format... */
			err = __put_user((unsigned long)kern_msg.msg_control - cmsg_ptr,
					 &user_msg->msg_controllen);
		}
	}

	if(kern_msg.msg_iov != iov)
		kfree(kern_msg.msg_iov);
out:
	if(err < 0)
		return err;
	return len;
}
