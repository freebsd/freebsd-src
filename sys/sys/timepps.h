/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@FreeBSD.org> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $Id$
 *
 * The is a FreeBSD protype version of the "draft-mogul-pps-api-02.txt" 
 * specification for Pulse Per Second timing interfaces.  
 *
 */

#ifndef _SYS_TIMEPPS_H_
#define _SYS_TIMEPPS_H_

#include <sys/ioccom.h>

typedef int pps_handle_t;	

typedef unsigned pps_seq_t;

typedef struct {
	pps_seq_t	assert_sequence;
	pps_seq_t	clear_sequence;
	struct timespec	assert_timestamp;
	struct timespec	clear_timestamp;
	int		current_mode;
} pps_info_t;

typedef struct {
	int		mode;
	struct timespec	assert_offset;
	struct timespec	clear_offset;
} pps_params_t;

#define PPS_CAPTUREASSERT	0x01
#define PPS_CAPTURECLEAR	0x01
#define PPS_CAPTUREBOTH		0x03

#define PPS_HARDPPSONASSERT	0x04
#define PPS_HARDPPSONCLEAR	0x08

#define PPS_OFFSETASSERT	0x10
#define PPS_OFFSETCLEAR		0x20

#define PPS_ECHOASSERT		0x40
#define PPS_ECHOCLEAR		0x80

#define PPS_CANWAIT		0x100

struct pps_wait_args {
	struct timespec	timeout;
	pps_info_t	pps_info_buf;
};

#define PPS_IOC_CREATE		_IO('1', 1)
#define PPS_IOC_DESTROY		_IO('1', 2)
#define PPS_IOC_SETPARAMS	_IOW('1', 3, pps_params_t)
#define PPS_IOC_GETPARAMS	_IOR('1', 4, pps_params_t)
#define PPS_IOC_GETCAP		_IOR('1', 5, int)
#define PPS_IOC_FETCH		_IOWR('1', 6, pps_info_t)
#define PPS_IOC_WAIT		_IOWR('1', 6, struct pps_wait_args)

#endif /* _SYS_TIMEPPS_H_ */
