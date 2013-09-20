/*-
 * Copyright (c) 2003 Silicon Graphics International Corp.
 * Copyright (c) 2011 Spectra Logic Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    substantially similar to the "NO WARRANTY" disclaimer below
 *    ("Disclaimer") and any redistribution must be conditioned upon
 *    including a substantially similar Disclaimer requirement for further
 *    binary redistribution.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGES.
 *
 * $Id: //depot/users/kenm/FreeBSD-test2/sys/cam/ctl/ctl_ioctl.h#4 $
 * $FreeBSD$
 */
/*
 * CAM Target Layer ioctl interface.
 *
 * Author: Ken Merry <ken@FreeBSD.org>
 */

#ifndef	_CTL_IOCTL_H_
#define	_CTL_IOCTL_H_

#ifdef ICL_KERNEL_PROXY
#include <sys/socket.h>
#endif

#include <sys/ioccom.h>

#define	CTL_DEFAULT_DEV		"/dev/cam/ctl"
/*
 * Maximum number of targets we support.
 */
#define	CTL_MAX_TARGETS		1

/*
 * Maximum target ID we support.
 */
#define	CTL_MAX_TARGID		15

/*
 * Maximum number of LUNs we support at the moment.  MUST be a power of 2.
 */
#define	CTL_MAX_LUNS		256

/*
 * Maximum number of initiators per port.
 */
#define	CTL_MAX_INIT_PER_PORT	2048 // Was 16

/*
 * Maximum number of ports registered at one time.
 */
#define	CTL_MAX_PORTS		32

/*
 * Maximum number of initiators we support.
 */
#define	CTL_MAX_INITIATORS	(CTL_MAX_INIT_PER_PORT * CTL_MAX_PORTS)

/* Hopefully this won't conflict with new misc devices that pop up */
#define	CTL_MINOR	225

typedef enum {
	CTL_OOA_INVALID_LUN,
	CTL_OOA_SUCCESS
} ctl_ooa_status;

struct ctl_ooa_info {
	uint32_t target_id;	/* Passed in to CTL */
	uint32_t lun_id;	/* Passed in to CTL */
	uint32_t num_entries;	/* Returned from CTL */
	ctl_ooa_status status;	/* Returned from CTL */
};

struct ctl_hard_startstop_info {
	cfi_mt_status status;
	int total_luns;
	int luns_complete;
	int luns_failed;
};

struct ctl_bbrread_info {
	int			lun_num;	/* Passed in to CTL */
	uint64_t		lba;		/* Passed in to CTL */
	int			len;		/* Passed in to CTL */
	cfi_mt_status		status;		/* Returned from CTL */
	cfi_bbrread_status	bbr_status;	/* Returned from CTL */
	uint8_t			scsi_status;	/* Returned from CTL */
	struct scsi_sense_data	sense_data;	/* Returned from CTL */
};

typedef enum {
	CTL_DELAY_TYPE_NONE,
	CTL_DELAY_TYPE_CONT,
	CTL_DELAY_TYPE_ONESHOT
} ctl_delay_type;

typedef enum {
	CTL_DELAY_LOC_NONE,
	CTL_DELAY_LOC_DATAMOVE,
	CTL_DELAY_LOC_DONE,
} ctl_delay_location;

typedef enum {
	CTL_DELAY_STATUS_NONE,
	CTL_DELAY_STATUS_OK,
	CTL_DELAY_STATUS_INVALID_LUN,
	CTL_DELAY_STATUS_INVALID_TYPE,
	CTL_DELAY_STATUS_INVALID_LOC,
	CTL_DELAY_STATUS_NOT_IMPLEMENTED
} ctl_delay_status;

struct ctl_io_delay_info {
	uint32_t		target_id;
	uint32_t		lun_id;
	ctl_delay_type		delay_type;
	ctl_delay_location	delay_loc;
	uint32_t		delay_secs;
	ctl_delay_status	status;
};

typedef enum {
	CTL_GS_SYNC_NONE,
	CTL_GS_SYNC_OK,
	CTL_GS_SYNC_NO_LUN
} ctl_gs_sync_status;

/*
 * The target and LUN id specify which device to modify.  The sync interval
 * means that we will let through every N SYNCHRONIZE CACHE commands.
 */
struct ctl_sync_info {
	uint32_t		target_id;	/* passed to kernel */
	uint32_t		lun_id;		/* passed to kernel */
	int			sync_interval;	/* depends on whether get/set */
	ctl_gs_sync_status	status;		/* passed from kernel */
};

typedef enum {
	CTL_STATS_NO_IO,
	CTL_STATS_READ,
	CTL_STATS_WRITE
} ctl_stat_types;
#define	CTL_STATS_NUM_TYPES	3

typedef enum {
	CTL_LUN_STATS_NO_BLOCKSIZE	= 0x01
} ctl_lun_stats_flags;

struct ctl_lun_io_port_stats {
	uint32_t			targ_port;
	uint64_t			bytes[CTL_STATS_NUM_TYPES];
	uint64_t			operations[CTL_STATS_NUM_TYPES];
	struct bintime			time[CTL_STATS_NUM_TYPES];
	uint64_t			num_dmas[CTL_STATS_NUM_TYPES];
	struct bintime			dma_time[CTL_STATS_NUM_TYPES];
};

struct ctl_lun_io_stats {
	uint8_t				device_type;
	uint64_t			lun_number;
	uint32_t			blocksize;
	ctl_lun_stats_flags		flags;
	struct ctl_lun_io_port_stats	ports[CTL_MAX_PORTS];
};

typedef enum {
	CTL_SS_OK,
	CTL_SS_NEED_MORE_SPACE,
	CTL_SS_ERROR
} ctl_stats_status;

typedef enum {
	CTL_STATS_FLAG_NONE		= 0x00,
	CTL_STATS_FLAG_TIME_VALID	= 0x01
} ctl_stats_flags;

struct ctl_stats {
	int			alloc_len;	/* passed to kernel */
	struct ctl_lun_io_stats	*lun_stats;	/* passed to/from kernel */
	int			fill_len;	/* passed to userland */
	int			num_luns;	/* passed to userland */
	ctl_stats_status	status;		/* passed to userland */
	ctl_stats_flags		flags;		/* passed to userland */
	struct timespec		timestamp;	/* passed to userland */
};

/*
 * The types of errors that can be injected:
 *
 * NONE:	No error specified.
 * ABORTED:	SSD_KEY_ABORTED_COMMAND, 0x45, 0x00
 * MEDIUM_ERR:	Medium error, different asc/ascq depending on read/write.
 * UA:		Unit attention.
 * CUSTOM:	User specifies the sense data.
 * TYPE:	Mask to use with error types.
 *
 * Flags that affect injection behavior:
 * CONTINUOUS:	This error will stay around until explicitly cleared.
 * DESCRIPTOR:	Use descriptor sense instead of fixed sense.
 */
typedef enum {
	CTL_LUN_INJ_NONE		= 0x000,
	CTL_LUN_INJ_ABORTED		= 0x001,
	CTL_LUN_INJ_MEDIUM_ERR		= 0x002,
	CTL_LUN_INJ_UA			= 0x003,
	CTL_LUN_INJ_CUSTOM		= 0x004,
	CTL_LUN_INJ_TYPE		= 0x0ff,
	CTL_LUN_INJ_CONTINUOUS		= 0x100,
	CTL_LUN_INJ_DESCRIPTOR		= 0x200
} ctl_lun_error;

/*
 * Flags to specify what type of command the given error pattern will
 * execute on.  The first group of types can be ORed together.
 *
 * READ:	Any read command.
 * WRITE:	Any write command.
 * READWRITE:	Any read or write command.
 * READCAP:	Any read capacity command.
 * TUR:		Test Unit Ready.
 * ANY:		Any command.
 * MASK:	Mask for basic command patterns.
 *
 * Special types:
 *
 * CMD:		The CDB to act on is specified in struct ctl_error_desc_cmd.
 * RANGE:	For read/write commands, act when the LBA is in the
 *		specified range.
 */
typedef enum {
	CTL_LUN_PAT_NONE	= 0x000,
	CTL_LUN_PAT_READ	= 0x001,
	CTL_LUN_PAT_WRITE	= 0x002,
	CTL_LUN_PAT_READWRITE	= CTL_LUN_PAT_READ | CTL_LUN_PAT_WRITE,
	CTL_LUN_PAT_READCAP	= 0x004,
	CTL_LUN_PAT_TUR		= 0x008,
	CTL_LUN_PAT_ANY		= 0x0ff,
	CTL_LUN_PAT_MASK	= 0x0ff,
	CTL_LUN_PAT_CMD		= 0x100,
	CTL_LUN_PAT_RANGE	= 0x200
} ctl_lun_error_pattern;

/*
 * This structure allows the user to specify a particular CDB pattern to
 * look for.
 *
 * cdb_pattern:		Fill in the relevant bytes to look for in the CDB.
 * cdb_valid_bytes:	Bitmask specifying valid bytes in the cdb_pattern.
 * flags:		Specify any command flags (see ctl_io_flags) that
 *			should be set.
 */
struct ctl_error_desc_cmd {
	uint8_t		cdb_pattern[CTL_MAX_CDBLEN];
	uint32_t	cdb_valid_bytes;
	uint32_t	flags;
};

/*
 * Error injection descriptor.
 *
 * target_id:	   Target ID to act on.
 * lun_id	   LUN to act on.
 * lun_error:	   The type of error to inject.  See above for descriptions.
 * error_pattern:  What kind of command to act on.  See above.
 * cmd_desc:	   For CTL_LUN_PAT_CMD only.
 * lba_range:	   For CTL_LUN_PAT_RANGE only.
 * custom_sense:   Specify sense.  For CTL_LUN_INJ_CUSTOM only.
 * serial:	   Serial number returned by the kernel.  Use for deletion.
 * links:	   Kernel use only.
 */
struct ctl_error_desc {
	uint32_t			target_id;	/* To kernel */
	uint32_t			lun_id;		/* To kernel */
	ctl_lun_error			lun_error;	/* To kernel */
	ctl_lun_error_pattern		error_pattern;	/* To kernel */
	struct ctl_error_desc_cmd	cmd_desc;	/* To kernel */
	struct ctl_lba_len		lba_range;	/* To kernel */
	struct scsi_sense_data		custom_sense;	/* To kernel */
	uint64_t			serial;		/* From kernel */
	STAILQ_ENTRY(ctl_error_desc)	links;		/* Kernel use only */
};

typedef enum {
	CTL_OOA_FLAG_NONE	= 0x00,
	CTL_OOA_FLAG_ALL_LUNS	= 0x01
} ctl_ooa_flags;

typedef enum {
	CTL_OOA_OK,
	CTL_OOA_NEED_MORE_SPACE,
	CTL_OOA_ERROR
} ctl_get_ooa_status;

typedef enum {
	CTL_OOACMD_FLAG_NONE		= 0x00,
	CTL_OOACMD_FLAG_DMA		= 0x01,
	CTL_OOACMD_FLAG_BLOCKED		= 0x02,
	CTL_OOACMD_FLAG_ABORT		= 0x04,
	CTL_OOACMD_FLAG_RTR		= 0x08,
	CTL_OOACMD_FLAG_DMA_QUEUED	= 0x10
} ctl_ooa_cmd_flags;

struct ctl_ooa_entry {
	ctl_ooa_cmd_flags	cmd_flags;
	uint8_t			cdb[CTL_MAX_CDBLEN];
	uint8_t			cdb_len;
	uint32_t		tag_num;
	uint32_t		lun_num;
	struct bintime		start_bt;
};

struct ctl_ooa {
	ctl_ooa_flags		flags;		/* passed to kernel */
	uint64_t		lun_num;	/* passed to kernel */
	uint32_t		alloc_len;	/* passed to kernel */
	uint32_t		alloc_num;	/* passed to kernel */
	struct ctl_ooa_entry	*entries;	/* filled in kernel */
	uint32_t		fill_len;	/* passed to userland */
	uint32_t		fill_num;	/* passed to userland */
	uint32_t		dropped_num;	/* passed to userland */
	struct bintime		cur_bt;		/* passed to userland */
	ctl_get_ooa_status	status;		/* passed to userland */
};

typedef enum {
	CTL_PORT_LIST_NONE,
	CTL_PORT_LIST_OK,
	CTL_PORT_LIST_NEED_MORE_SPACE,
	CTL_PORT_LIST_ERROR
} ctl_port_list_status;

struct ctl_port_list {
	uint32_t		alloc_len;	/* passed to kernel */
	uint32_t		alloc_num;	/* passed to kernel */
	struct ctl_port_entry   *entries;	/* filled in kernel */
	uint32_t		fill_len;	/* passed to userland */
	uint32_t		fill_num;	/* passed to userland */
	uint32_t		dropped_num;	/* passed to userland */
	ctl_port_list_status	status;		/* passed to userland */
};

typedef enum {
	CTL_LUN_NOSTATUS,
	CTL_LUN_OK,
	CTL_LUN_ERROR
} ctl_lun_status;

#define	CTL_ERROR_STR_LEN	160

#define	CTL_BEARG_RD		0x01
#define	CTL_BEARG_WR		0x02
#define	CTL_BEARG_RW		(CTL_BEARG_RD|CTL_BEARG_WR)
#define	CTL_BEARG_ASCII		0x04

/*
 * Backend Argument:
 *
 * namelen:	Length of the name field, including the terminating NUL.
 *
 * name:	Name of the paramter.  This must be NUL-terminated.
 *
 * flags:	Flags for the parameter, see above for values.
 *
 * vallen:	Length of the value in bytes.
 *
 * value:	Value to be set/fetched.
 *
 * kname:	For kernel use only.
 *
 * kvalue:	For kernel use only.
 */
struct ctl_be_arg {
	int	namelen;
	char	*name;
	int	flags;
	int	vallen;
	void	*value;

	char	*kname;
	void	*kvalue;
};

typedef enum {
	CTL_LUNREQ_CREATE,
	CTL_LUNREQ_RM,
	CTL_LUNREQ_MODIFY,
} ctl_lunreq_type;


/*
 * LUN creation parameters:
 *
 * flags:		Various LUN flags, see ctl_backend.h for a
 *			description of the flag values and meanings.
 *
 * device_type:		The SCSI device type.  e.g. 0 for Direct Access,
 *			3 for Processor, etc.  Only certain backends may
 *			support setting this field.  The CTL_LUN_FLAG_DEV_TYPE
 *			flag should be set in the flags field if the device
 *			type is set.
 *
 * lun_size_bytes:	The size of the LUN in bytes.  For some backends
 *			this is relevant (e.g. ramdisk), for others, it may
 *			be ignored in favor of using the properties of the
 *			backing store.  If specified, this should be a
 *			multiple of the blocksize.
 *
 *			The actual size of the LUN is returned in this
 *			field.
 *
 * blocksize_bytes:	The LUN blocksize in bytes.  For some backends this
 *			is relevant, for others it may be ignored in
 *			favor of using the properties of the backing store. 
 *
 *			The actual blocksize of the LUN is returned in this
 *			field.
 *
 * req_lun_id:		The requested LUN ID.  The CTL_LUN_FLAG_ID_REQ flag
 *			should be set if this is set.  The request will be
 *			granted if the LUN number is available, otherwise
 * 			the LUN addition request will fail.
 *
 *			The allocated LUN number is returned in this field.
 *
 * serial_num:		This is the value returned in SCSI INQUIRY VPD page
 *			0x80.  If it is specified, the CTL_LUN_FLAG_SERIAL_NUM
 *			flag should be set.
 *
 *			The serial number value used is returned in this
 *			field.
 *
 * device_id:		This is the value returned in the T10 vendor ID
 *			based DESIGNATOR field in the SCSI INQUIRY VPD page
 *			0x83 data.  If it is specified, the CTL_LUN_FLAG_DEVID
 *			flag should be set.
 *
 *			The device id value used is returned in this field.
 */
struct ctl_lun_create_params {
	ctl_backend_lun_flags	flags;
	uint8_t			device_type;
	uint64_t		lun_size_bytes;
	uint32_t		blocksize_bytes;
	uint32_t		req_lun_id;
	uint8_t			serial_num[CTL_SN_LEN];
	uint8_t			device_id[CTL_DEVID_LEN];
};

/*
 * LUN removal parameters:
 *
 * lun_id:		The number of the LUN to delete.  This must be set.
 *			The LUN must be backed by the given backend.
 */
struct ctl_lun_rm_params {
	uint32_t		lun_id;
};

/*
 * LUN modification parameters:
 *
 * lun_id:		The number of the LUN to modify.  This must be set.
 *			The LUN must be backed by the given backend.
 *
 * lun_size_bytes:	The size of the LUN in bytes.  If zero, update
 * 			the size using the backing file size, if possible.
 */
struct ctl_lun_modify_params {
	uint32_t		lun_id;
	uint64_t		lun_size_bytes;
};

/*
 * Union of request type data.  Fill in the appropriate union member for
 * the request type.
 */
union ctl_lunreq_data {
	struct ctl_lun_create_params	create;
	struct ctl_lun_rm_params	rm;
	struct ctl_lun_modify_params	modify;
};

/*
 * LUN request interface:
 *
 * backend:		This is required, and is NUL-terminated a string
 *			that is the name of the backend, like "ramdisk" or
 *			"block".
 *
 * reqtype:		The type of request, CTL_LUNREQ_CREATE to create a
 *			LUN, CTL_LUNREQ_RM to delete a LUN.
 *
 * reqdata:		Request type-specific information.  See the
 *			description of individual the union members above
 *			for more information.
 *
 * num_be_args:		This is the number of backend-specific arguments
 *			in the be_args array.
 *
 * be_args:		This is an array of backend-specific arguments.
 *			See above for a description of the fields in this
 *			structure.
 *
 * status:		Status of the LUN request.
 *
 * error_str:		If the status is CTL_LUN_ERROR, this will
 *			contain a string describing the error.
 *
 * kern_be_args:	For kernel use only.
 */
struct ctl_lun_req {
	char			backend[CTL_BE_NAME_LEN];
	ctl_lunreq_type		reqtype;
	union ctl_lunreq_data	reqdata;
	int			num_be_args;
	struct ctl_be_arg	*be_args;
	ctl_lun_status		status;
	char			error_str[CTL_ERROR_STR_LEN];
	struct ctl_be_arg	*kern_be_args;
};

/*
 * LUN list status:
 *
 * NONE:		No status.
 *
 * OK:			Request completed successfully.
 *
 * NEED_MORE_SPACE:	The allocated length of the entries field is too
 * 			small for the available data.
 *
 * ERROR:		An error occured, look at the error string for a
 *			description of the error.
 */
typedef enum {
	CTL_LUN_LIST_NONE,
	CTL_LUN_LIST_OK,
	CTL_LUN_LIST_NEED_MORE_SPACE,
	CTL_LUN_LIST_ERROR
} ctl_lun_list_status;

/*
 * LUN list interface
 *
 * backend_name:	This is a NUL-terminated string.  If the string
 *			length is 0, then all LUNs on all backends will
 *			be enumerated.  Otherwise this is the name of the
 *			backend to be enumerated, like "ramdisk" or "block".
 *
 * alloc_len:		The length of the data buffer allocated for entries.
 *			In order to properly size the buffer, make one call
 *			with alloc_len set to 0, and then use the returned
 *			dropped_len as the buffer length to allocate and
 *			pass in on a subsequent call.
 *
 * lun_xml:		XML-formatted information on the requested LUNs.
 *
 * fill_len:		The amount of data filled in the storage for entries.
 *
 * status:		The status of the request.  See above for the 
 *			description of the values of this field.
 *
 * error_str:		If the status indicates an error, this string will
 *			be filled in to describe the error.
 */
struct ctl_lun_list {
	char			backend[CTL_BE_NAME_LEN]; /* passed to kernel*/
	uint32_t		alloc_len;	/* passed to kernel */
	char                   *lun_xml;	/* filled in kernel */
	uint32_t		fill_len;	/* passed to userland */
	ctl_lun_list_status	status;		/* passed to userland */
	char			error_str[CTL_ERROR_STR_LEN];
						/* passed to userland */
};

/*
 * iSCSI status
 *
 * OK:			Request completed successfully.
 *
 * ERROR:		An error occured, look at the error string for a
 *			description of the error.
 *
 * CTL_ISCSI_LIST_NEED_MORE_SPACE:
 * 			User has to pass larger buffer for CTL_ISCSI_LIST ioctl.
 */
typedef enum {
	CTL_ISCSI_OK,
	CTL_ISCSI_ERROR,
	CTL_ISCSI_LIST_NEED_MORE_SPACE,
	CTL_ISCSI_SESSION_NOT_FOUND
} ctl_iscsi_status;

typedef enum {
	CTL_ISCSI_HANDOFF,
	CTL_ISCSI_LIST,
	CTL_ISCSI_LOGOUT,
	CTL_ISCSI_TERMINATE,
#ifdef ICL_KERNEL_PROXY
	CTL_ISCSI_LISTEN,
	CTL_ISCSI_ACCEPT,
	CTL_ISCSI_SEND,
	CTL_ISCSI_RECEIVE,
	CTL_ISCSI_CLOSE,
#endif
} ctl_iscsi_type;

typedef enum {
	CTL_ISCSI_DIGEST_NONE,
	CTL_ISCSI_DIGEST_CRC32C
} ctl_iscsi_digest;

#define	CTL_ISCSI_NAME_LEN	224	/* 223 bytes, by RFC 3720, + '\0' */
#define	CTL_ISCSI_ADDR_LEN	47	/* INET6_ADDRSTRLEN + '\0' */
#define	CTL_ISCSI_ALIAS_LEN	128	/* Arbitrary. */

struct ctl_iscsi_handoff_params {
	char			initiator_name[CTL_ISCSI_NAME_LEN];
	char			initiator_addr[CTL_ISCSI_ADDR_LEN];
	char			initiator_alias[CTL_ISCSI_ALIAS_LEN];
	char			target_name[CTL_ISCSI_NAME_LEN];
#ifdef ICL_KERNEL_PROXY
	int			connection_id;
	/*
	 * XXX
	 */
	int			socket;
#else
	int			socket;
#endif
	int			portal_group_tag;
	
	/*
	 * Connection parameters negotiated by ctld(8).
	 */
	ctl_iscsi_digest	header_digest;
	ctl_iscsi_digest	data_digest;
	uint32_t		cmdsn;
	uint32_t		statsn;
	uint32_t		max_recv_data_segment_length;
	uint32_t		max_burst_length;
	uint32_t		first_burst_length;
	uint32_t		immediate_data;
	int			spare[4];
};

struct ctl_iscsi_list_params {
	uint32_t		alloc_len;	/* passed to kernel */
	char                   *conn_xml;	/* filled in kernel */
	uint32_t		fill_len;	/* passed to userland */
	int			spare[4];
};

struct ctl_iscsi_logout_params {
	int			connection_id;	/* passed to kernel */
	char			initiator_name[CTL_ISCSI_NAME_LEN];
						/* passed to kernel */
	char			initiator_addr[CTL_ISCSI_ADDR_LEN];
						/* passed to kernel */
	int			all;		/* passed to kernel */
	int			spare[4];
};

struct ctl_iscsi_terminate_params {
	int			connection_id;	/* passed to kernel */
	char			initiator_name[CTL_ISCSI_NAME_LEN];
						/* passed to kernel */
	char			initiator_addr[CTL_ISCSI_NAME_LEN];
						/* passed to kernel */
	int			all;		/* passed to kernel */
	int			spare[4];
};

#ifdef ICL_KERNEL_PROXY
struct ctl_iscsi_listen_params {
	int				iser;
	int				domain;
	int				socktype;
	int				protocol;
	struct sockaddr			*addr;
	socklen_t			addrlen;
	int				spare[4];
};

struct ctl_iscsi_accept_params {
	int				connection_id;
	int				spare[4];
};

struct ctl_iscsi_send_params {
	int				connection_id;
	void				*bhs;
	size_t				spare;
	void				*spare2;
	size_t				data_segment_len;
	void				*data_segment;
	int				spare[4];
};

struct ctl_iscsi_receive_params {
	int				connection_id;
	void				*bhs;
	size_t				spare;
	void				*spare2;
	size_t				data_segment_len;
	void				*data_segment;
	int				spare[4];
};

struct ctl_iscsi_close_params {
	int				connection_id;
	int				spare[4];
};
#endif /* ICL_KERNEL_PROXY */

union ctl_iscsi_data {
	struct ctl_iscsi_handoff_params		handoff;
	struct ctl_iscsi_list_params		list;
	struct ctl_iscsi_logout_params		logout;
	struct ctl_iscsi_terminate_params	terminate;
#ifdef ICL_KERNEL_PROXY
	struct ctl_iscsi_listen_params		listen;
	struct ctl_iscsi_accept_params		accept;
	struct ctl_iscsi_send_params		send;
	struct ctl_iscsi_receive_params		receive;
	struct ctl_iscsi_close_params		close;
#endif
};

/*
 * iSCSI interface
 *
 * status:		The status of the request.  See above for the 
 *			description of the values of this field.
 *
 * error_str:		If the status indicates an error, this string will
 *			be filled in to describe the error.
 */
struct ctl_iscsi {
	ctl_iscsi_type		type;		/* passed to kernel */
	union ctl_iscsi_data	data;		/* passed to kernel */
	ctl_iscsi_status	status;		/* passed to userland */
	char			error_str[CTL_ERROR_STR_LEN];
						/* passed to userland */
};

#define	CTL_IO			_IOWR(CTL_MINOR, 0x00, union ctl_io)
#define	CTL_ENABLE_PORT		_IOW(CTL_MINOR, 0x04, struct ctl_port_entry)
#define	CTL_DISABLE_PORT	_IOW(CTL_MINOR, 0x05, struct ctl_port_entry)
#define	CTL_DUMP_OOA		_IO(CTL_MINOR, 0x06)
#define	CTL_CHECK_OOA		_IOWR(CTL_MINOR, 0x07, struct ctl_ooa_info)
#define	CTL_HARD_STOP		_IOR(CTL_MINOR, 0x08, \
				     struct ctl_hard_startstop_info)
#define	CTL_HARD_START		_IOR(CTL_MINOR, 0x09, \
				     struct ctl_hard_startstop_info)
#define	CTL_DELAY_IO		_IOWR(CTL_MINOR, 0x10, struct ctl_io_delay_info)
#define	CTL_REALSYNC_GET	_IOR(CTL_MINOR, 0x11, int)
#define	CTL_REALSYNC_SET	_IOW(CTL_MINOR, 0x12, int)
#define	CTL_SETSYNC		_IOWR(CTL_MINOR, 0x13, struct ctl_sync_info)
#define	CTL_GETSYNC		_IOWR(CTL_MINOR, 0x14, struct ctl_sync_info)
#define	CTL_GETSTATS		_IOWR(CTL_MINOR, 0x15, struct ctl_stats)
#define	CTL_ERROR_INJECT	_IOWR(CTL_MINOR, 0x16, struct ctl_error_desc)
#define	CTL_BBRREAD		_IOWR(CTL_MINOR, 0x17, struct ctl_bbrread_info)
#define	CTL_GET_OOA		_IOWR(CTL_MINOR, 0x18, struct ctl_ooa)
#define	CTL_DUMP_STRUCTS	_IO(CTL_MINOR, 0x19)
#define	CTL_GET_PORT_LIST	_IOWR(CTL_MINOR, 0x20, struct ctl_port_list)
#define	CTL_LUN_REQ		_IOWR(CTL_MINOR, 0x21, struct ctl_lun_req)
#define	CTL_LUN_LIST		_IOWR(CTL_MINOR, 0x22, struct ctl_lun_list)
#define	CTL_ERROR_INJECT_DELETE	_IOW(CTL_MINOR, 0x23, struct ctl_error_desc)
#define	CTL_SET_PORT_WWNS	_IOW(CTL_MINOR, 0x24, struct ctl_port_entry)
#define	CTL_ISCSI		_IOWR(CTL_MINOR, 0x25, struct ctl_iscsi)

#endif /* _CTL_IOCTL_H_ */

/*
 * vim: ts=8
 */
