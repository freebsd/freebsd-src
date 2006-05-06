/*
 * $FreeBSD: src/sys/dev/ida/idaio.h,v 1.1 2004/01/15 06:37:52 mdodd Exp $
 */

#include <sys/ioccom.h>

struct ida_user_command {
	int		command;
	int		drive;
	u_int32_t	blkno;
	union {
		struct ida_drive_info		di;
		struct ida_drive_info_ext	die;
		struct ida_controller_info	ci;
		struct ida_drive_status		ds;
		struct ida_phys_drv_info	pdi;
		struct ida_blink_drv_leds	bdl;
		struct ida_label_logical	ll;
		u_int8_t buf;
	} d;
};

#define	IDAIO_COMMAND	_IOWR('I', 100, struct ida_user_command)
