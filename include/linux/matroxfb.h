#ifndef __LINUX_MATROXFB_H__
#define __LINUX_MATROXFB_H__

#include <asm/ioctl.h>
#include <asm/types.h>

struct matroxioc_output_mode {
	__u32	output;		/* which output */
#define MATROXFB_OUTPUT_PRIMARY		0x0000
#define MATROXFB_OUTPUT_SECONDARY	0x0001
#define MATROXFB_OUTPUT_DFP		0x0002
	__u32	mode;		/* which mode */
#define MATROXFB_OUTPUT_MODE_PAL	0x0001
#define MATROXFB_OUTPUT_MODE_NTSC	0x0002
#define MATROXFB_OUTPUT_MODE_MONITOR	0x0080
};
#define MATROXFB_SET_OUTPUT_MODE	_IOW('n',0xFA,sizeof(struct matroxioc_output_mode))
#define MATROXFB_GET_OUTPUT_MODE	_IOWR('n',0xFA,sizeof(struct matroxioc_output_mode))

/* bitfield */
#define MATROXFB_OUTPUT_CONN_PRIMARY	(1 << MATROXFB_OUTPUT_PRIMARY)
#define MATROXFB_OUTPUT_CONN_SECONDARY	(1 << MATROXFB_OUTPUT_SECONDARY)
#define MATROXFB_OUTPUT_CONN_DFP	(1 << MATROXFB_OUTPUT_DFP)
/* connect these outputs to this framebuffer */
#define MATROXFB_SET_OUTPUT_CONNECTION	_IOW('n',0xF8,sizeof(__u32))
/* which outputs are connected to this framebuffer */
#define MATROXFB_GET_OUTPUT_CONNECTION	_IOR('n',0xF8,sizeof(__u32))
/* which outputs are available for this framebuffer */
#define MATROXFB_GET_AVAILABLE_OUTPUTS	_IOR('n',0xF9,sizeof(__u32))
/* which outputs exist on this framebuffer */
#define MATROXFB_GET_ALL_OUTPUTS	_IOR('n',0xFB,sizeof(__u32))

struct matroxfb_queryctrl {
  __u32 id;			/* ID for control */
  char name[32];		/* A suggested label for this control */
  int minimum;			/* Minimum value */
  int maximum;			/* Maximum value */
  unsigned int step;            /* The increment between values of an integer
				   control that are distinct on the hardware */
  int default_value;		/* Driver default value */
  __u32 type;			/* Control type. */
  __u32 flags;			/* Control flags */
  __u32 category;               /* Control category code, useful for 
				   separating controls by function */
  char group[32];               /* A suggested label string for the 
				   control group */
  __u32 reserved[2];
};

enum matroxfb_ctrl_type {
  MATROXFB_CTRL_TYPE_INTEGER=0,	/* An integer-valued control */
  MATROXFB_CTRL_TYPE_BOOLEAN,	/* A boolean-valued control */
  MATROXFB_CTRL_TYPE_MENU,	/* The control has a menu of choices */
  MATROXFB_CTRL_TYPE_BUTTON /* A button which performs an action when clicked */
};

enum matroxfb_ctrl_id {
  MATROXFB_CID_BRIGHTNESS=0x00980900,
  MATROXFB_CID_CONTRAST,
  MATROXFB_CID_SATURATION,
  MATROXFB_CID_HUE,
  MATROXFB_CID_GAMMA	 =0x00980910,
  MATROXFB_CID_TESTOUT	 =0x08000000,
  MATROXFB_CID_DEFLICKER,
  MATROXFB_CID_LAST
};
  

#define MATROXFB_TVOQUERYCTRL	_IOWR('V',36,struct matroxfb_queryctrl)
   
struct matroxfb_control {
  __u32 id;			/* A driver-defined ID */
  int value;			/* The current value, or new value */
};

#define MATROXFB_G_TVOCTRL	_IOWR('V',27,struct matroxfb_control)
#define MATROXFB_S_TVOCTRL	_IOW ('V',28,struct matroxfb_control)

#define FBIO_WAITFORVSYNC	_IOW('F', 0x20, u_int32_t)

#endif

