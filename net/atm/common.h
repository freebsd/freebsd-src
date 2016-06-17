/* net/atm/common.h - ATM sockets (common part for PVC and SVC) */
 
/* Written 1995-2000 by Werner Almesberger, EPFL LRC/ICA */


#ifndef NET_ATM_COMMON_H
#define NET_ATM_COMMON_H

#include <linux/net.h>
#include <linux/poll.h> /* for poll_table */


int vcc_create(struct socket *sock, int protocol, int family);
int vcc_release(struct socket *sock);
int vcc_connect(struct socket *sock, int itf, short vpi, int vci);
int vcc_recvmsg(struct socket *sock, struct msghdr *msg,
		int size, int flags, struct scm_cookie *scm);
int vcc_sendmsg(struct socket *sock, struct msghdr *m, int total_len,
		struct scm_cookie *scm);
unsigned int atm_poll(struct file *file,struct socket *sock,poll_table *wait);
int vcc_ioctl(struct socket *sock, unsigned int cmd, unsigned long arg);
int vcc_setsockopt(struct socket *sock, int level, int optname, char *optval,
		   int optlen);
int vcc_getsockopt(struct socket *sock, int level, int optname, char *optval,
		   int *optlen);

void atm_shutdown_dev(struct atm_dev *dev);

void pppoatm_ioctl_set(int (*hook)(struct atm_vcc *, unsigned int, unsigned long));
void br2684_ioctl_set(int (*hook)(struct atm_vcc *, unsigned int, unsigned long));

int atmpvc_init(void);
void atmpvc_exit(void);
int atmsvc_init(void);
void atmsvc_exit(void);
int atm_proc_init(void);
void atm_proc_exit(void);

/* SVC */

void svc_callback(struct atm_vcc *vcc);
int svc_change_qos(struct atm_vcc *vcc,struct atm_qos *qos);

/* p2mp */

int create_leaf(struct socket *leaf,struct socket *session);

#endif
