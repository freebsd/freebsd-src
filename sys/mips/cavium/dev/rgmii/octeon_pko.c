/*------------------------------------------------------------------
 * octeon_pko.c      Packet Output Unit
 *
 *------------------------------------------------------------------
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <vm/vm.h>
#include <vm/pmap.h>

#include <mips/cavium/octeon_pcmap_regs.h>
#include "octeon_fau.h"
#include "octeon_fpa.h"
#include "octeon_pko.h"


/*
 *
 */
static void octeon_pko_clear_port_counts (u_int port)
{
    u_int port_num;
    octeon_pko_read_idx_t octeon_pko_idx;

    octeon_pko_idx.word64 = 0;
    octeon_pko_idx.bits.idx = port;
    octeon_pko_idx.bits.inc = 0;
    oct_write64(OCTEON_PKO_REG_READ_IDX, octeon_pko_idx.word64);

    port_num = port;
    oct_write64(OCTEON_PKO_MEM_COUNT0, port_num);
    port_num = port;
    oct_write64(OCTEON_PKO_MEM_COUNT1, port_num);
}

/*
 * octeon_pko_init
 *
 */
void octeon_pko_init (void)
{
    u_int queue, port;
    octeon_pko_read_idx_t octeon_pko_idx;
    octeon_pko_queue_cfg_t octeon_pko_queue_cfg;

    for (port = 0; port < OCTEON_PKO_PORTS_MAX; port++) {
        octeon_pko_clear_port_counts(port);
    }

    octeon_pko_idx.word64 = 0;
    octeon_pko_idx.bits.idx = 0;
    octeon_pko_idx.bits.inc = 1;
    oct_write64(OCTEON_PKO_REG_READ_IDX, octeon_pko_idx.word64);
    for (queue = 0; queue < OCTEON_PKO_QUEUES_MAX; queue++) {

        octeon_pko_queue_cfg.word64 = 0;
        octeon_pko_queue_cfg.bits.queue = queue;
        octeon_pko_queue_cfg.bits.port =  OCTEON_PKO_PORT_ILLEGAL;
        octeon_pko_queue_cfg.bits.buf_ptr = 0;
        oct_write64(OCTEON_PKO_MEM_QUEUE_PTRS, octeon_pko_queue_cfg.word64);
    }
}


/*
 * octeon_pko_enable
 *
 * enable pko
 */
void octeon_pko_enable (void)
{

    /*
     * PKO enable
     */
    oct_write64(OCTEON_PKO_REG_FLAGS, 3);    /*  octeon_pko_enable() */
}


/*
 * octeon_pko_disable
 *
 * disable pko
 */
void octeon_pko_disable (void)
{

    /*
     * PKO disable
     */
    oct_write64(OCTEON_PKO_REG_FLAGS, 0);    /*  pko_disable() */
}

/*
 * octeon_pko_config_cmdbuf_global_defaults
 *
 */
void octeon_pko_config_cmdbuf_global_defaults (u_int cmdbuf_pool,
                                               u_int cmdbuf_pool_elem_size )
{
    octeon_pko_pool_cfg_t octeon_pko_pool_config;

    octeon_pko_pool_config.word64 = 0;
    octeon_pko_pool_config.bits.pool = cmdbuf_pool;
    octeon_pko_pool_config.bits.size = cmdbuf_pool_elem_size;
    oct_write64(OCTEON_PKO_CMD_BUF, octeon_pko_pool_config.word64);
}

/*
 * octeon_pko_config_rgmx_ports
 *
 * Configure rgmx pko.  Always enables 4 + 4 ports
 */
void octeon_pko_config_rgmx_ports (void)
{
    octeon_pko_reg_gmx_port_mode_t octeon_pko_gmx_mode;

    octeon_pko_gmx_mode.word64 = 0;
    octeon_pko_gmx_mode.bits.mode0 = 2;	/* 16 >> 2 == 4 ports */
    octeon_pko_gmx_mode.bits.mode1 = 2;	/* 16 >> 2 == 4 ports */
    oct_write64(OCTEON_PKO_GMX_PORT_MODE, octeon_pko_gmx_mode.word64);
}


/*
 * octeon_pko_config
 *
 * Configure PKO
 *
 */
void octeon_pko_config (void)
{
}

/*
 * octeon_pko_get_port_status
 *
 * Get the status counters for a PKO port.
 *
 * port_num Port number to get statistics for.
 * clear    Set to 1 to clear the counters after they are read
 * status   Where to put the results.
 */
void octeon_pko_get_port_status (u_int port, u_int clear,
                                 octeon_pko_port_status_t *status)
{
    octeon_word_t packet_num;
    octeon_pko_read_idx_t octeon_pko_idx;

    packet_num.word64 = 0;

    octeon_pko_idx.word64 = 0;
    octeon_pko_idx.bits.idx = port;
    octeon_pko_idx.bits.inc = 0;
    oct_write64(OCTEON_PKO_REG_READ_IDX, octeon_pko_idx.word64);

    packet_num.word64 = oct_read64(OCTEON_PKO_MEM_COUNT0);
    status->packets = packet_num.bits.word32lo;

    status->octets = oct_read64(OCTEON_PKO_MEM_COUNT1);
    status->doorbell = oct_read64(OCTEON_PKO_MEM_DEBUG9);
    status->doorbell = (status->doorbell >> 8) & 0xfffff;
    if (clear) {
        octeon_pko_clear_port_counts(port);
    }
}

static void octeon_pko_doorbell_data_dump(uint64_t port);

static void octeon_pko_doorbell_data_dump (uint64_t port)
{
    octeon_pko_port_status_t status;

    octeon_pko_get_port_status(port, 0, &status);
    printf("\n Port #%lld  Pkts %ld   Bytes %lld  DoorBell %lld",
	(unsigned long long)port, status.packets,
	(unsigned long long)status.octets,
	(unsigned long long)status.doorbell);
}

/*
 * octeon_pko_show
 *
 * Show the OCTEON_PKO status & configs
 */
void octeon_pko_show (u_int start_port, u_int end_port)
{
    u_int queue, queue_max, gmx_int0_ports, gmx_int1_ports;
    u_int port;
    uint64_t val64;
    octeon_pko_port_status_t status;
    octeon_pko_pool_cfg_t octeon_pko_pool_config;
    octeon_pko_read_idx_t octeon_pko_idx;
    octeon_pko_queue_mode_t octeon_pko_queue_mode;
    octeon_pko_reg_gmx_port_mode_t octeon_pko_gmx_mode;
    octeon_pko_crc_ports_enable_t octeon_pko_crc_ports;
    octeon_pko_queue_cfg_t octeon_pko_queue_cfg;

    printf("\n\nPKO Status:");
    val64 = oct_read64(OCTEON_PKO_REG_FLAGS);
    if ((val64 & 0x3) != 0x3) {
        printf("  Disabled");
        return;
    } else {
        printf("  Enabled");
    }
    octeon_pko_queue_mode.word64 = oct_read64(OCTEON_PKO_QUEUE_MODE);
    queue_max = (128 >> octeon_pko_queue_mode.bits.mode);
    octeon_pko_gmx_mode.word64 = oct_read64(OCTEON_PKO_GMX_PORT_MODE);
    gmx_int0_ports = (16 >> octeon_pko_gmx_mode.bits.mode0);
    gmx_int1_ports = (16 >> octeon_pko_gmx_mode.bits.mode1);
    octeon_pko_crc_ports.word64 = oct_read64(OCTEON_PKO_REG_CRC_ENABLE);
    printf("\n Total Queues: 0..%d  Ports GMX0 %d   GMX1 %d  CRC 0x%X",
           queue_max - 1, gmx_int0_ports, gmx_int1_ports,
           octeon_pko_crc_ports.bits.crc_ports_mask);

    octeon_pko_pool_config.word64 = oct_read64(OCTEON_PKO_CMD_BUF);
    printf("\n  CmdBuf Pool: %d    CmdBuf  Size in Words: %d  Bytes: %d",
           octeon_pko_pool_config.bits.pool, octeon_pko_pool_config.bits.size,
           octeon_pko_pool_config.bits.size * 8);

    octeon_pko_idx.word64 = 0;
    octeon_pko_idx.bits.idx = 0;
    octeon_pko_idx.bits.inc = 1;
    oct_write64(OCTEON_PKO_REG_READ_IDX, octeon_pko_idx.word64);
    for (queue = 0; queue < queue_max; queue++) {

        octeon_pko_queue_cfg.word64 = oct_read64(OCTEON_PKO_MEM_QUEUE_PTRS);
        if (!octeon_pko_queue_cfg.bits.buf_ptr) continue;
        printf("\n  Port # %d   Queue %3d   [%d]  BufPtr: 0x%llX Mask: %X%s",
               octeon_pko_queue_cfg.bits.port, octeon_pko_queue_cfg.bits.queue,
               octeon_pko_queue_cfg.bits.index,
               (unsigned long long)octeon_pko_queue_cfg.bits.buf_ptr,
	       octeon_pko_queue_cfg.bits.qos_mask,
               (octeon_pko_queue_cfg.bits.tail)? "  Last":"");
    }
    printf("\n");

    for (port = start_port; port < (end_port + 1); port++) {

        octeon_pko_get_port_status(port, 0, &status);
        octeon_pko_doorbell_data_dump(port);

    }
}




/*
 * octeon_pko_config_port
 *
 * Configure a output port and the associated queues for use.
 *
 */
octeon_pko_status_t octeon_pko_config_port (u_int port,
                                            u_int base_queue,
                                            u_int num_queues,
                                            const u_int priority[],
                                            u_int pko_output_cmdbuf_fpa_pool,
                                            octeon_pko_sw_queue_info_t sw_queues[])
{
    octeon_pko_status_t	result_code;
    u_int		queue;
    octeon_pko_queue_cfg_t	qconfig;

    if ((port >= OCTEON_PKO_PORTS_MAX) && (port != OCTEON_PKO_PORT_ILLEGAL)) {
        printf("\n%% Error: octeon_pko_config_port: Invalid port %u", port);
        return (OCTEON_PKO_INVALID_PORT);
    }

    if ((base_queue + num_queues) > OCTEON_PKO_QUEUES_MAX) {
        printf("\n%% Error: octeon_pko_config_port: Invalid queue range");
        return (OCTEON_PKO_INVALID_QUEUE);
    }

    result_code = OCTEON_PKO_SUCCESS;

    for (queue = 0; queue < num_queues; queue++) {
        uint64_t  buf_ptr = 0;

        qconfig.word64          = 0;
        qconfig.bits.tail       = (queue == (num_queues - 1)) ? 1 : 0;
        qconfig.bits.index      = queue;
        qconfig.bits.port       = port;
        qconfig.bits.queue      = base_queue + queue;

        /* Convert the priority into an enable bit field. */
        /* Try to space the bits out evenly so the pkts don't get grouped up */
        switch ((int)priority[queue]) {
	    case 0: qconfig.bits.qos_mask = 0x00; break;
            case 1: qconfig.bits.qos_mask = 0x01; break;
            case 2: qconfig.bits.qos_mask = 0x11; break;
            case 3: qconfig.bits.qos_mask = 0x49; break;
            case 4: qconfig.bits.qos_mask = 0x55; break;
            case 5: qconfig.bits.qos_mask = 0x57; break;
            case 6: qconfig.bits.qos_mask = 0x77; break;
            case 7: qconfig.bits.qos_mask = 0x7f; break;
            case 8: qconfig.bits.qos_mask = 0xff; break;
            default:
                printf("\n%% Error: octeon_pko_config_port Invalid priority %llu",
                       (unsigned long long)priority[queue]);
                qconfig.bits.qos_mask = 0xff;
                result_code = OCTEON_PKO_INVALID_PRIORITY;
                break;
        }
        if (port != OCTEON_PKO_PORT_ILLEGAL) {

            buf_ptr = octeon_fpa_alloc_phys(pko_output_cmdbuf_fpa_pool);
            if (!buf_ptr) {
                printf("\n%% Error: octeon_pko_config_port: Unable to allocate");
                return (OCTEON_PKO_NO_MEMORY);
            }

            sw_queues[queue].xmit_command_state = (buf_ptr << OCTEON_PKO_INDEX_BITS);
            octeon_spinlock_init(&(sw_queues[queue].lock));

//#define DEBUG_TX

#ifdef DEBUG_TX
            printf(" PKO: port %u pool: %u  base+queue %u %u %u  buf_ptr: 0x%llX\n",
                   port,
                   pko_output_cmdbuf_fpa_pool,
                   base_queue, queue, base_queue+queue,
                   buf_ptr);

#endif
            qconfig.bits.buf_ptr = buf_ptr;
            oct_write64(OCTEON_PKO_MEM_QUEUE_PTRS, qconfig.word64);

        }
    }

    return (result_code);
}

