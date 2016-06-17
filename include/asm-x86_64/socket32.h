#ifndef SOCKET32_H
#define SOCKET32_H 1

/* XXX This really belongs in some header file... -DaveM */
#define MAX_SOCK_ADDR	128		/* 108 for Unix domain - 
					   16 for IP, 16 for IPX,
					   24 for IPv6,
					   about 80 for AX.25 */

extern struct socket *sockfd_lookup(int fd, int *err);

/* XXX This as well... */
extern __inline__ void sockfd_put(struct socket *sock)
{
	fput(sock->file);
}

struct msghdr32 {
        u32               msg_name;
        int               msg_namelen;
        u32               msg_iov;
        __kernel_size_t32 msg_iovlen;
        u32               msg_control;
        __kernel_size_t32 msg_controllen;
        unsigned          msg_flags;
};

struct cmsghdr32 {
        __kernel_size_t32 cmsg_len;
        int               cmsg_level;
        int               cmsg_type;
};

/* Bleech... */
#define __CMSG32_NXTHDR(ctl, len, cmsg, cmsglen) __cmsg32_nxthdr((ctl),(len),(cmsg),(cmsglen))
#define CMSG32_NXTHDR(mhdr, cmsg, cmsglen) cmsg32_nxthdr((mhdr), (cmsg), (cmsglen))

#define CMSG32_ALIGN(len) ( ((len)+sizeof(int)-1) & ~(sizeof(int)-1) )

#define CMSG32_DATA(cmsg)	((void *)((char *)(cmsg) + CMSG32_ALIGN(sizeof(struct cmsghdr32))))
#define CMSG32_SPACE(len) (CMSG32_ALIGN(sizeof(struct cmsghdr32)) + CMSG32_ALIGN(len))
#define CMSG32_LEN(len) (CMSG32_ALIGN(sizeof(struct cmsghdr32)) + (len))

#define __CMSG32_FIRSTHDR(ctl,len) ((len) >= sizeof(struct cmsghdr32) ? \
				    (struct cmsghdr32 *)(ctl) : \
				    (struct cmsghdr32 *)NULL)
#define CMSG32_FIRSTHDR(msg)	__CMSG32_FIRSTHDR((msg)->msg_control, (msg)->msg_controllen)

__inline__ struct cmsghdr32 *__cmsg32_nxthdr(void *__ctl, __kernel_size_t __size,
					      struct cmsghdr32 *__cmsg, int __cmsg_len)
{
	struct cmsghdr32 * __ptr;

	__ptr = (struct cmsghdr32 *)(((unsigned char *) __cmsg) +
				     CMSG32_ALIGN(__cmsg_len));
	if ((unsigned long)((char*)(__ptr+1) - (char *) __ctl) > __size)
		return NULL;

	return __ptr;
}

__inline__ struct cmsghdr32 *cmsg32_nxthdr (struct msghdr *__msg,
					    struct cmsghdr32 *__cmsg,
					    int __cmsg_len)
{
	return __cmsg32_nxthdr(__msg->msg_control, __msg->msg_controllen,
			       __cmsg, __cmsg_len);
}

#endif
