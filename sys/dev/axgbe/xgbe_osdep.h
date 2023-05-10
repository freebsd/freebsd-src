/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2016,2017 SoftIron Inc.
 * Copyright (c) 2020 Advanced Micro Devices, Inc.
 *
 * This software was developed by Andrew Turner under
 * the sponsorship of SoftIron Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _XGBE_OSDEP_H_
#define	_XGBE_OSDEP_H_

#include <sys/endian.h>
#include <sys/socket.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_var.h>
#include <net/iflib.h>

MALLOC_DECLARE(M_AXGBE);

typedef uint16_t __le16;
typedef uint16_t __be16;
typedef uint32_t __le32;

#define	BIT(pos)		(1ul << pos)

#define	cpu_to_be16(x)		be16toh(x)
#define	be16_to_cpu(x)		htobe16(x)
#define	lower_32_bits(x)	((x) & 0xffffffffu)
#define	upper_32_bits(x)	(((x) >> 32) & 0xffffffffu)
#define	cpu_to_le32(x)		le32toh(x)
#define	le32_to_cpu(x)		htole32(x)
#define cpu_to_le16(x)		htole16(x)

#define for_each_set_bit(bit, addr, size) \
	for ((bit) = find_first_bit((addr), (size));		\
	     (bit) < (size);					\
	     (bit) = find_next_bit((addr), (size), (bit) + 1))

typedef struct mtx spinlock_t;

static inline void
spin_lock_init(spinlock_t *spinlock)
{
	mtx_init(spinlock, "axgbe_spin", NULL, MTX_SPIN);
}

#define	spin_lock_irqsave(spinlock, flags)				\
do {									\
	(flags) = intr_disable();					\
	mtx_lock_spin(spinlock);					\
} while (0)

#define	spin_unlock_irqrestore(spinlock, flags)				\
do {									\
	mtx_unlock_spin(spinlock);					\
	intr_restore(flags);						\
} while (0)

#define	ADVERTISED_Pause		(1 << 0)
#define	ADVERTISED_Asym_Pause		(1 << 1)
#define	ADVERTISED_Autoneg		(1 << 2)
#define	ADVERTISED_Backplane		(1 << 3)
#define	ADVERTISED_10000baseKR_Full	(1 << 4)
#define	ADVERTISED_2500baseX_Full	(1 << 5)
#define	ADVERTISED_1000baseKX_Full	(1 << 6)
#define ADVERTISED_100baseT_Full        (1 << 7)
#define ADVERTISED_10000baseR_FEC	(1 << 8)
#define	ADVERTISED_10000baseT_Full	(1 << 9)
#define	ADVERTISED_2500baseT_Full	(1 << 10)
#define	ADVERTISED_1000baseT_Full	(1 << 11)
#define	ADVERTISED_TP			(1 << 12)
#define	ADVERTISED_FIBRE		(1 << 13)
#define	ADVERTISED_1000baseX_Full	(1 << 14)
#define	ADVERTISED_10000baseSR_Full	(1 << 15)
#define	ADVERTISED_10000baseLR_Full	(1 << 16)
#define	ADVERTISED_10000baseLRM_Full	(1 << 17)
#define	ADVERTISED_10000baseER_Full	(1 << 18)
#define	ADVERTISED_10000baseCR_Full	(1 << 19)
#define ADVERTISED_100baseT_Half	(1 << 20)
#define ADVERTISED_1000baseT_Half	(1 << 21)

#define	SUPPORTED_Pause			(1 << 0)
#define	SUPPORTED_Asym_Pause		(1 << 1)
#define	SUPPORTED_Autoneg		(1 << 2)
#define	SUPPORTED_Backplane		(1 << 3)
#define	SUPPORTED_10000baseKR_Full	(1 << 4)
#define	SUPPORTED_2500baseX_Full	(1 << 5)
#define	SUPPORTED_1000baseKX_Full	(1 << 6)
#define SUPPORTED_100baseT_Full        	(1 << 7)
#define SUPPORTED_10000baseR_FEC	(1 << 8)
#define	SUPPORTED_10000baseT_Full	(1 << 9)
#define	SUPPORTED_2500baseT_Full	(1 << 10)
#define	SUPPORTED_1000baseT_Full	(1 << 11)
#define	SUPPORTED_TP			(1 << 12)
#define	SUPPORTED_FIBRE			(1 << 13)
#define	SUPPORTED_1000baseX_Full	(1 << 14)
#define	SUPPORTED_10000baseSR_Full	(1 << 15)
#define	SUPPORTED_10000baseLR_Full	(1 << 16)
#define	SUPPORTED_10000baseLRM_Full	(1 << 17)
#define	SUPPORTED_10000baseER_Full	(1 << 18)
#define	SUPPORTED_10000baseCR_Full	(1 << 19)
#define SUPPORTED_100baseT_Half		(1 << 20)
#define SUPPORTED_1000baseT_Half	(1 << 21)

#define LPA_PAUSE_ASYM			0x0800

#define	AUTONEG_DISABLE			0
#define	AUTONEG_ENABLE			1

#define	DUPLEX_UNKNOWN			1
#define	DUPLEX_FULL			2
#define	DUPLEX_HALF			3

#define	SPEED_UNKNOWN			1
#define	SPEED_10000			2
#define	SPEED_2500			3
#define	SPEED_1000			4
#define	SPEED_100			5
#define SPEED_10			6

#define	BMCR_SPEED100			0x2000

#define	MDIO_MMD_PMAPMD			1
#define	MDIO_MMD_PCS			3
#define	MDIO_MMD_AN			7
#define	MDIO_MMD_VEND1			30      /* Vendor specific 1 */
#define	MDIO_MMD_VEND2			31      /* Vendor specific 2 */

#define	MDIO_PMA_10GBR_FECABLE 		170
#define	MDIO_PMA_10GBR_FECABLE_ABLE     0x0001
#define	MDIO_PMA_10GBR_FECABLE_ERRABLE  0x0002
#define	MII_ADDR_C45			(1<<30)

#define	MDIO_CTRL1			0x00 /* MII_BMCR */
#define	MDIO_CTRL1_RESET		0x8000 /* BMCR_RESET */
#define	MDIO_CTRL1_SPEEDSELEXT		0x2040 /* BMCR_SPEED1000|BMCR_SPEED100*/
#define	MDIO_CTRL1_SPEEDSEL		(MDIO_CTRL1_SPEEDSELEXT | 0x3c)
#define	MDIO_AN_CTRL1_ENABLE		0x1000 /* BMCR_AUTOEN */
#define	MDIO_CTRL1_LPOWER		0x0800 /* BMCR_PDOWN */
#define	MDIO_AN_CTRL1_RESTART		0x0200 /* BMCR_STARTNEG */

#define	MDIO_CTRL1_SPEED10G		(MDIO_CTRL1_SPEEDSELEXT | 0x00)

#define	MDIO_STAT1			1 /* MII_BMSR */
#define	MDIO_STAT1_LSTATUS		0x0004 /* BMSR_LINK */

#define MDIO_DEVID1			2 /* MII_PHYSID1 */
#define MDIO_DEVID2			3 /* MII_PHYSID2 */
#define	MDIO_SPEED			4
#define MDIO_DEVS1			5
#define MDIO_DEVS2			6
#define	MDIO_CTRL2			0x07
#define	MDIO_PCS_CTRL2_10GBR		0x0000
#define	MDIO_PCS_CTRL2_10GBX		0x0001
#define	MDIO_PCS_CTRL2_TYPE		0x0003

#define	MDIO_AN_ADVERTISE		16

#define	MDIO_AN_LPA			19

#define	ETH_ALEN			ETHER_ADDR_LEN
#define	ETH_HLEN			ETHER_HDR_LEN
#define	ETH_FCS_LEN			4
#define	VLAN_HLEN			ETHER_VLAN_ENCAP_LEN
#define VLAN_NVID			4096
#define VLAN_VID_MASK			0x0FFF

#define CRC32_POLY_LE 			0xedb88320

#define	ARRAY_SIZE(x)			nitems(x)

#define	BITS_PER_LONG			(sizeof(long) * CHAR_BIT)
#define	BITS_TO_LONGS(n)		howmany((n), BITS_PER_LONG)

#define BITMAP_LAST_WORD_MASK(n)        (~0UL >> (BITS_PER_LONG - (n)))

#define	min_t(t, a, b)			MIN((t)(a), (t)(b))
#define	max_t(t, a, b)			MAX((t)(a), (t)(b))

static inline void
clear_bit(int pos, unsigned long *p)
{

	atomic_clear_long(p, 1ul << pos);
}

static inline int
test_bit(int pos, unsigned long *p)
{
	unsigned long val;

	val = *p;
	return ((val & 1ul << pos) != 0);
}

static inline void
set_bit(int pos, unsigned long *p)
{

	atomic_set_long(p, 1ul << pos);
}

static inline int
__ffsl(long mask)
{

        return (ffsl(mask) - 1);
}

static inline int
fls64(uint64_t mask)
{

	return (flsll(mask));
}

static inline int
get_bitmask_order(unsigned int count)
{
	int order;

	order = fls(count);
	return (order);	/* We could be slightly more clever with -1 here... */
}

static inline unsigned long
find_next_bit(const unsigned long *addr, unsigned long size, unsigned long offset)
{
	long mask;
	int offs;
	int bit;
	int pos;

	if (offset >= size)
		return (size);
	pos = offset / BITS_PER_LONG;
	offs = offset % BITS_PER_LONG;
	bit = BITS_PER_LONG * pos;
	addr += pos;
	if (offs) {
		mask = (*addr) & ~BITMAP_LAST_WORD_MASK(offs);
		if (mask)
			return (bit + __ffsl(mask));
		if (size - bit <= BITS_PER_LONG)
			return (size);
		bit += BITS_PER_LONG;
		addr++;
	}
	for (size -= bit; size >= BITS_PER_LONG;
	    size -= BITS_PER_LONG, bit += BITS_PER_LONG, addr++) {
		if (*addr == 0)
			continue;
		return (bit + __ffsl(*addr));
	}
	if (size) {
		mask = (*addr) & BITMAP_LAST_WORD_MASK(size);
		if (mask)
			bit += __ffsl(mask);
		else
			bit += size;
	}
	return (bit);
}

static inline unsigned long
find_first_bit(const unsigned long *addr, unsigned long size)
{
        long mask;
        int bit;

        for (bit = 0; size >= BITS_PER_LONG;
            size -= BITS_PER_LONG, bit += BITS_PER_LONG, addr++) {
                if (*addr == 0)
                        continue;
                return (bit + __ffsl(*addr));
        }
        if (size) {
                mask = (*addr) & BITMAP_LAST_WORD_MASK(size);
                if (mask)
                        bit += __ffsl(mask);
                else
                        bit += size;
        }
        return (bit);
}

#endif /* _XGBE_OSDEP_H_ */
