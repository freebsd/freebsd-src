
/* ATAPI tape commands not in std ATAPI command set */
#define ATAPI_TAPE_REWIND   		0x01
#define ATAPI_TAPE_REQUEST_SENSE   	0x03
#define ATAPI_TAPE_READ_CMD   		0x08
#define ATAPI_TAPE_WRITE_CMD   		0x0a
#define ATAPI_TAPE_WEOF 		0x10
#define     WEOF_WRITE_MASK    			0x01
#define ATAPI_TAPE_SPACE_CMD 		0x11
#define     SP_FM				0x01
#define     SP_EOD				0x03
#define ATAPI_TAPE_ERASE    		0x19
#define ATAPI_TAPE_MODE_SENSE   	0x1a
#define ATAPI_TAPE_LOAD_UNLOAD  	0x1b
#define     LU_LOAD_MASK       			0x01
#define     LU_RETENSION_MASK 			0x02
#define     LU_EOT_MASK     			0x04

#define DSC_POLL_INTERVAL	10

/* 
 * MODE SENSE parameter header
 */
struct ast_header {
    u_int8_t  data_length;        	/* Total length of data */
    u_int8_t  medium_type;       	/* Medium type (if any) */
    u_int8_t  dsp;            		/* Device specific parameter */
    u_int8_t  bdl;            		/* Block Descriptor Length */
};

/*
 * ATAPI tape drive Capabilities and Mechanical Status Page
 */
#define ATAPI_TAPE_CAP_PAGE     0x2a

struct ast_cappage {
    u_int8_t	page_code	:6;	/* Page code == 0x2a */
    u_int8_t	reserved1_67	:2;
    u_int8_t	page_length;        	/* Page Length == 0x12 */
    u_int8_t	reserved2;
    u_int8_t    reserved3;
    u_int8_t	readonly	:1;	/* Read Only Mode */
    u_int8_t	reserved4_1234	:4;
    u_int8_t	reverse		:1;	/* Supports reverse direction */
    u_int8_t	reserved4_67	:2;
    u_int8_t	reserved5_012	:3;
    u_int8_t	eformat		:1;	/* Supports ERASE formatting */
    u_int8_t	reserved5_4	:1;
    u_int8_t	qfa     	:1;	/* Supports QFA formats */
    u_int8_t	reserved5_67   	:2;
    u_int8_t	lock        	:1;	/* Supports locking media */
    u_int8_t	locked      	:1;	/* The media is locked */
    u_int8_t	prevent     	:1;	/* Defaults  to prevent state */
    u_int8_t	eject       	:1;	/* Supports eject */
    u_int8_t	disconnect	:1;	/* Can break request > ctl */
    u_int8_t	reserved6_5    	:1;
    u_int8_t	ecc     	:1;	/* Supports error correction */
    u_int8_t	compress    	:1;	/* Supports data compression */
    u_int8_t	reserved7_0 	:1;
    u_int8_t	blk512      	:1;	/* Supports 512b block size */
    u_int8_t	blk1024     	:1;	/* Supports 1024b block size */
    u_int8_t	reserved7_3456 	:4;
    u_int8_t	slowb       	:1;	/* Restricts byte count */
    u_int16_t	max_speed;     		/* Supported speed in KBps */
    u_int16_t	max_defects;   		/* Max stored defect entries */
    u_int16_t	ctl;           		/* Continuous Transfer Limit */
    u_int16_t	speed;         		/* Current Speed, in KBps */
    u_int16_t	buffer_size;        	/* Buffer Size, in 512 bytes */
    u_int8_t	reserved18;
    u_int8_t	reserved19;
};

/*
 * REQUEST SENSE structure
 */
struct ast_reqsense {
    u_int8_t	error_code     	:7;	/* Current or deferred errors */
    u_int8_t	valid          	:1;	/* Follows QIC-157C */
    u_int8_t	reserved1;			/* Segment Number - Reserved */
    u_int8_t	sense_key	:4;	/* Sense Key */
    u_int8_t	reserved2_4	:1;	/* Reserved */
    u_int8_t	ili		:1;	/* Incorrect Length Indicator */
    u_int8_t	eom		:1;	/* End Of Medium */
    u_int8_t	filemark	:1;	/* Filemark */
    u_int8_t	info __attribute__((packed)); /* Cmd specific info */
    u_int8_t	asl;			/* Additional sense length (n-7) */
    u_int8_t	command_specific;	/* Additional cmd specific info */
    u_int8_t	asc;			/* Additional Sense Code */
    u_int8_t	ascq;			/* Additional Sense Code Qualifier */
    u_int8_t	replaceable_unit_code;	/* Field Replaceable Unit Code */
    u_int8_t	sk_specific1	:7;	/* Sense Key Specific */
    u_int8_t	sksv		:1;	/* Sense Key Specific info valid */
    u_int8_t	sk_specific2;		/* Sense Key Specific */
    u_int8_t	sk_specific3;		/* Sense Key Specific */
    u_int8_t	pad[2];			/* Padding */
};

struct ast_softc {
    struct atapi_softc 		*atp;          	/* Controller structure */
    int32_t lun;                		/* Logical device unit */
    int32_t flags;              		/* Device state flags */
    int32_t blksize;                		/* Block size (512 | 1024) */
    struct buf_queue_head	buf_queue;    	/* Queue of i/o requests */
    struct atapi_params		*param;     	/* Drive parameters table */
    struct ast_header		header;       	/* MODE SENSE param header */
    struct ast_cappage 		cap;         	/* Capabilities page info */
    struct devstat		stats;		/* devstat entry */
#ifdef  DEVFS
    void    *cdevs;
    void    *bdevs;
#endif
};
