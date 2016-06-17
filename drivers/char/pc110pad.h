#ifndef _PC110PAD_H
#define _PC110PAD_H

#include <linux/ioctl.h>

enum pc110pad_mode {
	PC110PAD_RAW,		/* bytes as they come out of the hardware */
	PC110PAD_RARE,		/* debounced up/down and absolute x,y */
	PC110PAD_DEBUG,		/* up/down, debounced, transitions, button */
	PC110PAD_PS2,		/* ps2 relative (default) */ 
};


struct pc110pad_params {
	enum pc110pad_mode mode;
	int	bounce_interval;
	int	tap_interval;
	int	irq;
	int	io;
};

#define MS *HZ/1000

/* Appears as device major=10 (MISC), minor=PC110_PAD */

#define PC110PAD_IOCTL_TYPE		0x9a

#define PC110PADIOCGETP _IOR(PC110PAD_IOCTL_TYPE, 0, struct pc110pad_params)
#define PC110PADIOCSETP _IOW(PC110PAD_IOCTL_TYPE, 1, struct pc110pad_params)
 
#endif /* _PC110PAD_H */
