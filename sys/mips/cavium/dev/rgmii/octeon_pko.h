/*------------------------------------------------------------------
 * octeon_pko.h      Packet Output Block
 *
 *------------------------------------------------------------------
 */


#ifndef ___OCTEON_PKO__H___
#define ___OCTEON_PKO__H___



/*
 * PKO Command Buffer Register.
 * Specify Pool-# and Size of each entry in Pool. For Output Cmd Buffers.
 */
typedef union {
    uint64_t word64;
    struct {
        uint64_t unused_mbz		: 41;   /* Must be zero */
        uint64_t pool                   : 3;    /* FPA Pool to use */
        uint64_t unused_mbz2              : 7;    /* Must be zero */
        uint64_t size                   : 13;   /* Size of the pool blocks */
    } bits;
} octeon_pko_pool_cfg_t;


/*
 * PKO GMX Mode Register
 * Specify the # of GMX1 ports and GMX0 ports
 */
typedef union {
    uint64_t word64;
    struct {
        uint64_t unused_mbz	: 58;      /* MBZ */
        uint64_t mode1          : 3;       /* # GMX1 ports; */
					   /*  16 >> MODE1, 0 <= MODE1 <=4 */
        uint64_t mode0          : 3;       /* # GMX0 ports; */
					   /*  16 >> MODE0, 0 <= MODE0 <=4 */
    } bits;
} octeon_pko_reg_gmx_port_mode_t;


typedef union {
    uint64_t word64;
    struct {
        uint64_t unused_mbz	: 62;      /* MBZ */
        uint64_t mode           : 2;       /* Queues Mode */
    } bits;
} octeon_pko_queue_mode_t;


typedef union {
    uint64_t word64;
    struct {
        uint64_t unused_mbz	: 32;      /* MBZ */
        uint64_t crc_ports_mask : 32;      /* CRC Ports Enable mask */
    } bits;
} octeon_pko_crc_ports_enable_t;



#define OCTEON_PKO_QUEUES_MAX	128
#define OCTEON_PKO_PORTS_MAX	36
#define OCTEON_PKO_PORT_ILLEGAL	63

/* Defines how the PKO command buffer FAU register is used */

#define OCTEON_PKO_INDEX_BITS     12
#define OCTEON_PKO_INDEX_MASK     ((1ull << OCTEON_PKO_INDEX_BITS) - 1)



typedef enum {
    OCTEON_PKO_SUCCESS,
    OCTEON_PKO_INVALID_PORT,
    OCTEON_PKO_INVALID_QUEUE,
    OCTEON_PKO_INVALID_PRIORITY,
    OCTEON_PKO_NO_MEMORY
} octeon_pko_status_t;


typedef struct {
    long	packets;
    uint64_t    octets;
    uint64_t	doorbell;
} octeon_pko_port_status_t;


typedef union {
    uint64_t                word64;
    struct {
	octeon_mips_space_t mem_space  : 2;    /* Octeon IO_SEG */
        uint64_t	unused_mbz    :13;    /* Must be zero */
        uint64_t        is_io       : 1;    /* Must be one */
        uint64_t        did         : 8;    /* device-ID on non-coherent bus*/
        uint64_t        unused_mbz2   : 4;    /* Must be zero */
        uint64_t        unused_mbz3   :18;    /* Must be zero */
        uint64_t        port	    : 6;    /* output port */
        uint64_t        queue       : 9;    /* output queue to send */
        uint64_t        unused_mbz4   : 3;    /* Must be zero */
   } bits;
} octeon_pko_doorbell_address_t;

/*
 * Structure of the first packet output command word.
 */
typedef union {
    uint64_t                word64;
    struct {
        octeon_fau_op_size_t  size1       : 2; /* The size of reg1 operation */
					/* - could be 8, 16, 32, or 64 bits */
        octeon_fau_op_size_t  size0       : 2; /* The size of the reg0 operation  */
					/* - could be 8, 16, 32, or 64 bits */
        uint64_t	subone1     : 1; /* Subtract 1, else sub pkt size */
        uint64_t	reg1        :11; /* The register, subtract will be */
					 /*       done if reg1 is non-zero */
        uint64_t	subone0     : 1; /* Subtract 1, else sub pkt size */
        uint64_t	reg0        :11; /* The register, subtract will be */
					 /*       done if reg0 is non-zero */
        uint64_t	unused      : 2; /* Must be zero */
        uint64_t	wqp         : 1; /* If rsp, then word3 contains a */
				         /*     ptr to a work queue entry */
        uint64_t	rsp         : 1; /* HW will  respond when done */
        uint64_t	gather      : 1; /* If set, the supplied pkt_ptr is */
					 /*    a ptr to a list of pkt_ptr's */
        uint64_t	ipoffp1     : 7; /* Off to IP hdr.  For HW checksum */
        uint64_t	ignore_i    : 1; /* Ignore  I bit in all pointers */
        uint64_t	dontfree    : 1; /* Don't free buffs containing pkt */
        uint64_t	segs        : 6; /* Number of segs. If gather set, */
					 /*        also gather list length */
        uint64_t	total_bytes :16; /* Includes L2, w/o trailing CRC */
    } bits;
} octeon_pko_command_word0_t;


typedef union {
    void*           ptr;
    uint64_t        word64;
    struct {
        uint64_t    i    : 1; /* Invert the "free" pick of the overall pkt. */
        		      /* For inbound pkts, HW always sets this to 0 */
        uint64_t    back : 4; /* Amount to back up to get to buffer start */
        		      /* in cache lines. This is mostly less than 1 */
			      /* complete cache line; so the value is zero */
        uint64_t    pool : 3; /* FPA pool that the buffer belongs to */
        uint64_t    size :16; /* segment size (bytes) pointed at by addr */
        uint64_t    addr :40; /* Ptr to 1st data byte. NOT buffer */
    } bits;
} octeon_pko_packet_ptr_t;


/*
 * Definition of the hardware structure used to configure an
 * output queue.
 */
typedef union {
    uint64_t word64;
    struct {
        uint64_t unused_mbz	: 3;   /* Must be zero */
        uint64_t qos_mask	: 8;   /* Control Mask priority */
				       /*      across 8 QOS levels */
        uint64_t buf_ptr	: 36;  /* Command buffer pointer, */
				       /*          8 byte-aligned */
        uint64_t tail		: 1;   /* Set if this queue is the tail */
				       /*       of the port queue array */
        uint64_t index		: 3;   /* Index (distance from head) in */
				       /*          the port queue array */
        uint64_t port		: 6;   /* Port ID for this queue  mapping */
        uint64_t queue		: 7;   /* Hardware queue number */
    } bits;
} octeon_pko_queue_cfg_t;


typedef union {
    uint64_t word64;
    struct {
        uint64_t unused_mbz	: 48;
        uint64_t inc		: 8;
        uint64_t idx		: 8;
    } bits;
} octeon_pko_read_idx_t;


typedef struct octeon_pko_sw_queue_info_t_
{
    uint64_t xmit_command_state;
    octeon_spinlock_t lock;
    uint32_t pad[29];
} octeon_pko_sw_queue_info_t;



#define OCTEON_DID_PKT		10ULL
#define OCTEON_DID_PKT_SEND	OCTEON_ADDR_FULL_DID(OCTEON_DID_PKT,2ULL)


/*
 * Ring the packet output doorbell. This tells the packet
 * output hardware that "len" command words have been added
 * to its pending list.  This command includes the required
 * SYNCW before the doorbell ring.
 *
 * @param port   Port the packet is for
 * @param queue  Queue the packet is for
 * @param len    Length of the command in 64 bit words
 */
extern void octeon_pko_doorbell_data(u_int port);

//#define CORE_0_ONLY 1

static inline void octeon_pko_ring_doorbell (u_int port, u_int queue,
                                             u_int len)
{
   octeon_pko_doorbell_address_t ptr;

   ptr.word64          = 0;
   ptr.bits.mem_space  = OCTEON_IO_SEG;
   ptr.bits.did        = OCTEON_DID_PKT_SEND;
   ptr.bits.is_io      = 1;
   ptr.bits.port       = port;
   ptr.bits.queue      = queue;
   OCTEON_SYNCWS;
   oct_write64(ptr.word64, len);
}



#define OCTEON_PKO_QUEUES_PER_PORT_INTERFACE0	1
#define OCTEON_PKO_QUEUES_PER_PORT_INTERFACE1	1
#define OCTEON_PKO_QUEUES_PER_PORT_PCI		1

/*
 * octeon_pko_get_base_queue
 *
 * For a given port number, return the base pko output queue
 * for the port.
 */
static inline u_int octeon_pko_get_base_queue (u_int port)
{
    if (port < 16) {
        return (port * OCTEON_PKO_QUEUES_PER_PORT_INTERFACE0);
    }
    if (port < 32) {
        return (16 * OCTEON_PKO_QUEUES_PER_PORT_INTERFACE0 +
                (port - 16) * OCTEON_PKO_QUEUES_PER_PORT_INTERFACE1);
    }
    return (16 * OCTEON_PKO_QUEUES_PER_PORT_INTERFACE0 +
            16 * OCTEON_PKO_QUEUES_PER_PORT_INTERFACE1 +
            (port - 32) * OCTEON_PKO_QUEUES_PER_PORT_PCI);
}


/*
 * For a given port number, return the number of pko output queues.
 *
 * @param port   Port number
 * @return Number of output queues
 */
static inline u_int octeon_pko_get_num_queues(u_int port)
{
    if (port < 16) {
        return (OCTEON_PKO_QUEUES_PER_PORT_INTERFACE0);
    } else if (port<32) {
        return (OCTEON_PKO_QUEUES_PER_PORT_INTERFACE1);
    }

    return (OCTEON_PKO_QUEUES_PER_PORT_PCI);
}



/*
 * Externs
 */
extern void octeon_pko_init(void);
extern void octeon_pko_enable(void);
extern void octeon_pko_disable(void);
extern void octeon_pko_show(u_int start_port, u_int end_port);
extern void octeon_pko_config(void);
extern void octeon_pko_config_cmdbuf_global_defaults(u_int cmdbuf_pool, u_int elem_size);
extern void octeon_pko_config_rgmx_ports(void);
extern void octeon_pko_get_port_status(u_int, u_int, octeon_pko_port_status_t *status);
extern octeon_pko_status_t octeon_pko_config_port(u_int port,
                                                  u_int base_queue,
                                                  u_int num_queues,
                                                  const u_int priority[],
                                                  u_int pko_output_cmdbuf_fpa_pool,
						  octeon_pko_sw_queue_info_t sw_queues[]);


#endif   /*  ___OCTEON_PKO__H___ */
