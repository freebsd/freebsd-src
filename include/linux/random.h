/*
 * include/linux/random.h
 *
 * Include file for the random number generator.
 */

#ifndef _LINUX_RANDOM_H
#define _LINUX_RANDOM_H

#include <linux/ioctl.h>

/* ioctl()'s for the random number generator */

/* Get the entropy count. */
#define RNDGETENTCNT	_IOR( 'R', 0x00, int )

/* Add to (or subtract from) the entropy count.  (Superuser only.) */
#define RNDADDTOENTCNT	_IOW( 'R', 0x01, int )

/* Get the contents of the entropy pool.  (Superuser only.) */
#define RNDGETPOOL	_IOR( 'R', 0x02, int [2] )

/* 
 * Write bytes into the entropy pool and add to the entropy count.
 * (Superuser only.)
 */
#define RNDADDENTROPY	_IOW( 'R', 0x03, int [2] )

/* Clear entropy count to 0.  (Superuser only.) */
#define RNDZAPENTCNT	_IO( 'R', 0x04 )

/* Clear the entropy pool and associated counters.  (Superuser only.) */
#define RNDCLEARPOOL	_IO( 'R', 0x06 )

struct rand_pool_info {
	int	entropy_count;
	int	buf_size;
	__u32	buf[0];
};

/* Exported functions */

#ifdef __KERNEL__

extern void rand_initialize(void);
extern void rand_initialize_irq(int irq);
extern void rand_initialize_blkdev(int irq, int mode);

extern void batch_entropy_store(u32 a, u32 b, int num);

extern void add_keyboard_randomness(unsigned char scancode);
extern void add_mouse_randomness(__u32 mouse_data);
extern void add_interrupt_randomness(int irq);
extern void add_blkdev_randomness(int major);

extern void get_random_bytes(void *buf, int nbytes);
void generate_random_uuid(unsigned char uuid_out[16]);

extern __u32 secure_ip_id(__u32 daddr);
extern __u32 secure_tcp_sequence_number(__u32 saddr, __u32 daddr,
					__u16 sport, __u16 dport);
extern __u32 secure_tcp_syn_cookie(__u32 saddr, __u32 daddr,
				   __u16 sport, __u16 dport,
				   __u32 sseq, __u32 count,
				   __u32 data);
extern __u32 check_tcp_syn_cookie(__u32 cookie, __u32 saddr,
				  __u32 daddr, __u16 sport,
				  __u16 dport, __u32 sseq,
				  __u32 count, __u32 maxdiff);
extern __u32 secure_tcpv6_sequence_number(__u32 *saddr, __u32 *daddr,
					  __u16 sport, __u16 dport);

extern __u32 secure_ipv6_id(__u32 *daddr);

#ifndef MODULE
extern struct file_operations random_fops, urandom_fops;
#endif

#endif /* __KERNEL___ */

#endif /* _LINUX_RANDOM_H */
