/*
 *  drivers/s390/char/hwc_rw.h
 *    interface to the HWC-read/write driver 
 *
 *  S390 version
 *    Copyright (C) 1999 IBM Deutschland Entwicklung GmbH, IBM Corporation
 *    Author(s): Martin Peschke <mpeschke@de.ibm.com>
 */

#ifndef __HWC_RW_H__
#define __HWC_RW_H__

#include <linux/ioctl.h>

typedef struct {

	void (*move_input) (unsigned char *, unsigned int);

	void (*wake_up) (void);
} hwc_high_level_calls_t;

struct _hwc_request;

typedef void hwc_callback_t (struct _hwc_request *);

typedef struct _hwc_request {
	void *block;
	u32 word;
	hwc_callback_t *callback;
	void *data;
} __attribute__ ((packed)) 

hwc_request_t;

#define HWC_ASCEBC(x) ((MACHINE_IS_VM ? _ascebc[x] : _ascebc_500[x]))

#define HWC_EBCASC_STR(s,c) ((MACHINE_IS_VM ? EBCASC(s,c) : EBCASC_500(s,c)))

#define HWC_ASCEBC_STR(s,c) ((MACHINE_IS_VM ? ASCEBC(s,c) : ASCEBC_500(s,c)))

#define IN_HWCB      1
#define IN_WRITE_BUF 2
#define IN_BUFS_TOTAL        (IN_HWCB | IN_WRITE_BUF)

typedef unsigned short int ioctl_htab_t;
typedef unsigned char ioctl_echo_t;
typedef unsigned short int ioctl_cols_t;
typedef signed char ioctl_nl_t;
typedef unsigned short int ioctl_obuf_t;
typedef unsigned char ioctl_case_t;
typedef unsigned char ioctl_delim_t;

typedef struct {
	ioctl_htab_t width_htab;
	ioctl_echo_t echo;
	ioctl_cols_t columns;
	ioctl_nl_t final_nl;
	ioctl_obuf_t max_hwcb;
	ioctl_obuf_t kmem_hwcb;
	ioctl_case_t tolower;
	ioctl_delim_t delim;
} hwc_ioctls_t;

static hwc_ioctls_t _hwc_ioctls;

#define HWC_IOCTL_LETTER 'B'

#define TIOCHWCSHTAB	_IOW(HWC_IOCTL_LETTER, 0, _hwc_ioctls.width_htab)

#define TIOCHWCSECHO	_IOW(HWC_IOCTL_LETTER, 1, _hwc_ioctls.echo)

#define TIOCHWCSCOLS	_IOW(HWC_IOCTL_LETTER, 2, _hwc_ioctls.columns)

#define TIOCHWCSNL	_IOW(HWC_IOCTL_LETTER, 4, _hwc_ioctls.final_nl)

#define TIOCHWCSOBUF	_IOW(HWC_IOCTL_LETTER, 5, _hwc_ioctls.max_hwcb)

#define TIOCHWCSINIT	_IO(HWC_IOCTL_LETTER, 6)

#define TIOCHWCSCASE	_IOW(HWC_IOCTL_LETTER, 7, _hwc_ioctls.tolower)

#define TIOCHWCSDELIM	_IOW(HWC_IOCTL_LETTER, 9, _hwc_ioctls.delim)

#define TIOCHWCGHTAB	_IOR(HWC_IOCTL_LETTER, 10, _hwc_ioctls.width_htab)

#define TIOCHWCGECHO	_IOR(HWC_IOCTL_LETTER, 11, _hwc_ioctls.echo)

#define TIOCHWCGCOLS	_IOR(HWC_IOCTL_LETTER, 12, _hwc_ioctls.columns)

#define TIOCHWCGNL	_IOR(HWC_IOCTL_LETTER, 14, _hwc_ioctls.final_nl)

#define TIOCHWCGOBUF	_IOR(HWC_IOCTL_LETTER, 15, _hwc_ioctls.max_hwcb)

#define TIOCHWCGINIT	_IOR(HWC_IOCTL_LETTER, 16, _hwc_ioctls)

#define TIOCHWCGCASE	_IOR(HWC_IOCTL_LETTER, 17, _hwc_ioctls.tolower)

#define TIOCHWCGDELIM	_IOR(HWC_IOCTL_LETTER, 19, _hwc_ioctls.delim)

#define TIOCHWCGKBUF	_IOR(HWC_IOCTL_LETTER, 20, _hwc_ioctls.max_hwcb)

#define TIOCHWCGCURR	_IOR(HWC_IOCTL_LETTER, 21, _hwc_ioctls)

#ifndef __HWC_RW_C__

extern int hwc_init (void);

extern int hwc_write (int from_user, const unsigned char *, unsigned int);

extern unsigned int hwc_chars_in_buffer (unsigned char);

extern unsigned int hwc_write_room (unsigned char);

extern void hwc_flush_buffer (unsigned char);

extern void hwc_unblank (void);

extern signed int hwc_ioctl (unsigned int, unsigned long);

extern void do_hwc_interrupt (void);

extern int hwc_printk (const char *,...);

extern signed int hwc_register_calls (hwc_high_level_calls_t *);

extern signed int hwc_unregister_calls (hwc_high_level_calls_t *);

extern int hwc_send (hwc_request_t *);

#endif

#endif
