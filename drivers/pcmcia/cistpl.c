/*======================================================================

    PCMCIA Card Information Structure parser

    cistpl.c 1.99 2002/10/24 06:11:48

    The contents of this file are subject to the Mozilla Public
    License Version 1.1 (the "License"); you may not use this file
    except in compliance with the License. You may obtain a copy of
    the License at http://www.mozilla.org/MPL/

    Software distributed under the License is distributed on an "AS
    IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
    implied. See the License for the specific language governing
    rights and limitations under the License.

    The initial developer of the original code is David A. Hinds
    <dahinds@users.sourceforge.net>.  Portions created by David A. Hinds
    are Copyright (C) 1999 David A. Hinds.  All Rights Reserved.

    Alternatively, the contents of this file may be used under the
    terms of the GNU General Public License version 2 (the "GPL"), in
    which case the provisions of the GPL are applicable instead of the
    above.  If you wish to allow the use of your version of this file
    only under the terms of the GPL and not to allow others to use
    your version of this file under the MPL, indicate your decision
    by deleting the provisions above and replace them with the notice
    and other provisions required by the GPL.  If you do not delete
    the provisions above, a recipient may use your version of this
    file under either the MPL or the GPL.
    
======================================================================*/

#define __NO_VERSION__

#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/major.h>
#include <linux/errno.h>
#include <linux/timer.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/pci.h>
#include <linux/ioport.h>
#include <asm/io.h>
#include <asm/byteorder.h>

#include <pcmcia/cs_types.h>
#include <pcmcia/bus_ops.h>
#include <pcmcia/ss.h>
#include <pcmcia/cs.h>
#include <pcmcia/bulkmem.h>
#include <pcmcia/cisreg.h>
#include <pcmcia/cistpl.h>
#include "cs_internal.h"

static const u_char mantissa[] = {
    10, 12, 13, 15, 20, 25, 30, 35,
    40, 45, 50, 55, 60, 70, 80, 90
};

static const u_int exponent[] = {
    1, 10, 100, 1000, 10000, 100000, 1000000, 10000000
};

/* Convert an extended speed byte to a time in nanoseconds */
#define SPEED_CVT(v) \
    (mantissa[(((v)>>3)&15)-1] * exponent[(v)&7] / 10)
/* Convert a power byte to a current in 0.1 microamps */
#define POWER_CVT(v) \
    (mantissa[((v)>>3)&15] * exponent[(v)&7] / 10)
#define POWER_SCALE(v)		(exponent[(v)&7])

/* Upper limit on reasonable # of tuples */
#define MAX_TUPLES		200

/*====================================================================*/

/* Parameters that can be set with 'insmod' */

#define INT_MODULE_PARM(n, v) static int n = v; MODULE_PARM(n, "i")

INT_MODULE_PARM(cis_width,	0);		/* 16-bit CIS? */

/*======================================================================

    Low-level functions to read and write CIS memory.  I think the
    write routine is only useful for writing one-byte registers.
    
======================================================================*/

/* Bits in attr field */
#define IS_ATTR		1
#define IS_INDIRECT	8

static int setup_cis_mem(socket_info_t *s);

static void set_cis_map(socket_info_t *s, pccard_mem_map *mem)
{
    s->ss_entry->set_mem_map(s->sock, mem);
    if (s->cap.features & SS_CAP_STATIC_MAP) {
	if (s->cis_virt)
	    bus_iounmap(s->cap.bus, s->cis_virt);
	s->cis_virt = bus_ioremap(s->cap.bus, mem->sys_start,
				  s->cap.map_size);
    }
}

int read_cis_mem(socket_info_t *s, int attr, u_int addr,
		 u_int len, void *ptr)
{
    pccard_mem_map *mem = &s->cis_mem;
    u_char *sys, *buf = ptr;
    
    DEBUG(3, "cs: read_cis_mem(%d, %#x, %u)\n", attr, addr, len);
    if (setup_cis_mem(s) != 0) {
	memset(ptr, 0xff, len);
	return -1;
    }
    mem->flags = MAP_ACTIVE | ((cis_width) ? MAP_16BIT : 0);

    if (attr & IS_INDIRECT) {
	/* Indirect accesses use a bunch of special registers at fixed
	   locations in common memory */
	u_char flags = ICTRL0_COMMON|ICTRL0_AUTOINC|ICTRL0_BYTEGRAN;
	if (attr & IS_ATTR) { addr *= 2; flags = ICTRL0_AUTOINC; }
	mem->card_start = 0; mem->flags = MAP_ACTIVE;
	set_cis_map(s, mem);
	sys = s->cis_virt;
	bus_writeb(s->cap.bus, flags, sys+CISREG_ICTRL0);
	bus_writeb(s->cap.bus, addr & 0xff, sys+CISREG_IADDR0);
	bus_writeb(s->cap.bus, (addr>>8) & 0xff, sys+CISREG_IADDR1);
	bus_writeb(s->cap.bus, (addr>>16) & 0xff, sys+CISREG_IADDR2);
	bus_writeb(s->cap.bus, (addr>>24) & 0xff, sys+CISREG_IADDR3);
	for ( ; len > 0; len--, buf++)
	    *buf = bus_readb(s->cap.bus, sys+CISREG_IDATA0);
    } else {
	u_int inc = 1;
	if (attr) { mem->flags |= MAP_ATTRIB; inc++; addr *= 2; }
	sys += (addr & (s->cap.map_size-1));
	mem->card_start = addr & ~(s->cap.map_size-1);
	while (len) {
	    set_cis_map(s, mem);
	    sys = s->cis_virt + (addr & (s->cap.map_size-1));
	    for ( ; len > 0; len--, buf++, sys += inc) {
		if (sys == s->cis_virt+s->cap.map_size) break;
		*buf = bus_readb(s->cap.bus, sys);
	    }
	    mem->card_start += s->cap.map_size;
	    addr = 0;
	}
    }
    DEBUG(3, "cs:  %#2.2x %#2.2x %#2.2x %#2.2x ...\n",
	  *(u_char *)(ptr+0), *(u_char *)(ptr+1),
	  *(u_char *)(ptr+2), *(u_char *)(ptr+3));
    return 0;
}

void write_cis_mem(socket_info_t *s, int attr, u_int addr,
		   u_int len, void *ptr)
{
    pccard_mem_map *mem = &s->cis_mem;
    u_char *sys, *buf = ptr;
    
    DEBUG(3, "cs: write_cis_mem(%d, %#x, %u)\n", attr, addr, len);
    if (setup_cis_mem(s) != 0) return;
    mem->flags = MAP_ACTIVE | ((cis_width) ? MAP_16BIT : 0);

    if (attr & IS_INDIRECT) {
	/* Indirect accesses use a bunch of special registers at fixed
	   locations in common memory */
	u_char flags = ICTRL0_COMMON|ICTRL0_AUTOINC|ICTRL0_BYTEGRAN;
	if (attr & IS_ATTR) { addr *= 2; flags = ICTRL0_AUTOINC; }
	mem->card_start = 0; mem->flags = MAP_ACTIVE;
	set_cis_map(s, mem);
	sys = s->cis_virt;
	bus_writeb(s->cap.bus, flags, sys+CISREG_ICTRL0);
	bus_writeb(s->cap.bus, addr & 0xff, sys+CISREG_IADDR0);
	bus_writeb(s->cap.bus, (addr>>8) & 0xff, sys+CISREG_IADDR1);
	bus_writeb(s->cap.bus, (addr>>16) & 0xff, sys+CISREG_IADDR2);
	bus_writeb(s->cap.bus, (addr>>24) & 0xff, sys+CISREG_IADDR3);
	for ( ; len > 0; len--, buf++)
	    bus_writeb(s->cap.bus, *buf, sys+CISREG_IDATA0);
    } else {
	int inc = 1;
	if (attr & IS_ATTR) { mem->flags |= MAP_ATTRIB; inc++; addr *= 2; }
	mem->card_start = addr & ~(s->cap.map_size-1);
	while (len) {
	    set_cis_map(s, mem);
	    sys = s->cis_virt + (addr & (s->cap.map_size-1));
	    for ( ; len > 0; len--, buf++, sys += inc) {
		if (sys == s->cis_virt+s->cap.map_size) break;
		bus_writeb(s->cap.bus, *buf, sys);
	    }
	    mem->card_start += s->cap.map_size;
	    addr = 0;
	}
    }
}

/*======================================================================

    This is tricky... when we set up CIS memory, we try to validate
    the memory window space allocations.
    
======================================================================*/

/* Scratch pointer to the socket we use for validation */
static socket_info_t *vs = NULL;

/* Validation function for cards with a valid CIS */
static int cis_readable(u_long base)
{
    cisinfo_t info1, info2;
    int ret;
    vs->cis_mem.sys_start = base;
    vs->cis_mem.sys_stop = base+vs->cap.map_size-1;
    vs->cis_virt = bus_ioremap(vs->cap.bus, base, vs->cap.map_size);
    ret = pcmcia_validate_cis(vs->clients, &info1);
    /* invalidate mapping and CIS cache */
    bus_iounmap(vs->cap.bus, vs->cis_virt); vs->cis_used = 0;
    if ((ret != 0) || (info1.Chains == 0))
	return 0;
    vs->cis_mem.sys_start = base+vs->cap.map_size;
    vs->cis_mem.sys_stop = base+2*vs->cap.map_size-1;
    vs->cis_virt = bus_ioremap(vs->cap.bus, base+vs->cap.map_size,
			       vs->cap.map_size);
    ret = pcmcia_validate_cis(vs->clients, &info2);
    bus_iounmap(vs->cap.bus, vs->cis_virt); vs->cis_used = 0;
    return ((ret == 0) && (info1.Chains == info2.Chains));
}

/* Validation function for simple memory cards */
static int checksum(u_long base)
{
    int i, a, b, d;
    vs->cis_mem.sys_start = base;
    vs->cis_mem.sys_stop = base+vs->cap.map_size-1;
    vs->cis_virt = bus_ioremap(vs->cap.bus, base, vs->cap.map_size);
    vs->cis_mem.card_start = 0;
    vs->cis_mem.flags = MAP_ACTIVE;
    vs->ss_entry->set_mem_map(vs->sock, &vs->cis_mem);
    /* Don't bother checking every word... */
    a = 0; b = -1;
    for (i = 0; i < vs->cap.map_size; i += 44) {
	d = bus_readl(vs->cap.bus, vs->cis_virt+i);
	a += d; b &= d;
    }
    bus_iounmap(vs->cap.bus, vs->cis_virt);
    return (b == -1) ? -1 : (a>>1);
}

static int checksum_match(u_long base)
{
    int a = checksum(base), b = checksum(base+vs->cap.map_size);
    return ((a == b) && (a >= 0));
}

static int setup_cis_mem(socket_info_t *s)
{
    if (!(s->cap.features & SS_CAP_STATIC_MAP) &&
	(s->cis_mem.sys_start == 0)) {
	int low = !(s->cap.features & SS_CAP_PAGE_REGS);
	vs = s;
	validate_mem(cis_readable, checksum_match, low, s);
	s->cis_mem.sys_start = 0;
	vs = NULL;
	if (find_mem_region(&s->cis_mem.sys_start, s->cap.map_size,
			    s->cap.map_size, low, "card services", s)) {
	    printk(KERN_NOTICE "cs: unable to map card memory!\n");
	    return -1;
	}
	s->cis_mem.sys_stop = s->cis_mem.sys_start+s->cap.map_size-1;
	s->cis_virt = bus_ioremap(s->cap.bus, s->cis_mem.sys_start,
				  s->cap.map_size);
    }
    return 0;
}

void release_cis_mem(socket_info_t *s)
{
    if (s->cis_mem.sys_start != 0) {
	s->cis_mem.flags &= ~MAP_ACTIVE;
	s->ss_entry->set_mem_map(s->sock, &s->cis_mem);
	if (!(s->cap.features & SS_CAP_STATIC_MAP))
	    release_mem_region(s->cis_mem.sys_start, s->cap.map_size);
	bus_iounmap(s->cap.bus, s->cis_virt);
	s->cis_mem.sys_start = 0;
	s->cis_virt = NULL;
    }
}

/*======================================================================

    This is a wrapper around read_cis_mem, with the same interface,
    but which caches information, for cards whose CIS may not be
    readable all the time.
    
======================================================================*/

static void read_cis_cache(socket_info_t *s, int attr, u_int addr,
			   u_int len, void *ptr)
{
    int i, ret;
    char *caddr;

    if (s->fake_cis) {
	if (s->fake_cis_len > addr+len)
	    memcpy(ptr, s->fake_cis+addr, len);
	else
	    memset(ptr, 0xff, len);
	return;
    }
    caddr = s->cis_cache;
    for (i = 0; i < s->cis_used; i++) {
	if ((s->cis_table[i].addr == addr) &&
	    (s->cis_table[i].len == len) &&
	    (s->cis_table[i].attr == attr)) break;
	caddr += s->cis_table[i].len;
    }
    if (i < s->cis_used) {
	memcpy(ptr, caddr, len);
	return;
    }
#ifdef CONFIG_CARDBUS
    if (s->state & SOCKET_CARDBUS)
	ret = read_cb_mem(s, 0, attr, addr, len, ptr);
    else
#endif
	ret = read_cis_mem(s, attr, addr, len, ptr);
    /* Copy data into the cache, if there is room */
    if ((ret == 0) && (i < MAX_CIS_TABLE) &&
	(caddr+len < s->cis_cache+MAX_CIS_DATA)) {
	s->cis_table[i].addr = addr;
	s->cis_table[i].len = len;
	s->cis_table[i].attr = attr;
	s->cis_used++;
	memcpy(caddr, ptr, len);
    }	    
}

/*======================================================================

    This verifies if the CIS of a card matches what is in the CIS
    cache.
    
======================================================================*/

int verify_cis_cache(socket_info_t *s)
{
    char *buf, *caddr;
    int i;

    buf = kmalloc(256, GFP_KERNEL);
    if (buf == NULL)
	return -1;
    caddr = s->cis_cache;
    for (i = 0; i < s->cis_used; i++) {
#ifdef CONFIG_CARDBUS
	if (s->state & SOCKET_CARDBUS)
	    read_cb_mem(s, 0, s->cis_table[i].attr, s->cis_table[i].addr,
			s->cis_table[i].len, buf);
	else
#endif
	    read_cis_mem(s, s->cis_table[i].attr, s->cis_table[i].addr,
			 s->cis_table[i].len, buf);
	if (memcmp(buf, caddr, s->cis_table[i].len) != 0)
	    break;
	caddr += s->cis_table[i].len;
    }
    kfree(buf);
    return (i < s->cis_used);
}

/*======================================================================

    For really bad cards, we provide a facility for uploading a
    replacement CIS.
    
======================================================================*/

int pcmcia_replace_cis(client_handle_t handle, cisdump_t *cis)
{
    socket_info_t *s;
    if (CHECK_HANDLE(handle))
	return CS_BAD_HANDLE;
    s = SOCKET(handle);
    if (s->fake_cis != NULL) {
	kfree(s->fake_cis);
	s->fake_cis = NULL;
    }
    if (cis->Length > CISTPL_MAX_CIS_SIZE)
	return CS_BAD_SIZE;
    s->fake_cis = kmalloc(cis->Length, GFP_KERNEL);
    if (s->fake_cis == NULL)
	return CS_OUT_OF_RESOURCE;
    s->fake_cis_len = cis->Length;
    memcpy(s->fake_cis, cis->Data, cis->Length);
    return CS_SUCCESS;
}

/*======================================================================

    The high-level CIS tuple services
    
======================================================================*/

typedef struct tuple_flags {
    u_int		link_space:4;
    u_int		has_link:1;
    u_int		mfc_fn:3;
    u_int		space:4;
} tuple_flags;

#define LINK_SPACE(f)	(((tuple_flags *)(&(f)))->link_space)
#define HAS_LINK(f)	(((tuple_flags *)(&(f)))->has_link)
#define MFC_FN(f)	(((tuple_flags *)(&(f)))->mfc_fn)
#define SPACE(f)	(((tuple_flags *)(&(f)))->space)

int pcmcia_get_next_tuple(client_handle_t handle, tuple_t *tuple);

int pcmcia_get_first_tuple(client_handle_t handle, tuple_t *tuple)
{
    socket_info_t *s;
    if (CHECK_HANDLE(handle))
	return CS_BAD_HANDLE;
    s = SOCKET(handle);
    if (!(s->state & SOCKET_PRESENT))
	return CS_NO_CARD;
    tuple->TupleLink = tuple->Flags = 0;
#ifdef CONFIG_CARDBUS
    if (s->state & SOCKET_CARDBUS) {
	u_int ptr;
	pcibios_read_config_dword(s->cap.cb_dev->subordinate->number, 0, 0x28, &ptr);
	tuple->CISOffset = ptr & ~7;
	SPACE(tuple->Flags) = (ptr & 7);
    } else
#endif
    {
	/* Assume presence of a LONGLINK_C to address 0 */
	tuple->CISOffset = tuple->LinkOffset = 0;
	SPACE(tuple->Flags) = HAS_LINK(tuple->Flags) = 1;
    }
    if (!(s->state & SOCKET_CARDBUS) && (s->functions > 1) &&
	!(tuple->Attributes & TUPLE_RETURN_COMMON)) {
	cisdata_t req = tuple->DesiredTuple;
	tuple->DesiredTuple = CISTPL_LONGLINK_MFC;
	if (pcmcia_get_next_tuple(handle, tuple) == CS_SUCCESS) {
	    tuple->DesiredTuple = CISTPL_LINKTARGET;
	    if (pcmcia_get_next_tuple(handle, tuple) != CS_SUCCESS)
		return CS_NO_MORE_ITEMS;
	} else
	    tuple->CISOffset = tuple->TupleLink = 0;
	tuple->DesiredTuple = req;
    }
    return pcmcia_get_next_tuple(handle, tuple);
}

static int follow_link(socket_info_t *s, tuple_t *tuple)
{
    u_char link[5];
    u_int ofs;

    if (MFC_FN(tuple->Flags)) {
	/* Get indirect link from the MFC tuple */
	read_cis_cache(s, LINK_SPACE(tuple->Flags),
		       tuple->LinkOffset, 5, link);
	ofs = le32_to_cpu(*(u_int *)(link+1));
	SPACE(tuple->Flags) = (link[0] == CISTPL_MFC_ATTR);
	/* Move to the next indirect link */
	tuple->LinkOffset += 5;
	MFC_FN(tuple->Flags)--;
    } else if (HAS_LINK(tuple->Flags)) {
	ofs = tuple->LinkOffset;
	SPACE(tuple->Flags) = LINK_SPACE(tuple->Flags);
	HAS_LINK(tuple->Flags) = 0;
    } else {
	return -1;
    }
    if (!(s->state & SOCKET_CARDBUS) && SPACE(tuple->Flags)) {
	/* This is ugly, but a common CIS error is to code the long
	   link offset incorrectly, so we check the right spot... */
	read_cis_cache(s, SPACE(tuple->Flags), ofs, 5, link);
	if ((link[0] == CISTPL_LINKTARGET) && (link[1] >= 3) &&
	    (strncmp(link+2, "CIS", 3) == 0))
	    return ofs;
	/* Then, we try the wrong spot... */
	ofs = ofs >> 1;
    }
    read_cis_cache(s, SPACE(tuple->Flags), ofs, 5, link);
    if ((link[0] != CISTPL_LINKTARGET) || (link[1] < 3) ||
	(strncmp(link+2, "CIS", 3) != 0))
	return -1;
    return ofs;
}

int pcmcia_get_next_tuple(client_handle_t handle, tuple_t *tuple)
{
    socket_info_t *s;
    u_char link[2], tmp;
    int ofs, i, attr;
    
    if (CHECK_HANDLE(handle))
	return CS_BAD_HANDLE;
    s = SOCKET(handle);
    if (!(s->state & SOCKET_PRESENT))
	return CS_NO_CARD;

    link[1] = tuple->TupleLink;
    ofs = tuple->CISOffset + tuple->TupleLink;
    attr = SPACE(tuple->Flags);

    for (i = 0; i < MAX_TUPLES; i++) {
	if (link[1] == 0xff) {
	    link[0] = CISTPL_END;
	} else {
	    read_cis_cache(s, attr, ofs, 2, link);
	    if (link[0] == CISTPL_NULL) {
		ofs++; continue;
	    }
	}
	
	/* End of chain?  Follow long link if possible */
	if (link[0] == CISTPL_END) {
	    if ((ofs = follow_link(s, tuple)) < 0)
		return CS_NO_MORE_ITEMS;
	    attr = SPACE(tuple->Flags);
	    read_cis_cache(s, attr, ofs, 2, link);
	}

	/* Is this a link tuple?  Make a note of it */
	if ((link[0] == CISTPL_LONGLINK_A) ||
	    (link[0] == CISTPL_LONGLINK_C) ||
	    (link[0] == CISTPL_LONGLINK_MFC) ||
	    (link[0] == CISTPL_LINKTARGET) ||
	    (link[0] == CISTPL_INDIRECT) ||
	    (link[0] == CISTPL_NO_LINK)) {
	    switch (link[0]) {
	    case CISTPL_LONGLINK_A:
		HAS_LINK(tuple->Flags) = 1;
		LINK_SPACE(tuple->Flags) = attr | IS_ATTR;
		read_cis_cache(s, attr, ofs+2, 4, &tuple->LinkOffset);
		break;
	    case CISTPL_LONGLINK_C:
		HAS_LINK(tuple->Flags) = 1;
		LINK_SPACE(tuple->Flags) = attr & ~IS_ATTR;
		read_cis_cache(s, attr, ofs+2, 4, &tuple->LinkOffset);
		break;
	    case CISTPL_INDIRECT:
		HAS_LINK(tuple->Flags) = 1;
		LINK_SPACE(tuple->Flags) = IS_ATTR | IS_INDIRECT;
		tuple->LinkOffset = 0;
		break;
	    case CISTPL_LONGLINK_MFC:
		tuple->LinkOffset = ofs + 3;
		LINK_SPACE(tuple->Flags) = attr;
		if (handle->Function == BIND_FN_ALL) {
		    /* Follow all the MFC links */
		    read_cis_cache(s, attr, ofs+2, 1, &tmp);
		    MFC_FN(tuple->Flags) = tmp;
		} else {
		    /* Follow exactly one of the links */
		    MFC_FN(tuple->Flags) = 1;
		    tuple->LinkOffset += handle->Function * 5;
		}
		break;
	    case CISTPL_NO_LINK:
		HAS_LINK(tuple->Flags) = 0;
		break;
	    }
	    if ((tuple->Attributes & TUPLE_RETURN_LINK) &&
		(tuple->DesiredTuple == RETURN_FIRST_TUPLE))
		break;
	} else
	    if (tuple->DesiredTuple == RETURN_FIRST_TUPLE)
		break;
	
	if (link[0] == tuple->DesiredTuple)
	    break;
	ofs += link[1] + 2;
    }
    if (i == MAX_TUPLES) {
	DEBUG(1, "cs: overrun in pcmcia_get_next_tuple for socket %d\n",
	      handle->Socket);
	return CS_NO_MORE_ITEMS;
    }
    
    tuple->TupleCode = link[0];
    tuple->TupleLink = link[1];
    tuple->CISOffset = ofs + 2;
    return CS_SUCCESS;
}

/*====================================================================*/

#define _MIN(a, b)		(((a) < (b)) ? (a) : (b))

int pcmcia_get_tuple_data(client_handle_t handle, tuple_t *tuple)
{
    socket_info_t *s;
    u_int len;
    
    if (CHECK_HANDLE(handle))
	return CS_BAD_HANDLE;

    s = SOCKET(handle);

    if (tuple->TupleLink < tuple->TupleOffset)
	return CS_NO_MORE_ITEMS;
    len = tuple->TupleLink - tuple->TupleOffset;
    tuple->TupleDataLen = tuple->TupleLink;
    if (len == 0)
	return CS_SUCCESS;
    read_cis_cache(s, SPACE(tuple->Flags),
		   tuple->CISOffset + tuple->TupleOffset,
		   _MIN(len, tuple->TupleDataMax), tuple->TupleData);
    return CS_SUCCESS;
}

/*======================================================================

    Parsing routines for individual tuples
    
======================================================================*/

static int parse_device(tuple_t *tuple, cistpl_device_t *device)
{
    int i;
    u_char scale;
    u_char *p, *q;

    p = (u_char *)tuple->TupleData;
    q = p + tuple->TupleDataLen;

    device->ndev = 0;
    for (i = 0; i < CISTPL_MAX_DEVICES; i++) {
	
	if (*p == 0xff) break;
	device->dev[i].type = (*p >> 4);
	device->dev[i].wp = (*p & 0x08) ? 1 : 0;
	switch (*p & 0x07) {
	case 0: device->dev[i].speed = 0;   break;
	case 1: device->dev[i].speed = 250; break;
	case 2: device->dev[i].speed = 200; break;
	case 3: device->dev[i].speed = 150; break;
	case 4: device->dev[i].speed = 100; break;
	case 7:
	    if (++p == q) return CS_BAD_TUPLE;
	    device->dev[i].speed = SPEED_CVT(*p);
	    while (*p & 0x80)
		if (++p == q) return CS_BAD_TUPLE;
	    break;
	default:
	    return CS_BAD_TUPLE;
	}

	if (++p == q) return CS_BAD_TUPLE;
	if (*p == 0xff) break;
	scale = *p & 7;
	if (scale == 7) return CS_BAD_TUPLE;
	device->dev[i].size = ((*p >> 3) + 1) * (512 << (scale*2));
	device->ndev++;
	if (++p == q) break;
    }
    
    return CS_SUCCESS;
}

/*====================================================================*/

static int parse_checksum(tuple_t *tuple, cistpl_checksum_t *csum)
{
    u_char *p;
    if (tuple->TupleDataLen < 5)
	return CS_BAD_TUPLE;
    p = (u_char *)tuple->TupleData;
    csum->addr = tuple->CISOffset+(short)le16_to_cpu(*(u_short *)p)-2;
    csum->len = le16_to_cpu(*(u_short *)(p + 2));
    csum->sum = *(p+4);
    return CS_SUCCESS;
}

/*====================================================================*/

static int parse_longlink(tuple_t *tuple, cistpl_longlink_t *link)
{
    if (tuple->TupleDataLen < 4)
	return CS_BAD_TUPLE;
    link->addr = le32_to_cpu(*(u_int *)tuple->TupleData);
    return CS_SUCCESS;
}

/*====================================================================*/

static int parse_longlink_mfc(tuple_t *tuple,
			      cistpl_longlink_mfc_t *link)
{
    u_char *p;
    int i;
    
    p = (u_char *)tuple->TupleData;
    
    link->nfn = *p; p++;
    if (tuple->TupleDataLen <= link->nfn*5)
	return CS_BAD_TUPLE;
    for (i = 0; i < link->nfn; i++) {
	link->fn[i].space = *p; p++;
	link->fn[i].addr = le32_to_cpu(*(u_int *)p); p += 4;
    }
    return CS_SUCCESS;
}

/*====================================================================*/

static int parse_strings(u_char *p, u_char *q, int max,
			 char *s, u_char *ofs, u_char *found)
{
    int i, j, ns;

    if (p == q) return CS_BAD_TUPLE;
    ns = 0; j = 0;
    for (i = 0; i < max; i++) {
	if (*p == 0xff) break;
	ofs[i] = j;
	ns++;
	for (;;) {
	    s[j++] = (*p == 0xff) ? '\0' : *p;
	    if ((*p == '\0') || (*p == 0xff)) break;
	    if (++p == q) return CS_BAD_TUPLE;
	}
	if ((*p == 0xff) || (++p == q)) break;
    }
    if (found) {
	*found = ns;
	return CS_SUCCESS;
    } else {
	return (ns == max) ? CS_SUCCESS : CS_BAD_TUPLE;
    }
}

/*====================================================================*/

static int parse_vers_1(tuple_t *tuple, cistpl_vers_1_t *vers_1)
{
    u_char *p, *q;
    
    p = (u_char *)tuple->TupleData;
    q = p + tuple->TupleDataLen;
    
    vers_1->major = *p; p++;
    vers_1->minor = *p; p++;
    if (p >= q) return CS_BAD_TUPLE;

    return parse_strings(p, q, CISTPL_VERS_1_MAX_PROD_STRINGS,
			 vers_1->str, vers_1->ofs, &vers_1->ns);
}

/*====================================================================*/

static int parse_altstr(tuple_t *tuple, cistpl_altstr_t *altstr)
{
    u_char *p, *q;
    
    p = (u_char *)tuple->TupleData;
    q = p + tuple->TupleDataLen;
    
    return parse_strings(p, q, CISTPL_MAX_ALTSTR_STRINGS,
			 altstr->str, altstr->ofs, &altstr->ns);
}

/*====================================================================*/

static int parse_jedec(tuple_t *tuple, cistpl_jedec_t *jedec)
{
    u_char *p, *q;
    int nid;

    p = (u_char *)tuple->TupleData;
    q = p + tuple->TupleDataLen;

    for (nid = 0; nid < CISTPL_MAX_DEVICES; nid++) {
	if (p > q-2) break;
	jedec->id[nid].mfr = p[0];
	jedec->id[nid].info = p[1];
	p += 2;
    }
    jedec->nid = nid;
    return CS_SUCCESS;
}

/*====================================================================*/

static int parse_manfid(tuple_t *tuple, cistpl_manfid_t *m)
{
    u_short *p;
    if (tuple->TupleDataLen < 4)
	return CS_BAD_TUPLE;
    p = (u_short *)tuple->TupleData;
    m->manf = le16_to_cpu(p[0]);
    m->card = le16_to_cpu(p[1]);
    return CS_SUCCESS;
}

/*====================================================================*/

static int parse_funcid(tuple_t *tuple, cistpl_funcid_t *f)
{
    u_char *p;
    if (tuple->TupleDataLen < 2)
	return CS_BAD_TUPLE;
    p = (u_char *)tuple->TupleData;
    f->func = p[0];
    f->sysinit = p[1];
    return CS_SUCCESS;
}

/*====================================================================*/

static int parse_funce(tuple_t *tuple, cistpl_funce_t *f)
{
    u_char *p;
    int i;
    if (tuple->TupleDataLen < 1)
	return CS_BAD_TUPLE;
    p = (u_char *)tuple->TupleData;
    f->type = p[0];
    for (i = 1; i < tuple->TupleDataLen; i++)
	f->data[i-1] = p[i];
    return CS_SUCCESS;
}

/*====================================================================*/

static int parse_config(tuple_t *tuple, cistpl_config_t *config)
{
    int rasz, rmsz, i;
    u_char *p;

    p = (u_char *)tuple->TupleData;
    rasz = *p & 0x03;
    rmsz = (*p & 0x3c) >> 2;
    if (tuple->TupleDataLen < rasz+rmsz+4)
	return CS_BAD_TUPLE;
    config->last_idx = *(++p);
    p++;
    config->base = 0;
    for (i = 0; i <= rasz; i++)
	config->base += p[i] << (8*i);
    p += rasz+1;
    for (i = 0; i < 4; i++)
	config->rmask[i] = 0;
    for (i = 0; i <= rmsz; i++)
	config->rmask[i>>2] += p[i] << (8*(i%4));
    config->subtuples = tuple->TupleDataLen - (rasz+rmsz+4);
    return CS_SUCCESS;
}

/*======================================================================

    The following routines are all used to parse the nightmarish
    config table entries.
    
======================================================================*/

static u_char *parse_power(u_char *p, u_char *q,
			   cistpl_power_t *pwr)
{
    int i;
    u_int scale;

    if (p == q) return NULL;
    pwr->present = *p;
    pwr->flags = 0;
    p++;
    for (i = 0; i < 7; i++)
	if (pwr->present & (1<<i)) {
	    if (p == q) return NULL;
	    pwr->param[i] = POWER_CVT(*p);
	    scale = POWER_SCALE(*p);
	    while (*p & 0x80) {
		if (++p == q) return NULL;
		if ((*p & 0x7f) < 100)
		    pwr->param[i] += (*p & 0x7f) * scale / 100;
		else if (*p == 0x7d)
		    pwr->flags |= CISTPL_POWER_HIGHZ_OK;
		else if (*p == 0x7e)
		    pwr->param[i] = 0;
		else if (*p == 0x7f)
		    pwr->flags |= CISTPL_POWER_HIGHZ_REQ;
		else
		    return NULL;
	    }
	    p++;
	}
    return p;
}

/*====================================================================*/

static u_char *parse_timing(u_char *p, u_char *q,
			    cistpl_timing_t *timing)
{
    u_char scale;

    if (p == q) return NULL;
    scale = *p;
    if ((scale & 3) != 3) {
	if (++p == q) return NULL;
	timing->wait = SPEED_CVT(*p);
	timing->waitscale = exponent[scale & 3];
    } else
	timing->wait = 0;
    scale >>= 2;
    if ((scale & 7) != 7) {
	if (++p == q) return NULL;
	timing->ready = SPEED_CVT(*p);
	timing->rdyscale = exponent[scale & 7];
    } else
	timing->ready = 0;
    scale >>= 3;
    if (scale != 7) {
	if (++p == q) return NULL;
	timing->reserved = SPEED_CVT(*p);
	timing->rsvscale = exponent[scale];
    } else
	timing->reserved = 0;
    p++;
    return p;
}

/*====================================================================*/

static u_char *parse_io(u_char *p, u_char *q, cistpl_io_t *io)
{
    int i, j, bsz, lsz;

    if (p == q) return NULL;
    io->flags = *p;

    if (!(*p & 0x80)) {
	io->nwin = 1;
	io->win[0].base = 0;
	io->win[0].len = (1 << (io->flags & CISTPL_IO_LINES_MASK));
	return p+1;
    }
    
    if (++p == q) return NULL;
    io->nwin = (*p & 0x0f) + 1;
    bsz = (*p & 0x30) >> 4;
    if (bsz == 3) bsz++;
    lsz = (*p & 0xc0) >> 6;
    if (lsz == 3) lsz++;
    p++;
    
    for (i = 0; i < io->nwin; i++) {
	io->win[i].base = 0;
	io->win[i].len = 1;
	for (j = 0; j < bsz; j++, p++) {
	    if (p == q) return NULL;
	    io->win[i].base += *p << (j*8);
	}
	for (j = 0; j < lsz; j++, p++) {
	    if (p == q) return NULL;
	    io->win[i].len += *p << (j*8);
	}
    }
    return p;
}

/*====================================================================*/

static u_char *parse_mem(u_char *p, u_char *q, cistpl_mem_t *mem)
{
    int i, j, asz, lsz, has_ha;
    u_int len, ca, ha;

    if (p == q) return NULL;

    mem->nwin = (*p & 0x07) + 1;
    lsz = (*p & 0x18) >> 3;
    asz = (*p & 0x60) >> 5;
    has_ha = (*p & 0x80);
    if (++p == q) return NULL;
    
    for (i = 0; i < mem->nwin; i++) {
	len = ca = ha = 0;
	for (j = 0; j < lsz; j++, p++) {
	    if (p == q) return NULL;
	    len += *p << (j*8);
	}
	for (j = 0; j < asz; j++, p++) {
	    if (p == q) return NULL;
	    ca += *p << (j*8);
	}
	if (has_ha)
	    for (j = 0; j < asz; j++, p++) {
		if (p == q) return NULL;
		ha += *p << (j*8);
	    }
	mem->win[i].len = len << 8;
	mem->win[i].card_addr = ca << 8;
	mem->win[i].host_addr = ha << 8;
    }
    return p;
}

/*====================================================================*/

static u_char *parse_irq(u_char *p, u_char *q, cistpl_irq_t *irq)
{
    if (p == q) return NULL;
    irq->IRQInfo1 = *p; p++;
    if (irq->IRQInfo1 & IRQ_INFO2_VALID) {
	if (p+2 > q) return NULL;
	irq->IRQInfo2 = (p[1]<<8) + p[0];
	p += 2;
    }
    return p;
}

/*====================================================================*/

static int parse_cftable_entry(tuple_t *tuple,
			       cistpl_cftable_entry_t *entry)
{
    u_char *p, *q, features;

    p = tuple->TupleData;
    q = p + tuple->TupleDataLen;
    entry->index = *p & 0x3f;
    entry->flags = 0;
    if (*p & 0x40)
	entry->flags |= CISTPL_CFTABLE_DEFAULT;
    if (*p & 0x80) {
	if (++p == q) return CS_BAD_TUPLE;
	if (*p & 0x10)
	    entry->flags |= CISTPL_CFTABLE_BVDS;
	if (*p & 0x20)
	    entry->flags |= CISTPL_CFTABLE_WP;
	if (*p & 0x40)
	    entry->flags |= CISTPL_CFTABLE_RDYBSY;
	if (*p & 0x80)
	    entry->flags |= CISTPL_CFTABLE_MWAIT;
	entry->interface = *p & 0x0f;
    } else
	entry->interface = 0;

    /* Process optional features */
    if (++p == q) return CS_BAD_TUPLE;
    features = *p; p++;

    /* Power options */
    if ((features & 3) > 0) {
	p = parse_power(p, q, &entry->vcc);
	if (p == NULL) return CS_BAD_TUPLE;
    } else
	entry->vcc.present = 0;
    if ((features & 3) > 1) {
	p = parse_power(p, q, &entry->vpp1);
	if (p == NULL) return CS_BAD_TUPLE;
    } else
	entry->vpp1.present = 0;
    if ((features & 3) > 2) {
	p = parse_power(p, q, &entry->vpp2);
	if (p == NULL) return CS_BAD_TUPLE;
    } else
	entry->vpp2.present = 0;

    /* Timing options */
    if (features & 0x04) {
	p = parse_timing(p, q, &entry->timing);
	if (p == NULL) return CS_BAD_TUPLE;
    } else {
	entry->timing.wait = 0;
	entry->timing.ready = 0;
	entry->timing.reserved = 0;
    }
    
    /* I/O window options */
    if (features & 0x08) {
	p = parse_io(p, q, &entry->io);
	if (p == NULL) return CS_BAD_TUPLE;
    } else
	entry->io.nwin = 0;
    
    /* Interrupt options */
    if (features & 0x10) {
	p = parse_irq(p, q, &entry->irq);
	if (p == NULL) return CS_BAD_TUPLE;
    } else
	entry->irq.IRQInfo1 = 0;

    switch (features & 0x60) {
    case 0x00:
	entry->mem.nwin = 0;
	break;
    case 0x20:
	entry->mem.nwin = 1;
	entry->mem.win[0].len = le16_to_cpu(*(u_short *)p) << 8;
	entry->mem.win[0].card_addr = 0;
	entry->mem.win[0].host_addr = 0;
	p += 2;
	if (p > q) return CS_BAD_TUPLE;
	break;
    case 0x40:
	entry->mem.nwin = 1;
	entry->mem.win[0].len = le16_to_cpu(*(u_short *)p) << 8;
	entry->mem.win[0].card_addr =
	    le16_to_cpu(*(u_short *)(p+2)) << 8;
	entry->mem.win[0].host_addr = 0;
	p += 4;
	if (p > q) return CS_BAD_TUPLE;
	break;
    case 0x60:
	p = parse_mem(p, q, &entry->mem);
	if (p == NULL) return CS_BAD_TUPLE;
	break;
    }

    /* Misc features */
    if (features & 0x80) {
	if (p == q) return CS_BAD_TUPLE;
	entry->flags |= (*p << 8);
	while (*p & 0x80)
	    if (++p == q) return CS_BAD_TUPLE;
	p++;
    }

    entry->subtuples = q-p;
    
    return CS_SUCCESS;
}

/*====================================================================*/

#ifdef CONFIG_CARDBUS

static int parse_bar(tuple_t *tuple, cistpl_bar_t *bar)
{
    u_char *p;
    if (tuple->TupleDataLen < 6)
	return CS_BAD_TUPLE;
    p = (u_char *)tuple->TupleData;
    bar->attr = *p;
    p += 2;
    bar->size = le32_to_cpu(*(u_int *)p);
    return CS_SUCCESS;
}

static int parse_config_cb(tuple_t *tuple, cistpl_config_t *config)
{
    u_char *p;
    
    p = (u_char *)tuple->TupleData;
    if ((*p != 3) || (tuple->TupleDataLen < 6))
	return CS_BAD_TUPLE;
    config->last_idx = *(++p);
    p++;
    config->base = le32_to_cpu(*(u_int *)p);
    config->subtuples = tuple->TupleDataLen - 6;
    return CS_SUCCESS;
}

static int parse_cftable_entry_cb(tuple_t *tuple,
				  cistpl_cftable_entry_cb_t *entry)
{
    u_char *p, *q, features;

    p = tuple->TupleData;
    q = p + tuple->TupleDataLen;
    entry->index = *p & 0x3f;
    entry->flags = 0;
    if (*p & 0x40)
	entry->flags |= CISTPL_CFTABLE_DEFAULT;

    /* Process optional features */
    if (++p == q) return CS_BAD_TUPLE;
    features = *p; p++;

    /* Power options */
    if ((features & 3) > 0) {
	p = parse_power(p, q, &entry->vcc);
	if (p == NULL) return CS_BAD_TUPLE;
    } else
	entry->vcc.present = 0;
    if ((features & 3) > 1) {
	p = parse_power(p, q, &entry->vpp1);
	if (p == NULL) return CS_BAD_TUPLE;
    } else
	entry->vpp1.present = 0;
    if ((features & 3) > 2) {
	p = parse_power(p, q, &entry->vpp2);
	if (p == NULL) return CS_BAD_TUPLE;
    } else
	entry->vpp2.present = 0;

    /* I/O window options */
    if (features & 0x08) {
	if (p == q) return CS_BAD_TUPLE;
	entry->io = *p; p++;
    } else
	entry->io = 0;
    
    /* Interrupt options */
    if (features & 0x10) {
	p = parse_irq(p, q, &entry->irq);
	if (p == NULL) return CS_BAD_TUPLE;
    } else
	entry->irq.IRQInfo1 = 0;

    if (features & 0x20) {
	if (p == q) return CS_BAD_TUPLE;
	entry->mem = *p; p++;
    } else
	entry->mem = 0;

    /* Misc features */
    if (features & 0x80) {
	if (p == q) return CS_BAD_TUPLE;
	entry->flags |= (*p << 8);
	if (*p & 0x80) {
	    if (++p == q) return CS_BAD_TUPLE;
	    entry->flags |= (*p << 16);
	}
	while (*p & 0x80)
	    if (++p == q) return CS_BAD_TUPLE;
	p++;
    }

    entry->subtuples = q-p;
    
    return CS_SUCCESS;
}

#endif

/*====================================================================*/

static int parse_device_geo(tuple_t *tuple, cistpl_device_geo_t *geo)
{
    u_char *p, *q;
    int n;

    p = (u_char *)tuple->TupleData;
    q = p + tuple->TupleDataLen;

    for (n = 0; n < CISTPL_MAX_DEVICES; n++) {
	if (p > q-6) break;
	geo->geo[n].buswidth = p[0];
	geo->geo[n].erase_block = 1 << (p[1]-1);
	geo->geo[n].read_block  = 1 << (p[2]-1);
	geo->geo[n].write_block = 1 << (p[3]-1);
	geo->geo[n].partition   = 1 << (p[4]-1);
	geo->geo[n].interleave  = 1 << (p[5]-1);
	p += 6;
    }
    geo->ngeo = n;
    return CS_SUCCESS;
}

/*====================================================================*/

static int parse_vers_2(tuple_t *tuple, cistpl_vers_2_t *v2)
{
    u_char *p, *q;

    if (tuple->TupleDataLen < 10)
	return CS_BAD_TUPLE;
    
    p = tuple->TupleData;
    q = p + tuple->TupleDataLen;

    v2->vers = p[0];
    v2->comply = p[1];
    v2->dindex = le16_to_cpu(*(u_short *)(p+2));
    v2->vspec8 = p[6];
    v2->vspec9 = p[7];
    v2->nhdr = p[8];
    p += 9;
    return parse_strings(p, q, 2, v2->str, &v2->vendor, NULL);
}

/*====================================================================*/

static int parse_org(tuple_t *tuple, cistpl_org_t *org)
{
    u_char *p, *q;
    int i;
    
    p = tuple->TupleData;
    q = p + tuple->TupleDataLen;
    if (p == q) return CS_BAD_TUPLE;
    org->data_org = *p;
    if (++p == q) return CS_BAD_TUPLE;
    for (i = 0; i < 30; i++) {
	org->desc[i] = *p;
	if (*p == '\0') break;
	if (++p == q) return CS_BAD_TUPLE;
    }
    return CS_SUCCESS;
}

/*====================================================================*/

static int parse_format(tuple_t *tuple, cistpl_format_t *fmt)
{
    u_char *p;

    if (tuple->TupleDataLen < 10)
	return CS_BAD_TUPLE;

    p = tuple->TupleData;

    fmt->type = p[0];
    fmt->edc = p[1];
    fmt->offset = le32_to_cpu(*(u_int *)(p+2));
    fmt->length = le32_to_cpu(*(u_int *)(p+6));

    return CS_SUCCESS;
}

/*====================================================================*/

int pcmcia_parse_tuple(client_handle_t handle, tuple_t *tuple, cisparse_t *parse)
{
    int ret = CS_SUCCESS;
    
    if (tuple->TupleDataLen > tuple->TupleDataMax)
	return CS_BAD_TUPLE;
    switch (tuple->TupleCode) {
    case CISTPL_DEVICE:
    case CISTPL_DEVICE_A:
	ret = parse_device(tuple, &parse->device);
	break;
#ifdef CONFIG_CARDBUS
    case CISTPL_BAR:
	ret = parse_bar(tuple, &parse->bar);
	break;
    case CISTPL_CONFIG_CB:
	ret = parse_config_cb(tuple, &parse->config);
	break;
    case CISTPL_CFTABLE_ENTRY_CB:
	ret = parse_cftable_entry_cb(tuple, &parse->cftable_entry_cb);
	break;
#endif
    case CISTPL_CHECKSUM:
	ret = parse_checksum(tuple, &parse->checksum);
	break;
    case CISTPL_LONGLINK_A:
    case CISTPL_LONGLINK_C:
	ret = parse_longlink(tuple, &parse->longlink);
	break;
    case CISTPL_LONGLINK_MFC:
	ret = parse_longlink_mfc(tuple, &parse->longlink_mfc);
	break;
    case CISTPL_VERS_1:
	ret = parse_vers_1(tuple, &parse->version_1);
	break;
    case CISTPL_ALTSTR:
	ret = parse_altstr(tuple, &parse->altstr);
	break;
    case CISTPL_JEDEC_A:
    case CISTPL_JEDEC_C:
	ret = parse_jedec(tuple, &parse->jedec);
	break;
    case CISTPL_MANFID:
	ret = parse_manfid(tuple, &parse->manfid);
	break;
    case CISTPL_FUNCID:
	ret = parse_funcid(tuple, &parse->funcid);
	break;
    case CISTPL_FUNCE:
	ret = parse_funce(tuple, &parse->funce);
	break;
    case CISTPL_CONFIG:
	ret = parse_config(tuple, &parse->config);
	break;
    case CISTPL_CFTABLE_ENTRY:
	ret = parse_cftable_entry(tuple, &parse->cftable_entry);
	break;
    case CISTPL_DEVICE_GEO:
    case CISTPL_DEVICE_GEO_A:
	ret = parse_device_geo(tuple, &parse->device_geo);
	break;
    case CISTPL_VERS_2:
	ret = parse_vers_2(tuple, &parse->vers_2);
	break;
    case CISTPL_ORG:
	ret = parse_org(tuple, &parse->org);
	break;
    case CISTPL_FORMAT:
    case CISTPL_FORMAT_A:
	ret = parse_format(tuple, &parse->format);
	break;
    case CISTPL_NO_LINK:
    case CISTPL_LINKTARGET:
	ret = CS_SUCCESS;
	break;
    default:
	ret = CS_UNSUPPORTED_FUNCTION;
	break;
    }
    return ret;
}

/*======================================================================

    This is used internally by Card Services to look up CIS stuff.
    
======================================================================*/

int read_tuple(client_handle_t handle, cisdata_t code, void *parse)
{
    tuple_t *tuple;
    cisdata_t *buf;
    int ret;

    buf = kmalloc(256, GFP_KERNEL);
    if (buf == NULL)
	return CS_OUT_OF_RESOURCE;
    tuple = kmalloc(sizeof(*tuple), GFP_KERNEL);
    if (tuple == NULL) {
	kfree(buf);
	return CS_OUT_OF_RESOURCE;
    }
    tuple->DesiredTuple = code;
    tuple->Attributes = TUPLE_RETURN_COMMON;
    ret = pcmcia_get_first_tuple(handle, tuple);
    if (ret != CS_SUCCESS) goto done;
    tuple->TupleData = buf;
    tuple->TupleOffset = 0;
    tuple->TupleDataMax = 255;
    ret = pcmcia_get_tuple_data(handle, tuple);
    if (ret != CS_SUCCESS) goto done;
    ret = pcmcia_parse_tuple(handle, tuple, parse);
done:
    kfree(tuple);
    kfree(buf);
    return ret;
}

/*======================================================================

    This tries to determine if a card has a sensible CIS.  It returns
    the number of tuples in the CIS, or 0 if the CIS looks bad.  The
    checks include making sure several critical tuples are present and
    valid; seeing if the total number of tuples is reasonable; and
    looking for tuples that use reserved codes.
    
======================================================================*/

int pcmcia_validate_cis(client_handle_t handle, cisinfo_t *info)
{
    tuple_t *tuple;
    cisparse_t *p;
    int ret, reserved, dev_ok = 0, ident_ok = 0;

    if (CHECK_HANDLE(handle))
	return CS_BAD_HANDLE;
    tuple = kmalloc(sizeof(*tuple), GFP_KERNEL);
    if (tuple == NULL)
	return CS_OUT_OF_RESOURCE;
    p = kmalloc(sizeof(*p), GFP_KERNEL);
    if (p == NULL) {
	kfree(tuple);
	return CS_OUT_OF_RESOURCE;
    }

    info->Chains = reserved = 0;
    tuple->DesiredTuple = RETURN_FIRST_TUPLE;
    tuple->Attributes = TUPLE_RETURN_COMMON;
    ret = pcmcia_get_first_tuple(handle, tuple);
    if (ret != CS_SUCCESS)
	goto done;

    /* First tuple should be DEVICE; we should really have either that
       or a CFTABLE_ENTRY of some sort */
    if ((tuple->TupleCode == CISTPL_DEVICE) ||
	(read_tuple(handle, CISTPL_CFTABLE_ENTRY, p) == CS_SUCCESS) ||
	(read_tuple(handle, CISTPL_CFTABLE_ENTRY_CB, p) == CS_SUCCESS))
	dev_ok++;

    /* All cards should have a MANFID tuple, and/or a VERS_1 or VERS_2
       tuple, for card identification.  Certain old D-Link and Linksys
       cards have only a broken VERS_2 tuple; hence the bogus test. */
    if ((read_tuple(handle, CISTPL_MANFID, p) == CS_SUCCESS) ||
	(read_tuple(handle, CISTPL_VERS_1, p) == CS_SUCCESS) ||
	(read_tuple(handle, CISTPL_VERS_2, p) != CS_NO_MORE_ITEMS))
	ident_ok++;

    if (!dev_ok && !ident_ok)
	goto done;

    for (info->Chains = 1; info->Chains < MAX_TUPLES; info->Chains++) {
	ret = pcmcia_get_next_tuple(handle, tuple);
	if (ret != CS_SUCCESS) break;
	if (((tuple->TupleCode > 0x23) && (tuple->TupleCode < 0x40)) ||
	    ((tuple->TupleCode > 0x47) && (tuple->TupleCode < 0x80)) ||
	    ((tuple->TupleCode > 0x90) && (tuple->TupleCode < 0xff)))
	    reserved++;
    }
    if ((info->Chains == MAX_TUPLES) || (reserved > 5) ||
	((!dev_ok || !ident_ok) && (info->Chains > 10)))
	info->Chains = 0;

done:
    kfree(tuple);
    kfree(p);
    return CS_SUCCESS;
}

