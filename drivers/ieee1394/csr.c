/*
 * IEEE 1394 for Linux
 *
 * CSR implementation, iso/bus manager implementation.
 *
 * Copyright (C) 1999 Andreas E. Bombe
 *               2002 Manfred Weihs <weihs@ict.tuwien.ac.at>
 *
 * This code is licensed under the GPL.  See the file COPYING in the root
 * directory of the kernel sources for details.
 *
 *
 * Contributions:
 *
 * Manfred Weihs <weihs@ict.tuwien.ac.at>
 *        configuration ROM manipulation
 *
 */

#include <linux/string.h>
#include <linux/module.h> /* needed for MODULE_PARM */
#include <linux/param.h>
#include <linux/spinlock.h>

#include "ieee1394_types.h"
#include "hosts.h"
#include "ieee1394.h"
#include "highlevel.h"

/* Module Parameters */
/* this module parameter can be used to disable mapping of the FCP registers */
MODULE_PARM(fcp,"i");
MODULE_PARM_DESC(fcp, "FCP-registers");
static int fcp = 1;

static u16 csr_crc16(unsigned *data, int length)
{
        int check=0, i;
        int shift, sum, next=0;

        for (i = length; i; i--) {
                for (next = check, shift = 28; shift >= 0; shift -= 4 ) {
                        sum = ((next >> 12) ^ (be32_to_cpu(*data) >> shift)) & 0xf;
                        next = (next << 4) ^ (sum << 12) ^ (sum << 5) ^ (sum);
                }
                check = next & 0xffff;
                data++;
        }

        return check;
}

static void host_reset(struct hpsb_host *host)
{
        host->csr.state &= 0x300;

        host->csr.bus_manager_id = 0x3f;
        host->csr.bandwidth_available = 4915;
	host->csr.channels_available_hi = 0xfffffffe;	/* pre-alloc ch 31 per 1394a-2000 */
        host->csr.channels_available_lo = ~0;
	host->csr.broadcast_channel = 0x80000000 | 31;

	if (host->is_irm) {
		if (host->driver->hw_csr_reg) {
			host->driver->hw_csr_reg(host, 2, 0xfffffffe, ~0);
		}
	}

        host->csr.node_ids = host->node_id << 16;

        if (!host->is_root) {
                /* clear cmstr bit */
                host->csr.state &= ~0x100;
        }

        host->csr.topology_map[1] = 
                cpu_to_be32(be32_to_cpu(host->csr.topology_map[1]) + 1);
        host->csr.topology_map[2] = cpu_to_be32(host->node_count << 16 
                                                | host->selfid_count);
        host->csr.topology_map[0] = 
                cpu_to_be32((host->selfid_count + 2) << 16
                            | csr_crc16(host->csr.topology_map + 1,
                                        host->selfid_count + 2));

        host->csr.speed_map[1] = 
                cpu_to_be32(be32_to_cpu(host->csr.speed_map[1]) + 1);
        host->csr.speed_map[0] = cpu_to_be32(0x3f1 << 16 
                                             | csr_crc16(host->csr.speed_map+1,
                                                         0x3f1));
}

/* 
 * HI == seconds (bits 0:2)
 * LO == fraction units of 1/8000 of a second, as per 1394 (bits 19:31)
 *
 * Convert to units and then to HZ, for comparison to jiffies.
 *
 * By default this will end up being 800 units, or 100ms (125usec per
 * unit).
 *
 * NOTE: The spec says 1/8000, but also says we can compute based on 1/8192
 * like CSR specifies. Should make our math less complex.
 */
static inline void calculate_expire(struct csr_control *csr)
{
	unsigned long units;
	
	/* Take the seconds, and convert to units */
	units = (unsigned long)(csr->split_timeout_hi & 0x07) << 13;

	/* Add in the fractional units */
	units += (unsigned long)(csr->split_timeout_lo >> 19);

	/* Convert to jiffies */
	csr->expire = (unsigned long)(units * HZ) >> 13UL;

	/* Just to keep from rounding low */
	csr->expire++;

	HPSB_VERBOSE("CSR: setting expire to %lu, HZ=%d", csr->expire, HZ);
}


static void add_host(struct hpsb_host *host)
{
        host->csr.lock = SPIN_LOCK_UNLOCKED;

        host->csr.rom_size = host->driver->get_rom(host, &host->csr.rom);
        host->csr.rom_version           = 0;
        host->csr.state                 = 0;
        host->csr.node_ids              = 0;
        host->csr.split_timeout_hi      = 0;
        host->csr.split_timeout_lo      = 800 << 19;
	calculate_expire(&host->csr);
        host->csr.cycle_time            = 0;
        host->csr.bus_time              = 0;
        host->csr.bus_manager_id        = 0x3f;
        host->csr.bandwidth_available   = 4915;
	host->csr.channels_available_hi = 0xfffffffe;	/* pre-alloc ch 31 per 1394a-2000 */
        host->csr.channels_available_lo = ~0;
	host->csr.broadcast_channel = 0x80000000 | 31;

	if (host->is_irm) {
		if (host->driver->hw_csr_reg) {
			host->driver->hw_csr_reg(host, 2, 0xfffffffe, ~0);
		}
	}
}

int hpsb_update_config_rom(struct hpsb_host *host, const quadlet_t *new_rom, 
	size_t size, unsigned char rom_version)
{
	unsigned long flags;
	int ret;

        spin_lock_irqsave(&host->csr.lock, flags); 
        if (rom_version != host->csr.rom_version)
                 ret = -1;
        else if (size > (CSR_CONFIG_ROM_SIZE << 2))
                 ret = -2;
        else {
                 memcpy(host->csr.rom,new_rom,size);
                 host->csr.rom_size=size;
                 host->csr.rom_version++;
                 ret=0;
        }
        spin_unlock_irqrestore(&host->csr.lock, flags);
        return ret;
}

int hpsb_get_config_rom(struct hpsb_host *host, quadlet_t *buffer, 
	size_t buffersize, size_t *rom_size, unsigned char *rom_version)
{
	unsigned long flags;
	int ret;

        spin_lock_irqsave(&host->csr.lock, flags); 
        *rom_version=host->csr.rom_version;
        *rom_size=host->csr.rom_size;
        if (buffersize < host->csr.rom_size)
                 ret = -1;
        else {
                 memcpy(buffer,host->csr.rom,host->csr.rom_size);
                 ret=0;
        }
        spin_unlock_irqrestore(&host->csr.lock, flags);
        return ret;
}


/* Read topology / speed maps and configuration ROM */
static int read_maps(struct hpsb_host *host, int nodeid, quadlet_t *buffer,
                     u64 addr, size_t length, u16 fl)
{
	unsigned long flags;
        int csraddr = addr - CSR_REGISTER_BASE;
        const char *src;

        spin_lock_irqsave(&host->csr.lock, flags); 

        if (csraddr < CSR_TOPOLOGY_MAP) {
                if (csraddr + length > CSR_CONFIG_ROM + host->csr.rom_size) {
                        spin_unlock_irqrestore(&host->csr.lock, flags);
                        return RCODE_ADDRESS_ERROR;
                }
                src = ((char *)host->csr.rom) + csraddr - CSR_CONFIG_ROM;
        } else if (csraddr < CSR_SPEED_MAP) {
                src = ((char *)host->csr.topology_map) + csraddr 
                        - CSR_TOPOLOGY_MAP;
        } else {
                src = ((char *)host->csr.speed_map) + csraddr - CSR_SPEED_MAP;
        }

        memcpy(buffer, src, length);
        spin_unlock_irqrestore(&host->csr.lock, flags);
        return RCODE_COMPLETE;
}


#define out if (--length == 0) break

static int read_regs(struct hpsb_host *host, int nodeid, quadlet_t *buf,
                     u64 addr, size_t length, u16 flags)
{
        int csraddr = addr - CSR_REGISTER_BASE;
        int oldcycle;
        quadlet_t ret;
        
        if ((csraddr | length) & 0x3)
                return RCODE_TYPE_ERROR;

        length /= 4;

        switch (csraddr) {
        case CSR_STATE_CLEAR:
                *(buf++) = cpu_to_be32(host->csr.state);
                out;
        case CSR_STATE_SET:
                *(buf++) = cpu_to_be32(host->csr.state);
                out;
        case CSR_NODE_IDS:
                *(buf++) = cpu_to_be32(host->csr.node_ids);
                out;

        case CSR_RESET_START:
                return RCODE_TYPE_ERROR;

                /* address gap - handled by default below */

        case CSR_SPLIT_TIMEOUT_HI:
                *(buf++) = cpu_to_be32(host->csr.split_timeout_hi);
                out;
        case CSR_SPLIT_TIMEOUT_LO:
                *(buf++) = cpu_to_be32(host->csr.split_timeout_lo);
                out;

                /* address gap */
                return RCODE_ADDRESS_ERROR;

        case CSR_CYCLE_TIME:
                oldcycle = host->csr.cycle_time;
                host->csr.cycle_time =
                        host->driver->devctl(host, GET_CYCLE_COUNTER, 0);

                if (oldcycle > host->csr.cycle_time) {
                        /* cycle time wrapped around */
                        host->csr.bus_time += 1 << 7;
                }
                *(buf++) = cpu_to_be32(host->csr.cycle_time);
                out;
        case CSR_BUS_TIME:
                oldcycle = host->csr.cycle_time;
                host->csr.cycle_time =
                        host->driver->devctl(host, GET_CYCLE_COUNTER, 0);

                if (oldcycle > host->csr.cycle_time) {
                        /* cycle time wrapped around */
                        host->csr.bus_time += (1 << 7);
                }
                *(buf++) = cpu_to_be32(host->csr.bus_time 
                                       | (host->csr.cycle_time >> 25));
                out;

                /* address gap */
                return RCODE_ADDRESS_ERROR;

        case CSR_BUSY_TIMEOUT:
                /* not yet implemented */
                return RCODE_ADDRESS_ERROR;

        case CSR_BUS_MANAGER_ID:
                if (host->driver->hw_csr_reg)
                        ret = host->driver->hw_csr_reg(host, 0, 0, 0);
                else
                        ret = host->csr.bus_manager_id;

                *(buf++) = cpu_to_be32(ret);
                out;
        case CSR_BANDWIDTH_AVAILABLE:
                if (host->driver->hw_csr_reg)
                        ret = host->driver->hw_csr_reg(host, 1, 0, 0);
                else
                        ret = host->csr.bandwidth_available;

                *(buf++) = cpu_to_be32(ret);
                out;
        case CSR_CHANNELS_AVAILABLE_HI:
                if (host->driver->hw_csr_reg)
                        ret = host->driver->hw_csr_reg(host, 2, 0, 0);
                else
                        ret = host->csr.channels_available_hi;

                *(buf++) = cpu_to_be32(ret);
                out;
        case CSR_CHANNELS_AVAILABLE_LO:
                if (host->driver->hw_csr_reg)
                        ret = host->driver->hw_csr_reg(host, 3, 0, 0);
                else
                        ret = host->csr.channels_available_lo;

                *(buf++) = cpu_to_be32(ret);
                out;

	case CSR_BROADCAST_CHANNEL:
		*(buf++) = cpu_to_be32(host->csr.broadcast_channel);
		out;

                /* address gap to end - fall through to default */
        default:
                return RCODE_ADDRESS_ERROR;
        }

        return RCODE_COMPLETE;
}

static int write_regs(struct hpsb_host *host, int nodeid, int destid,
		      quadlet_t *data, u64 addr, size_t length, u16 flags)
{
        int csraddr = addr - CSR_REGISTER_BASE;
        
        if ((csraddr | length) & 0x3)
                return RCODE_TYPE_ERROR;

        length /= 4;

        switch (csraddr) {
        case CSR_STATE_CLEAR:
                /* FIXME FIXME FIXME */
                printk("doh, someone wants to mess with state clear\n");
                out;
        case CSR_STATE_SET:
                printk("doh, someone wants to mess with state set\n");
                out;

        case CSR_NODE_IDS:
                host->csr.node_ids &= NODE_MASK << 16;
                host->csr.node_ids |= be32_to_cpu(*(data++)) & (BUS_MASK << 16);
                host->node_id = host->csr.node_ids >> 16;
                host->driver->devctl(host, SET_BUS_ID, host->node_id >> 6);
                out;

        case CSR_RESET_START:
                /* FIXME - perform command reset */
                out;

                /* address gap */
                return RCODE_ADDRESS_ERROR;

        case CSR_SPLIT_TIMEOUT_HI:
                host->csr.split_timeout_hi = 
                        be32_to_cpu(*(data++)) & 0x00000007;
		calculate_expire(&host->csr);
                out;
        case CSR_SPLIT_TIMEOUT_LO:
                host->csr.split_timeout_lo = 
                        be32_to_cpu(*(data++)) & 0xfff80000;
		calculate_expire(&host->csr);
                out;

                /* address gap */
                return RCODE_ADDRESS_ERROR;

        case CSR_CYCLE_TIME:
                /* should only be set by cycle start packet, automatically */
                host->csr.cycle_time = be32_to_cpu(*data);
                host->driver->devctl(host, SET_CYCLE_COUNTER,
                                       be32_to_cpu(*(data++)));
                out;
        case CSR_BUS_TIME:
                host->csr.bus_time = be32_to_cpu(*(data++)) & 0xffffff80;
                out;

                /* address gap */
                return RCODE_ADDRESS_ERROR;

        case CSR_BUSY_TIMEOUT:
                /* not yet implemented */
                return RCODE_ADDRESS_ERROR;

        case CSR_BUS_MANAGER_ID:
        case CSR_BANDWIDTH_AVAILABLE:
        case CSR_CHANNELS_AVAILABLE_HI:
        case CSR_CHANNELS_AVAILABLE_LO:
                /* these are not writable, only lockable */
                return RCODE_TYPE_ERROR;

	case CSR_BROADCAST_CHANNEL:
		/* only the valid bit can be written */
		host->csr.broadcast_channel = (host->csr.broadcast_channel & ~0x40000000)
                        | (be32_to_cpu(*data) & 0x40000000);
		out;

                /* address gap to end - fall through */
        default:
                return RCODE_ADDRESS_ERROR;
        }

        return RCODE_COMPLETE;
}

#undef out


static int lock_regs(struct hpsb_host *host, int nodeid, quadlet_t *store,
                     u64 addr, quadlet_t data, quadlet_t arg, int extcode, u16 fl)
{
        int csraddr = addr - CSR_REGISTER_BASE;
        unsigned long flags;
        quadlet_t *regptr = NULL;

        if (csraddr & 0x3)
		return RCODE_TYPE_ERROR;

        if (csraddr < CSR_BUS_MANAGER_ID || csraddr > CSR_CHANNELS_AVAILABLE_LO
            || extcode != EXTCODE_COMPARE_SWAP)
                goto unsupported_lockreq;

        data = be32_to_cpu(data);
        arg = be32_to_cpu(arg);

	/* Is somebody releasing the broadcast_channel on us? */
	if (csraddr == CSR_CHANNELS_AVAILABLE_HI && (data & 0x1)) {
		/* Note: this is may not be the right way to handle
		 * the problem, so we should look into the proper way
		 * eventually. */
		HPSB_WARN("Node [" NODE_BUS_FMT "] wants to release "
			  "broadcast channel 31.  Ignoring.",
			  NODE_BUS_ARGS(host, nodeid));

		data &= ~0x1;	/* keep broadcast channel allocated */
	}

        if (host->driver->hw_csr_reg) {
                quadlet_t old;

                old = host->driver->
                        hw_csr_reg(host, (csraddr - CSR_BUS_MANAGER_ID) >> 2,
                                   data, arg);

                *store = cpu_to_be32(old);
                return RCODE_COMPLETE;
        }

        spin_lock_irqsave(&host->csr.lock, flags);

        switch (csraddr) {
        case CSR_BUS_MANAGER_ID:
                regptr = &host->csr.bus_manager_id;
		*store = cpu_to_be32(*regptr);
		if (*regptr == arg)
			*regptr = data;
                break;

        case CSR_BANDWIDTH_AVAILABLE:
        {
                quadlet_t bandwidth;
                quadlet_t old;
                quadlet_t new;

                regptr = &host->csr.bandwidth_available;
                old = *regptr;

                /* bandwidth available algorithm adapted from IEEE 1394a-2000 spec */
                if (arg > 0x1fff) {
                        *store = cpu_to_be32(old);	/* change nothing */
			break;
                }
                data &= 0x1fff;
                if (arg >= data) {
                        /* allocate bandwidth */
                        bandwidth = arg - data;
                        if (old >= bandwidth) {
                                new = old - bandwidth;
                                *store = cpu_to_be32(arg);
                                *regptr = new;
                        } else {
                                *store = cpu_to_be32(old);
                        }
                } else {
                        /* deallocate bandwidth */
                        bandwidth = data - arg;
                        if (old + bandwidth < 0x2000) {
                                new = old + bandwidth;
                                *store = cpu_to_be32(arg);
                                *regptr = new;
                        } else {
                                *store = cpu_to_be32(old);
                        }
                }
                break;
        }

        case CSR_CHANNELS_AVAILABLE_HI:
        {
                /* Lock algorithm for CHANNELS_AVAILABLE as recommended by 1394a-2000 */
                quadlet_t affected_channels = arg ^ data;

                regptr = &host->csr.channels_available_hi;

                if ((arg & affected_channels) == (*regptr & affected_channels)) {
                        *regptr ^= affected_channels;
                        *store = cpu_to_be32(arg);
                } else {
                        *store = cpu_to_be32(*regptr);
                }

                break;
        }

        case CSR_CHANNELS_AVAILABLE_LO:
        {
                /* Lock algorithm for CHANNELS_AVAILABLE as recommended by 1394a-2000 */
                quadlet_t affected_channels = arg ^ data;

                regptr = &host->csr.channels_available_lo;

                if ((arg & affected_channels) == (*regptr & affected_channels)) {
                        *regptr ^= affected_channels;
                        *store = cpu_to_be32(arg);
                } else {
                        *store = cpu_to_be32(*regptr);
                }
                break;
        }
        }

        spin_unlock_irqrestore(&host->csr.lock, flags);

        return RCODE_COMPLETE;

 unsupported_lockreq:
        switch (csraddr) {
        case CSR_STATE_CLEAR:
        case CSR_STATE_SET:
        case CSR_RESET_START:
        case CSR_NODE_IDS:
        case CSR_SPLIT_TIMEOUT_HI:
        case CSR_SPLIT_TIMEOUT_LO:
        case CSR_CYCLE_TIME:
        case CSR_BUS_TIME:
	case CSR_BROADCAST_CHANNEL:
                return RCODE_TYPE_ERROR;

        case CSR_BUSY_TIMEOUT:
                /* not yet implemented - fall through */
        default:
                return RCODE_ADDRESS_ERROR;
        }
}

static int lock64_regs(struct hpsb_host *host, int nodeid, octlet_t * store,
		       u64 addr, octlet_t data, octlet_t arg, int extcode, u16 fl)
{
	int csraddr = addr - CSR_REGISTER_BASE;
	unsigned long flags;

	data = be64_to_cpu(data);
	arg = be64_to_cpu(arg);

	if (csraddr & 0x3)
		return RCODE_TYPE_ERROR;

	if (csraddr != CSR_CHANNELS_AVAILABLE
	    || extcode != EXTCODE_COMPARE_SWAP)
		goto unsupported_lock64req;

	/* Is somebody releasing the broadcast_channel on us? */
	if (csraddr == CSR_CHANNELS_AVAILABLE_HI && (data & 0x100000000ULL)) {
		/* Note: this is may not be the right way to handle
		 * the problem, so we should look into the proper way
                 * eventually. */
		HPSB_WARN("Node [" NODE_BUS_FMT "] wants to release "
			  "broadcast channel 31.  Ignoring.",
			  NODE_BUS_ARGS(host, nodeid));

		data &= ~0x100000000ULL;	/* keep broadcast channel allocated */
	}

	if (host->driver->hw_csr_reg) {
		quadlet_t data_hi, data_lo;
		quadlet_t arg_hi, arg_lo;
		quadlet_t old_hi, old_lo;

		data_hi = data >> 32;
		data_lo = data & 0xFFFFFFFF;
		arg_hi = arg >> 32;
		arg_lo = arg & 0xFFFFFFFF;

		old_hi = host->driver->hw_csr_reg(host, (csraddr - CSR_BUS_MANAGER_ID) >> 2,
                                                  data_hi, arg_hi);

		old_lo = host->driver->hw_csr_reg(host, ((csraddr + 4) - CSR_BUS_MANAGER_ID) >> 2,
                                                  data_lo, arg_lo);

		*store = cpu_to_be64(((octlet_t)old_hi << 32) | old_lo);
	} else {
		octlet_t old;
		octlet_t affected_channels = arg ^ data;

		spin_lock_irqsave(&host->csr.lock, flags);

		old = ((octlet_t)host->csr.channels_available_hi << 32) | host->csr.channels_available_lo;

		if ((arg & affected_channels) == (old & affected_channels)) {
			host->csr.channels_available_hi ^= (affected_channels >> 32);
			host->csr.channels_available_lo ^= (affected_channels & 0xffffffff);
			*store = cpu_to_be64(arg);
		} else {
			*store = cpu_to_be64(old);
		}

		spin_unlock_irqrestore(&host->csr.lock, flags);
	}

	/* Is somebody erroneously releasing the broadcast_channel on us? */
	if (host->csr.channels_available_hi & 0x1)
		host->csr.channels_available_hi &= ~0x1;

	return RCODE_COMPLETE;

 unsupported_lock64req:
	switch (csraddr) {
	case CSR_STATE_CLEAR:
	case CSR_STATE_SET:
	case CSR_RESET_START:
	case CSR_NODE_IDS:
	case CSR_SPLIT_TIMEOUT_HI:
	case CSR_SPLIT_TIMEOUT_LO:
	case CSR_CYCLE_TIME:
	case CSR_BUS_TIME:
	case CSR_BUS_MANAGER_ID:
	case CSR_BROADCAST_CHANNEL:
	case CSR_BUSY_TIMEOUT:
	case CSR_BANDWIDTH_AVAILABLE:
		return RCODE_TYPE_ERROR;

	default:
		return RCODE_ADDRESS_ERROR;
	}
}

static int write_fcp(struct hpsb_host *host, int nodeid, int dest,
		     quadlet_t *data, u64 addr, size_t length, u16 flags)
{
        int csraddr = addr - CSR_REGISTER_BASE;

        if (length > 512)
                return RCODE_TYPE_ERROR;

        switch (csraddr) {
        case CSR_FCP_COMMAND:
                highlevel_fcp_request(host, nodeid, 0, (u8 *)data, length);
                break;
        case CSR_FCP_RESPONSE:
                highlevel_fcp_request(host, nodeid, 1, (u8 *)data, length);
                break;
        default:
                return RCODE_TYPE_ERROR;
        }

        return RCODE_COMPLETE;
}


static struct hpsb_highlevel csr_highlevel = {
	.name =		"standard registers",
	.add_host =	add_host,
        .host_reset =	host_reset,
};


static struct hpsb_address_ops map_ops = {
        .read = read_maps,
};

static struct hpsb_address_ops fcp_ops = {
        .write = write_fcp,
};

static struct hpsb_address_ops reg_ops = {
        .read = read_regs,
        .write = write_regs,
        .lock = lock_regs,
	.lock64 = lock64_regs,
};

void init_csr(void)
{
	hpsb_register_highlevel(&csr_highlevel);

        hpsb_register_addrspace(&csr_highlevel, &reg_ops, CSR_REGISTER_BASE,
                                CSR_REGISTER_BASE + CSR_CONFIG_ROM);
        hpsb_register_addrspace(&csr_highlevel, &map_ops, 
                                CSR_REGISTER_BASE + CSR_CONFIG_ROM,
                                CSR_REGISTER_BASE + CSR_CONFIG_ROM_END);
        if (fcp) {
		hpsb_register_addrspace(&csr_highlevel, &fcp_ops,
                                CSR_REGISTER_BASE + CSR_FCP_COMMAND,
                                CSR_REGISTER_BASE + CSR_FCP_END);
	}
        hpsb_register_addrspace(&csr_highlevel, &map_ops,
                                CSR_REGISTER_BASE + CSR_TOPOLOGY_MAP,
                                CSR_REGISTER_BASE + CSR_TOPOLOGY_MAP_END);
        hpsb_register_addrspace(&csr_highlevel, &map_ops,
                                CSR_REGISTER_BASE + CSR_SPEED_MAP,
                                CSR_REGISTER_BASE + CSR_SPEED_MAP_END);
}

void cleanup_csr(void)
{
        hpsb_unregister_highlevel(&csr_highlevel);
}
