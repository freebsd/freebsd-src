/*-
 * Copyright 2009 Scott Long
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _CAM_CAM_XPT_INTERNAL_H
#define _CAM_CAM_XPT_INTERNAL_H 1

/* Forward Declarations */
struct cam_eb;
struct cam_et;
struct cam_ed;

typedef struct cam_ed * (*xpt_alloc_device_func)(struct cam_eb *bus,
					         struct cam_et *target,
					         lun_id_t lun_id);
typedef void (*xpt_release_device_func)(struct cam_ed *device);
typedef void (*xpt_action_func)(union ccb *start_ccb);
typedef void (*xpt_dev_async_func)(u_int32_t async_code,
				   struct cam_eb *bus,
				   struct cam_et *target,
				   struct cam_ed *device,
				   void *async_arg);
typedef void (*xpt_announce_periph_func)(struct cam_periph *periph);

struct xpt_xport {
	xpt_alloc_device_func	alloc_device;
	xpt_release_device_func	reldev;
	xpt_action_func		action;
	xpt_dev_async_func	async;
	xpt_announce_periph_func announce;
};

/*
 * Structure for queueing a device in a run queue.
 * There is one run queue for allocating new ccbs,
 * and another for sending ccbs to the controller.
 */
struct cam_ed_qinfo {
	cam_pinfo pinfo;
	struct	  cam_ed *device;
};

/*
 * The CAM EDT (Existing Device Table) contains the device information for
 * all devices for all busses in the system.  The table contains a
 * cam_ed structure for each device on the bus.
 */
struct cam_ed {
	TAILQ_ENTRY(cam_ed) links;
	struct	cam_ed_qinfo devq_entry;
	struct	cam_et	 *target;
	struct	cam_sim  *sim;
	lun_id_t	 lun_id;
	struct	camq drvq;		/*
					 * Queue of type drivers wanting to do
					 * work on this device.
					 */
	struct	cam_ccbq ccbq;		/* Queue of pending ccbs */
	struct	async_list asyncs;	/* Async callback info for this B/T/L */
	struct	periph_list periphs;	/* All attached devices */
	u_int	generation;		/* Generation number */
	void		 *quirk;	/* Oddities about this device */
	u_int		 maxtags;
	u_int		 mintags;
	cam_proto	 protocol;
	u_int		 protocol_version;
	cam_xport	 transport;
	u_int		 transport_version;
	struct		 scsi_inquiry_data inq_data;
	uint8_t		 *supported_vpds;
	uint8_t		 supported_vpds_len;
	uint32_t	 device_id_len;
	uint8_t		 *device_id;
	uint8_t		 physpath_len;
	uint8_t		 *physpath;	/* physical path string form */
	uint32_t	 rcap_len;
	uint8_t		 *rcap_buf;
	struct		 ata_params ident_data;
	u_int8_t	 inq_flags;	/*
					 * Current settings for inquiry flags.
					 * This allows us to override settings
					 * like disconnection and tagged
					 * queuing for a device.
					 */
	u_int8_t	 queue_flags;	/* Queue flags from the control page */
	u_int8_t	 serial_num_len;
	u_int8_t	*serial_num;
	u_int32_t	 flags;
#define CAM_DEV_UNCONFIGURED	 	0x01
#define CAM_DEV_REL_TIMEOUT_PENDING	0x02
#define CAM_DEV_REL_ON_COMPLETE		0x04
#define CAM_DEV_REL_ON_QUEUE_EMPTY	0x08
#define CAM_DEV_RESIZE_QUEUE_NEEDED	0x10
#define CAM_DEV_TAG_AFTER_COUNT		0x20
#define CAM_DEV_INQUIRY_DATA_VALID	0x40
#define	CAM_DEV_IN_DV			0x80
#define	CAM_DEV_DV_HIT_BOTTOM		0x100
#define CAM_DEV_IDENTIFY_DATA_VALID	0x200
	u_int32_t	 tag_delay_count;
#define	CAM_TAG_DELAY_COUNT		5
	u_int32_t	 tag_saved_openings;
	u_int32_t	 refcount;
	struct callout	 callout;
};

/*
 * Each target is represented by an ET (Existing Target).  These
 * entries are created when a target is successfully probed with an
 * identify, and removed when a device fails to respond after a number
 * of retries, or a bus rescan finds the device missing.
 */
struct cam_et {
	TAILQ_HEAD(, cam_ed) ed_entries;
	TAILQ_ENTRY(cam_et) links;
	struct	cam_eb	*bus;
	target_id_t	target_id;
	u_int32_t	refcount;
	u_int		generation;
	struct		timeval last_reset;
	u_int		rpl_size;
	struct scsi_report_luns_data *luns;
};

/*
 * Each bus is represented by an EB (Existing Bus).  These entries
 * are created by calls to xpt_bus_register and deleted by calls to
 * xpt_bus_deregister.
 */
struct cam_eb {
	TAILQ_HEAD(, cam_et) et_entries;
	TAILQ_ENTRY(cam_eb)  links;
	path_id_t	     path_id;
	struct cam_sim	     *sim;
	struct timeval	     last_reset;
	u_int32_t	     flags;
#define	CAM_EB_RUNQ_SCHEDULED	0x01
	u_int32_t	     refcount;
	u_int		     generation;
	device_t	     parent_dev;
	struct xpt_xport     *xport;
};

struct cam_path {
	struct cam_periph *periph;
	struct cam_eb	  *bus;
	struct cam_et	  *target;
	struct cam_ed	  *device;
};

struct xpt_xport *	scsi_get_xport(void);
struct xpt_xport *	ata_get_xport(void);

struct cam_ed *		xpt_alloc_device(struct cam_eb *bus,
					 struct cam_et *target,
					 lun_id_t lun_id);
void			xpt_acquire_device(struct cam_ed *device);
void			xpt_release_device(struct cam_ed *device);
int			xpt_schedule_dev(struct camq *queue, cam_pinfo *dev_pinfo,
					 u_int32_t new_priority);
u_int32_t		xpt_dev_ccbq_resize(struct cam_path *path, int newopenings);
void			xpt_start_tags(struct cam_path *path);
void			xpt_stop_tags(struct cam_path *path);

MALLOC_DECLARE(M_CAMXPT);

#endif
