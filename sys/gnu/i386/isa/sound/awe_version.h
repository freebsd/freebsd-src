/* AWE32 driver version number */

#ifndef AWE_VERSION_H_DEF
#define AWE_VERSION_H_DEF

#define AWE_VERSION_NUMBER	0x00040203
#define AWEDRV_VERSION		"0.4.2c"
#define AWE_MAJOR_VERSION(id)	(((id) >> 16) & 0xff)
#define AWE_MINOR_VERSION(id)	(((id) >> 8) & 0xff)
#define AWE_TINY_VERSION(id)	((id) & 0xff)

#endif
