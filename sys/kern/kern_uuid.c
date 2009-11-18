/*-
 * Copyright (c) 2002 Marcel Moolenaar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/endian.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/sbuf.h>
#include <sys/socket.h>
#include <sys/sysproto.h>
#include <sys/systm.h>
#include <sys/jail.h>
#include <sys/uuid.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/vnet.h>

/*
 * See also:
 *	http://www.opengroup.org/dce/info/draft-leach-uuids-guids-01.txt
 *	http://www.opengroup.org/onlinepubs/009629399/apdxa.htm
 *
 * Note that the generator state is itself an UUID, but the time and clock
 * sequence fields are written in the native byte order.
 */

CTASSERT(sizeof(struct uuid) == 16);

/* We use an alternative, more convenient representation in the generator. */
struct uuid_private {
	union {
		uint64_t	ll;		/* internal. */
		struct {
			uint32_t	low;
			uint16_t	mid;
			uint16_t	hi;
		} x;
	} time;
	uint16_t	seq;			/* Big-endian. */
	uint16_t	node[UUID_NODE_LEN>>1];
};

CTASSERT(sizeof(struct uuid_private) == 16);

static struct uuid_private uuid_last;

static struct mtx uuid_mutex;
MTX_SYSINIT(uuid_lock, &uuid_mutex, "UUID generator mutex lock", MTX_DEF);

/*
 * Return the first MAC address we encounter or, if none was found,
 * construct a sufficiently random multicast address. We don't try
 * to return the same MAC address as previously returned. We always
 * generate a new multicast address if no MAC address exists in the
 * system.
 * It would be nice to know if 'ifnet' or any of its sub-structures
 * has been changed in any way. If not, we could simply skip the
 * scan and safely return the MAC address we returned before.
 */
static void
uuid_node(uint16_t *node)
{
	struct ifnet *ifp;
	struct ifaddr *ifa;
	struct sockaddr_dl *sdl;
	int i;

	CURVNET_SET(TD_TO_VNET(curthread));
	IFNET_RLOCK_NOSLEEP();
	TAILQ_FOREACH(ifp, &V_ifnet, if_link) {
		/* Walk the address list */
		IF_ADDR_LOCK(ifp);
		TAILQ_FOREACH(ifa, &ifp->if_addrhead, ifa_link) {
			sdl = (struct sockaddr_dl*)ifa->ifa_addr;
			if (sdl != NULL && sdl->sdl_family == AF_LINK &&
			    sdl->sdl_type == IFT_ETHER) {
				/* Got a MAC address. */
				bcopy(LLADDR(sdl), node, UUID_NODE_LEN);
				IF_ADDR_UNLOCK(ifp);
				IFNET_RUNLOCK_NOSLEEP();
				CURVNET_RESTORE();
				return;
			}
		}
		IF_ADDR_UNLOCK(ifp);
	}
	IFNET_RUNLOCK_NOSLEEP();

	for (i = 0; i < (UUID_NODE_LEN>>1); i++)
		node[i] = (uint16_t)arc4random();
	*((uint8_t*)node) |= 0x01;
	CURVNET_RESTORE();
}

/*
 * Get the current time as a 60 bit count of 100-nanosecond intervals
 * since 00:00:00.00, October 15,1582. We apply a magic offset to convert
 * the Unix time since 00:00:00.00, January 1, 1970 to the date of the
 * Gregorian reform to the Christian calendar.
 */
static uint64_t
uuid_time(void)
{
	struct bintime bt;
	uint64_t time = 0x01B21DD213814000LL;

	bintime(&bt);
	time += (uint64_t)bt.sec * 10000000LL;
	time += (10000000LL * (uint32_t)(bt.frac >> 32)) >> 32;
	return (time & ((1LL << 60) - 1LL));
}

struct uuid *
kern_uuidgen(struct uuid *store, size_t count)
{
	struct uuid_private uuid;
	uint64_t time;
	size_t n;

	mtx_lock(&uuid_mutex);

	uuid_node(uuid.node);
	time = uuid_time();

	if (uuid_last.time.ll == 0LL || uuid_last.node[0] != uuid.node[0] ||
	    uuid_last.node[1] != uuid.node[1] ||
	    uuid_last.node[2] != uuid.node[2])
		uuid.seq = (uint16_t)arc4random() & 0x3fff;
	else if (uuid_last.time.ll >= time)
		uuid.seq = (uuid_last.seq + 1) & 0x3fff;
	else
		uuid.seq = uuid_last.seq;

	uuid_last = uuid;
	uuid_last.time.ll = (time + count - 1) & ((1LL << 60) - 1LL);

	mtx_unlock(&uuid_mutex);

	/* Set sequence and variant and deal with byte order. */
	uuid.seq = htobe16(uuid.seq | 0x8000);

	for (n = 0; n < count; n++) {
		/* Set time and version (=1). */
		uuid.time.x.low = (uint32_t)time;
		uuid.time.x.mid = (uint16_t)(time >> 32);
		uuid.time.x.hi = ((uint16_t)(time >> 48) & 0xfff) | (1 << 12);
		store[n] = *(struct uuid *)&uuid;
		time++;
	}

	return (store);
}

#ifndef _SYS_SYSPROTO_H_
struct uuidgen_args {
	struct uuid *store;
	int	count;
};
#endif
int
uuidgen(struct thread *td, struct uuidgen_args *uap)
{
	struct uuid *store;
	size_t count;
	int error;

	/*
	 * Limit the number of UUIDs that can be created at the same time
	 * to some arbitrary number. This isn't really necessary, but I
	 * like to have some sort of upper-bound that's less than 2G :-)
	 * XXX probably needs to be tunable.
	 */
	if (uap->count < 1 || uap->count > 2048)
		return (EINVAL);

	count = uap->count;
	store = malloc(count * sizeof(struct uuid), M_TEMP, M_WAITOK);
	kern_uuidgen(store, count);
	error = copyout(store, uap->store, count * sizeof(struct uuid));
	free(store, M_TEMP);
	return (error);
}

int
snprintf_uuid(char *buf, size_t sz, struct uuid *uuid)
{
	struct uuid_private *id;
	int cnt;

	id = (struct uuid_private *)uuid;
	cnt = snprintf(buf, sz, "%08x-%04x-%04x-%04x-%04x%04x%04x",
	    id->time.x.low, id->time.x.mid, id->time.x.hi, be16toh(id->seq),
	    be16toh(id->node[0]), be16toh(id->node[1]), be16toh(id->node[2]));
	return (cnt);
}

int
printf_uuid(struct uuid *uuid)
{
	char buf[38];

	snprintf_uuid(buf, sizeof(buf), uuid);
	return (printf("%s", buf));
}

int
sbuf_printf_uuid(struct sbuf *sb, struct uuid *uuid)
{
	char buf[38];

	snprintf_uuid(buf, sizeof(buf), uuid);
	return (sbuf_printf(sb, "%s", buf));
}

/*
 * Encode/Decode UUID into byte-stream.
 *   http://www.opengroup.org/dce/info/draft-leach-uuids-guids-01.txt
 *
 * 0                   1                   2                   3
 *   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |                          time_low                             |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |       time_mid                |         time_hi_and_version   |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |clk_seq_hi_res |  clk_seq_low  |         node (0-1)            |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |                         node (2-5)                            |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */

void
le_uuid_enc(void *buf, struct uuid const *uuid)
{
	u_char *p;
	int i;

	p = buf;
	le32enc(p, uuid->time_low);
	le16enc(p + 4, uuid->time_mid);
	le16enc(p + 6, uuid->time_hi_and_version);
	p[8] = uuid->clock_seq_hi_and_reserved;
	p[9] = uuid->clock_seq_low;
	for (i = 0; i < _UUID_NODE_LEN; i++)
		p[10 + i] = uuid->node[i];
}

void
le_uuid_dec(void const *buf, struct uuid *uuid)
{
	u_char const *p;
	int i;

	p = buf;
	uuid->time_low = le32dec(p);
	uuid->time_mid = le16dec(p + 4);
	uuid->time_hi_and_version = le16dec(p + 6);
	uuid->clock_seq_hi_and_reserved = p[8];
	uuid->clock_seq_low = p[9];
	for (i = 0; i < _UUID_NODE_LEN; i++)
		uuid->node[i] = p[10 + i];
}

void
be_uuid_enc(void *buf, struct uuid const *uuid)
{
	u_char *p;
	int i;

	p = buf;
	be32enc(p, uuid->time_low);
	be16enc(p + 4, uuid->time_mid);
	be16enc(p + 6, uuid->time_hi_and_version);
	p[8] = uuid->clock_seq_hi_and_reserved;
	p[9] = uuid->clock_seq_low;
	for (i = 0; i < _UUID_NODE_LEN; i++)
		p[10 + i] = uuid->node[i];
}

void
be_uuid_dec(void const *buf, struct uuid *uuid)
{
	u_char const *p;
	int i;

	p = buf;
	uuid->time_low = be32dec(p);
	uuid->time_mid = le16dec(p + 4);
	uuid->time_hi_and_version = be16dec(p + 6);
	uuid->clock_seq_hi_and_reserved = p[8];
	uuid->clock_seq_low = p[9];
	for (i = 0; i < _UUID_NODE_LEN; i++)
		uuid->node[i] = p[10 + i];
}

int
parse_uuid(const char *str, struct uuid *uuid)
{
	u_int c[11];
	int n;

	/* An empty string represents a nil UUID. */
	if (*str == '\0') {
		bzero(uuid, sizeof(*uuid));
		return (0);
	}

	/* The UUID string representation has a fixed length. */
	if (strlen(str) != 36)
		return (EINVAL);

	/*
	 * We only work with "new" UUIDs. New UUIDs have the form:
	 *      01234567-89ab-cdef-0123-456789abcdef
	 * The so called "old" UUIDs, which we don't support, have the form:
	 *      0123456789ab.cd.ef.01.23.45.67.89.ab
	 */
	if (str[8] != '-')
		return (EINVAL);

	n = sscanf(str, "%8x-%4x-%4x-%2x%2x-%2x%2x%2x%2x%2x%2x", c + 0, c + 1,
	    c + 2, c + 3, c + 4, c + 5, c + 6, c + 7, c + 8, c + 9, c + 10);
	/* Make sure we have all conversions. */
	if (n != 11)
		return (EINVAL);

	/* Successful scan. Build the UUID. */
	uuid->time_low = c[0];
	uuid->time_mid = c[1];
	uuid->time_hi_and_version = c[2];
	uuid->clock_seq_hi_and_reserved = c[3];
	uuid->clock_seq_low = c[4];
	for (n = 0; n < 6; n++)
		uuid->node[n] = c[n + 5];

	/* Check semantics... */
	return (((c[3] & 0x80) != 0x00 &&		/* variant 0? */
	    (c[3] & 0xc0) != 0x80 &&			/* variant 1? */
	    (c[3] & 0xe0) != 0xc0) ? EINVAL : 0);	/* variant 2? */
}
