/*
 * General definitions for the COMX driver 
 * 
 * Original authors:  Arpad Bakay <bakay.arpad@synergon.hu>,
 *                    Peter Bajan <bajan.peter@synergon.hu>,
 * Previous maintainer: Tivadar Szemethy <tiv@itc.hu>
 * Currently maintained by: Gergely Madarasz <gorgo@itc.hu>
 *
 * Copyright (C) 1995-1999 ITConsult-Pro Co. <info@itc.hu>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 *
 *
 * net_device_stats:
 *	rx_length_errors	rec_len < 4 || rec_len > 2000
 *	rx_over_errors		receive overrun (OVR)
 *	rx_crc_errors		rx crc error
 *	rx_frame_errors		aborts rec'd (ABO)
 *	rx_fifo_errors		status fifo overrun (PBUFOVR)
 *	rx_missed_errors	receive buffer overrun (BUFOVR)
 *	tx_aborted_errors	?
 *	tx_carrier_errors	modem line status changes
 *	tx_fifo_errors		tx underrun (locomx)
 */
#include <linux/config.h>

struct comx_protocol {
	char	*name;
	char	*version;
	unsigned short encap_type;
	int	(*line_init)(struct net_device *dev);
	int	(*line_exit)(struct net_device *dev);
	struct comx_protocol *next;
	};

struct comx_hardware {
	char *name; 
	char *version;
	int	(*hw_init)(struct net_device *dev);
	int	(*hw_exit)(struct net_device *dev);
	int	(*hw_dump)(struct net_device *dev);
	struct comx_hardware *next;
	};

struct comx_channel {
	void		*if_ptr;	// General purpose pointer
	struct net_device 	*dev;		// Where we belong to
	struct net_device	*twin;		// On dual-port cards
	struct proc_dir_entry *procdir;	// the directory

	unsigned char	init_status;
	unsigned char	line_status;

	struct timer_list lineup_timer;	// against line jitter
	long int	lineup_pending;
	unsigned char	lineup_delay;

#if 0
	struct timer_list reset_timer; // for board resetting
	long		reset_pending;
	int		reset_timeout;
#endif

	struct net_device_stats	stats;	
	struct net_device_stats *current_stats;
#if 0
	unsigned long	board_resets;
#endif
	unsigned long 	*avg_bytes;
	int		loadavg_counter, loadavg_size;
	int		loadavg[3];
	struct timer_list loadavg_timer;
	int		debug_flags;
	char 		*debug_area;
	int		debug_start, debug_end, debug_size;
	struct proc_dir_entry *debug_file;
#ifdef	CONFIG_COMX_DEBUG_RAW
	char		*raw;
	int		raw_len;
#endif
	// LINE specific	
	struct comx_protocol *protocol;
	void		(*LINE_rx)(struct net_device *dev, struct sk_buff *skb);
	int		(*LINE_tx)(struct net_device *dev);
	void		(*LINE_status)(struct net_device *dev, u_short status);
	int		(*LINE_open)(struct net_device *dev);
	int		(*LINE_close)(struct net_device *dev);
	int		(*LINE_xmit)(struct sk_buff *skb, struct net_device *dev);
	int		(*LINE_header)(struct sk_buff *skb, struct net_device *dev,
				u_short type,void *daddr, void *saddr, 
				unsigned len);
	int		(*LINE_rebuild_header)(struct sk_buff *skb);
	int		(*LINE_statistics)(struct net_device *dev, char *page);
	int		(*LINE_parameter_check)(struct net_device *dev);
	int		(*LINE_ioctl)(struct net_device *dev, struct ifreq *ifr,
				int cmd);
	void		(*LINE_mod_use)(int);
	void *		LINE_privdata;

	// HW specific

	struct comx_hardware *hardware;
	void	(*HW_board_on)(struct net_device *dev);
	void	(*HW_board_off)(struct net_device *dev);
	struct net_device *(*HW_access_board)(struct net_device *dev);
	void	(*HW_release_board)(struct net_device *dev, struct net_device *savep);
	int	(*HW_txe)(struct net_device *dev);
	int	(*HW_open)(struct net_device *dev);
	int	(*HW_close)(struct net_device *dev);
	int	(*HW_send_packet)(struct net_device *dev,struct sk_buff *skb);
	int	(*HW_statistics)(struct net_device *dev, char *page);
#if 0
	int	(*HW_reset)(struct net_device *dev, char *page);
#endif
	int	(*HW_load_board)(struct net_device *dev);
	void	(*HW_set_clock)(struct net_device *dev);
	void	*HW_privdata;
	};

struct comx_debugflags_struct {
	char *name;
	int  value;
	};

#define	COMX_ROOT_DIR_NAME	"comx"

#define	FILENAME_HARDWARE	"boardtype"
#define FILENAME_HARDWARELIST	"boardtypes"
#define FILENAME_PROTOCOL	"protocol"
#define FILENAME_PROTOCOLLIST	"protocols"
#define FILENAME_DEBUG		"debug"
#define FILENAME_CLOCK		"clock"
#define	FILENAME_STATUS		"status"
#define	FILENAME_IO		"io"
#define FILENAME_IRQ		"irq"
#define	FILENAME_KEEPALIVE	"keepalive"
#define FILENAME_LINEUPDELAY	"lineup_delay"
#define FILENAME_CHANNEL	"channel"
#define FILENAME_FIRMWARE	"firmware"
#define FILENAME_MEMADDR	"memaddr"
#define	FILENAME_TWIN		"twin"
#define FILENAME_T1		"t1"
#define FILENAME_T2		"t2"
#define FILENAME_N2		"n2"
#define FILENAME_WINDOW		"window"
#define FILENAME_MODE		"mode"
#define	FILENAME_DLCI		"dlci"
#define	FILENAME_MASTER		"master"
#ifdef	CONFIG_COMX_DEBUG_RAW
#define	FILENAME_RAW		"raw"
#endif

#define PROTONAME_NONE		"none"
#define HWNAME_NONE		"none"
#define KEEPALIVE_OFF		"off"

#define FRAME_ACCEPTED		0		/* sending and xmitter busy */
#define FRAME_DROPPED		1
#define FRAME_ERROR		2		/* xmitter error */
#define	FRAME_QUEUED		3		/* sending but more can come */

#define	LINE_UP			1		/* Modem UP */
#define PROTO_UP		2
#define PROTO_LOOP		4

#define	HW_OPEN			1
#define	LINE_OPEN		2
#define FW_LOADED		4
#define IRQ_ALLOCATED		8

#define DEBUG_COMX_RX		2
#define	DEBUG_COMX_TX		4
#define	DEBUG_HW_TX		16
#define	DEBUG_HW_RX		32
#define	DEBUG_HDLC_KEEPALIVE	64
#define	DEBUG_COMX_PPP		128
#define DEBUG_COMX_LAPB		256
#define	DEBUG_COMX_DLCI		512

#define	DEBUG_PAGESIZE		3072
#define DEFAULT_DEBUG_SIZE	4096
#define	DEFAULT_LINEUP_DELAY	1
#define	FILE_PAGESIZE		3072

#ifndef	COMX_PPP_MAJOR
#define	COMX_PPP_MAJOR		88
#endif


#define COMX_CHANNEL(dev) ((struct comx_channel*)dev->priv)

#define TWIN(dev) (COMX_CHANNEL(dev)->twin)


#ifndef byte
typedef u8	byte;
#endif
#ifndef word
typedef u16	word;
#endif

#ifndef	SEEK_SET
#define	SEEK_SET	0
#endif
#ifndef	SEEK_CUR
#define	SEEK_CUR	1
#endif
#ifndef	SEEK_END
#define	SEEK_END	2
#endif

extern struct proc_dir_entry * comx_root_dir;

extern int	comx_register_hardware(struct comx_hardware *comx_hw);
extern int	comx_unregister_hardware(char *name);
extern int	comx_register_protocol(struct comx_protocol *comx_line);
extern int	comx_unregister_protocol(char *name);

extern int	comx_rx(struct net_device *dev, struct sk_buff *skb);
extern void	comx_status(struct net_device *dev, int status);
extern void	comx_lineup_func(unsigned long d);

extern int	comx_debug(struct net_device *dev, char *fmt, ...);
extern int	comx_debug_skb(struct net_device *dev, struct sk_buff *skb, char *msg);
extern int	comx_debug_bytes(struct net_device *dev, unsigned char *bytes, int len,
		char *msg);
extern int	comx_strcasecmp(const char *cs, const char *ct);

extern struct inode_operations comx_normal_inode_ops;
