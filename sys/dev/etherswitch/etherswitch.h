/*
 * $FreeBSD$
 */

#ifndef __SYS_DEV_ETHERSWITCH_ETHERSWITCH_H
#define __SYS_DEV_ETHERSWITCH_ETHERSWITCH_H

#include <sys/ioccom.h>

#ifdef _KERNEL
extern devclass_t       etherswitch_devclass;
extern driver_t         etherswitch_driver;
#endif /* _KERNEL */

struct etherswitch_reg {
	uint16_t	reg;
	uint16_t	val;
};
typedef struct etherswitch_reg etherswitch_reg_t;

struct etherswitch_phyreg {
	uint16_t	phy;
	uint16_t	reg;
	uint16_t	val;
};
typedef struct etherswitch_phyreg etherswitch_phyreg_t;

#define ETHERSWITCH_NAMEMAX	64

struct etherswitch_info {
	int		es_nports;
	int		es_nvlangroups;
	char		es_name[ETHERSWITCH_NAMEMAX];
};
typedef struct etherswitch_info etherswitch_info_t;

struct etherswitch_port {
	int		es_port;
	int		es_pvid;
	union {
		struct ifreq		es_uifr;
		struct ifmediareq	es_uifmr;
	} es_ifu;
#define es_ifr		es_ifu.es_uifr
#define es_ifmr		es_ifu.es_uifmr
};
typedef struct etherswitch_port etherswitch_port_t;

struct etherswitch_vlangroup {
	int		es_vlangroup;
	int		es_vid;
	int		es_member_ports;
	int		es_untagged_ports;
	int		es_fid;
};
typedef struct etherswitch_vlangroup etherswitch_vlangroup_t;

#define ETHERSWITCH_PORTMASK(_port)	(1 << (_port))

#define IOETHERSWITCHGETINFO		_IOR('i', 1, etherswitch_info_t)
#define IOETHERSWITCHGETREG		_IOWR('i', 2, etherswitch_reg_t)
#define IOETHERSWITCHSETREG		_IOW('i', 3, etherswitch_reg_t)
#define IOETHERSWITCHGETPORT		_IOWR('i', 4, etherswitch_port_t)
#define IOETHERSWITCHSETPORT		_IOW('i', 5, etherswitch_port_t)
#define IOETHERSWITCHGETVLANGROUP	_IOWR('i', 6, etherswitch_vlangroup_t)
#define IOETHERSWITCHSETVLANGROUP	_IOW('i', 7, etherswitch_vlangroup_t)
#define IOETHERSWITCHGETPHYREG		_IOWR('i', 8, etherswitch_phyreg_t)
#define IOETHERSWITCHSETPHYREG		_IOW('i', 9, etherswitch_phyreg_t)

#endif
