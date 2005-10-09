/*-
 * Copyright (c) 1999 FreeBSD(98) Porting Team.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer as
 *    the first lines of this file unmodified.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include "opt_syscons.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/consio.h>
#include <sys/fbio.h>

#include <machine/md_var.h>

#include <dev/fb/fbreg.h>
#include <dev/syscons/syscons.h>

#define ATTR_OFFSET_FB		0x2000
#define attr_offset(vtb)	((vtb)->vtb_size*sizeof(u_int16_t))

#define vtb_pointer(vtb, at)					\
    ((vtb)->vtb_buffer + sizeof(u_int16_t)*(at))

#define vtb_wrap(vtb, at, offset)				\
    (((at) + (offset) + (vtb)->vtb_size)%(vtb)->vtb_size)

static u_int8_t	ibmpc_to_pc98[256] = {
	0x01, 0x21, 0x81, 0xa1, 0x41, 0x61, 0xc1, 0xe1,
	0x09, 0x29, 0x89, 0xa9, 0x49, 0x69, 0xc9, 0xe9,
	0x25, 0x25, 0x25, 0x25, 0x25, 0x25, 0x25, 0x25,
	0x25, 0x25, 0x25, 0x25, 0x25, 0x25, 0x25, 0x25,
	0x85, 0x85, 0x85, 0x85, 0x85, 0x85, 0x85, 0x85,
	0x85, 0x85, 0x85, 0x85, 0x85, 0x85, 0x85, 0x85,
	0xa5, 0xa5, 0xa5, 0xa5, 0xa5, 0xa5, 0xa5, 0xa5,
	0xa5, 0xa5, 0xa5, 0xa5, 0xa5, 0xa5, 0xa5, 0xa5,
	0x45, 0x45, 0x45, 0x45, 0x45, 0x45, 0x45, 0x45,
	0x45, 0x45, 0x45, 0x45, 0x45, 0x45, 0x45, 0x45,
	0x65, 0x65, 0x65, 0x65, 0x65, 0x65, 0x65, 0x65,
	0x65, 0x65, 0x65, 0x65, 0x65, 0x65, 0x65, 0x65,
	0xc5, 0xc5, 0xc5, 0xc5, 0xc5, 0xc5, 0xc5, 0xc5,
	0xc5, 0xc5, 0xc5, 0xc5, 0xc5, 0xc5, 0xc5, 0xc5,
	0xe5, 0xe5, 0xe5, 0xe5, 0xe5, 0xe5, 0xe5, 0xe5,
	0xe5, 0xe5, 0xe5, 0xe5, 0xe5, 0xe5, 0xe5, 0xe5,

	0x03, 0x23, 0x83, 0xa3, 0x43, 0x63, 0xc3, 0xe3,
	0x0b, 0x2b, 0x8b, 0xab, 0x4b, 0x6b, 0xcb, 0xeb,
	0x2f, 0x2f, 0x2f, 0x2f, 0x2f, 0x2f, 0x2f, 0x2f,
	0x2f, 0x2f, 0x2f, 0x2f, 0x2f, 0x2f, 0x2f, 0x2f,
	0x8f, 0x8f, 0x8f, 0x8f, 0x8f, 0x8f, 0x8f, 0x8f,
	0x8f, 0x8f, 0x8f, 0x8f, 0x8f, 0x8f, 0x8f, 0x8f,
	0xaf, 0xaf, 0xaf, 0xaf, 0xaf, 0xaf, 0xaf, 0xaf,
	0xaf, 0xaf, 0xaf, 0xaf, 0xaf, 0xaf, 0xaf, 0xaf,
	0x4f, 0x4f, 0x4f, 0x4f, 0x4f, 0x4f, 0x4f, 0x4f,
	0x4f, 0x4f, 0x4f, 0x4f, 0x4f, 0x4f, 0x4f, 0x4f,
	0x6f, 0x6f, 0x6f, 0x6f, 0x6f, 0x6f, 0x6f, 0x6f,
	0x6f, 0x6f, 0x6f, 0x6f, 0x6f, 0x6f, 0x6f, 0x6f,
	0xcf, 0xcf, 0xcf, 0xcf, 0xcf, 0xcf, 0xcf, 0xcf,
	0xcf, 0xcf, 0xcf, 0xcf, 0xcf, 0xcf, 0xcf, 0xcf,
	0xef, 0xef, 0xef, 0xef, 0xef, 0xef, 0xef, 0xef,
	0xef, 0xef, 0xef, 0xef, 0xef, 0xef, 0xef, 0xef, 
};
#define	at2pc98(attr)	((attr) | ibmpc_to_pc98[(unsigned)(attr) >> 8])

void
sc_vtb_init(sc_vtb_t *vtb, int type, int cols, int rows, void *buf, int wait)
{
	vtb->vtb_flags = 0;
	vtb->vtb_type = type;
	vtb->vtb_cols = cols;
	vtb->vtb_rows = rows;
	vtb->vtb_size = cols*rows;
	vtb->vtb_buffer = 0;
	vtb->vtb_tail = 0;

	switch (type) {
	case VTB_MEMORY:
	case VTB_RINGBUFFER:
		if ((buf == NULL) && (cols*rows != 0)) {
			vtb->vtb_buffer =
			    (vm_offset_t)malloc(cols*rows*sizeof(u_int16_t)*2,
				M_DEVBUF, 
				((wait) ? M_WAITOK : M_NOWAIT) | M_ZERO);
			if (vtb->vtb_buffer != 0) {
				vtb->vtb_flags |= VTB_ALLOCED;
			}
		} else {
			vtb->vtb_buffer = (vm_offset_t)buf;
		}
		vtb->vtb_flags |= VTB_VALID;
		break;
	case VTB_FRAMEBUFFER:
		vtb->vtb_buffer = (vm_offset_t)buf;
		vtb->vtb_flags |= VTB_VALID;
		break;
	default:
		break;
	}
}

void
sc_vtb_destroy(sc_vtb_t *vtb)
{
	vm_offset_t p;

	vtb->vtb_cols = 0;
	vtb->vtb_rows = 0;
	vtb->vtb_size = 0;
	vtb->vtb_tail = 0;

	p = vtb->vtb_buffer;
	vtb->vtb_buffer = 0;
	switch (vtb->vtb_type) {
	case VTB_MEMORY:
	case VTB_RINGBUFFER:
		if ((vtb->vtb_flags & VTB_ALLOCED) && (p != 0))
			free((void *)p, M_DEVBUF);
		break;
	default:
		break;
	}
	vtb->vtb_flags = 0;
	vtb->vtb_type = VTB_INVALID;
}

size_t
sc_vtb_size(int cols, int rows)
{
	return (size_t)(cols*rows*sizeof(u_int16_t)*2);
}

int
sc_vtb_getc(sc_vtb_t *vtb, int at)
{
	vm_offset_t p = vtb_pointer(vtb, at);

	if (vtb->vtb_type == VTB_FRAMEBUFFER)
		return (readw(p) & 0x00ff);
	else
		return (*(u_int16_t *)p & 0x00ff);
}

int
sc_vtb_geta(sc_vtb_t *vtb, int at)
{
	vm_offset_t p = vtb_pointer(vtb, at);

	if (vtb->vtb_type == VTB_FRAMEBUFFER)
		return (readw(p + ATTR_OFFSET_FB) & 0xff00);
	else
		return (*(u_int16_t *)(p + attr_offset(vtb)) & 0xff00);
}

__inline static void
vtb_putc(sc_vtb_t *vtb, vm_offset_t p, int c, int a)
{
	if (vtb->vtb_type == VTB_FRAMEBUFFER) {
		writew(p, c);
		writew(p + ATTR_OFFSET_FB, at2pc98(a));
	} else {
		*(u_int16_t *)p = c;
		*(u_int16_t *)(p + attr_offset(vtb)) = at2pc98(a);
	}
}

void
sc_vtb_putc(sc_vtb_t *vtb, int at, int c, int a)
{
	vtb_putc(vtb, vtb_pointer(vtb, at), c, a);
}

vm_offset_t
sc_vtb_putchar(sc_vtb_t *vtb, vm_offset_t p, int c, int a)
{
	vtb_putc(vtb, p, c, a);
	return (p + sizeof(u_int16_t));
}

vm_offset_t
sc_vtb_pointer(sc_vtb_t *vtb, int at)
{
	return (vtb_pointer(vtb, at));
}

int
sc_vtb_pos(sc_vtb_t *vtb, int pos, int offset)
{
	return ((pos + offset + vtb->vtb_size)%vtb->vtb_size);
}

void
sc_vtb_clear(sc_vtb_t *vtb, int c, int attr)
{
	vm_offset_t p = vtb_pointer(vtb, 0);

	if (vtb->vtb_type == VTB_FRAMEBUFFER) {
		fillw_io(c, p, vtb->vtb_size);
		fillw_io(at2pc98(attr), p + ATTR_OFFSET_FB, vtb->vtb_size);
	} else {
		fillw(c, (void *)p, vtb->vtb_size);
		fillw(at2pc98(attr), (void *)(p + attr_offset(vtb)),
		      vtb->vtb_size);
	}
}

void
sc_vtb_copy(sc_vtb_t *vtb1, int from, sc_vtb_t *vtb2, int to, int count)
{
	vm_offset_t p1, p2;

	p1 = vtb_pointer(vtb1, from);
	p2 = vtb_pointer(vtb2, to);
	if (vtb2->vtb_type == VTB_FRAMEBUFFER) {
		bcopy_toio(p1, p2, count*sizeof(u_int16_t));
		bcopy_toio(p1 + attr_offset(vtb1),
			   p2 + ATTR_OFFSET_FB,
			   count*sizeof(u_int16_t));
	} else if (vtb1->vtb_type == VTB_FRAMEBUFFER) {
		bcopy_fromio(p1, p2, count*sizeof(u_int16_t));
		bcopy_fromio(p1 + ATTR_OFFSET_FB,
			     p2 + attr_offset(vtb2),
			     count*sizeof(u_int16_t));
	} else {
		bcopy((void *)p1, (void *)p2, count*sizeof(u_int16_t));
		bcopy((void *)(p1 + attr_offset(vtb1)),
		      (void *)(p2 + attr_offset(vtb2)),
		      count*sizeof(u_int16_t));
	}
}

void
sc_vtb_append(sc_vtb_t *vtb1, int from, sc_vtb_t *vtb2, int count)
{
	int len;
	vm_offset_t p1, p2;

	if (vtb2->vtb_type != VTB_RINGBUFFER)
		return;

	while (count > 0) {
		p1 = vtb_pointer(vtb1, from);
		p2 = vtb_pointer(vtb2, vtb2->vtb_tail);
		len = imin(count, vtb2->vtb_size - vtb2->vtb_tail);
		if (vtb1->vtb_type == VTB_FRAMEBUFFER) {
			bcopy_fromio(p1, p2, len*sizeof(u_int16_t));
			bcopy_fromio(p1 + ATTR_OFFSET_FB,
				     p2 + attr_offset(vtb2),
				     len*sizeof(u_int16_t));
		} else {
			bcopy((void *)p1, (void *)p2, len*sizeof(u_int16_t));
			bcopy((void *)(p1 + attr_offset(vtb1)),
			      (void *)(p2 + attr_offset(vtb2)),
			      len*sizeof(u_int16_t));
		}
		from += len;
		count -= len;
		vtb2->vtb_tail = vtb_wrap(vtb2, vtb2->vtb_tail, len);
	}
}

void
sc_vtb_seek(sc_vtb_t *vtb, int pos)
{
	vtb->vtb_tail = pos%vtb->vtb_size;
}

void
sc_vtb_erase(sc_vtb_t *vtb, int at, int count, int c, int attr)
{
	vm_offset_t p;

	if (at + count > vtb->vtb_size)
		count = vtb->vtb_size - at;
	p = vtb_pointer(vtb, at);
	if (vtb->vtb_type == VTB_FRAMEBUFFER) {
		fillw_io(c, p, count);
		fillw_io(at2pc98(attr), p + ATTR_OFFSET_FB, count);
	} else {
		fillw(c, (void *)p, count);
		fillw(at2pc98(attr), (void *)(p + attr_offset(vtb)), count);
	}
}

void
sc_vtb_move(sc_vtb_t *vtb, int from, int to, int count)
{
	vm_offset_t p1, p2;

	if (from + count > vtb->vtb_size)
		count = vtb->vtb_size - from;
	if (to + count > vtb->vtb_size)
		count = vtb->vtb_size - to;
	if (count <= 0)
		return;

	p1 = vtb_pointer(vtb, from);
	p2 = vtb_pointer(vtb, to);
	if (vtb->vtb_type == VTB_FRAMEBUFFER) {
		bcopy_io(p1, p2, count*sizeof(u_int16_t)); 
		bcopy_io(p1 + ATTR_OFFSET_FB,
			 p2 + ATTR_OFFSET_FB, count*sizeof(u_int16_t));
	} else {
		bcopy((void *)p1, (void *)p2, count*sizeof(u_int16_t));
		bcopy((void *)(p1 + attr_offset(vtb)),
		      (void *)(p2 + attr_offset(vtb)), count*sizeof(u_int16_t));
	}
}

void
sc_vtb_delete(sc_vtb_t *vtb, int at, int count, int c, int attr)
{
	int len;
	vm_offset_t p1, p2;

	if (at + count > vtb->vtb_size)
		count = vtb->vtb_size - at;
	len = vtb->vtb_size - at - count;
	if (len > 0) {
		p1 = vtb_pointer(vtb, at + count);
		p2 = vtb_pointer(vtb, at);
		if (vtb->vtb_type == VTB_FRAMEBUFFER) {
			bcopy_io(p1, p2, len*sizeof(u_int16_t)); 
			bcopy_io(p1 + ATTR_OFFSET_FB,
				 p2 + ATTR_OFFSET_FB,
				 len*sizeof(u_int16_t)); 
		} else {
			bcopy((void *)p1, (void *)p2, len*sizeof(u_int16_t)); 
			bcopy((void *)(p1 + attr_offset(vtb)),
			      (void *)(p2 + attr_offset(vtb)),
			      len*sizeof(u_int16_t)); 
		}
	}
	p1 = vtb_pointer(vtb, at + len);
	if (vtb->vtb_type == VTB_FRAMEBUFFER) {
		fillw_io(c, p1, vtb->vtb_size - at - len);
		fillw_io(at2pc98(attr), p1 + ATTR_OFFSET_FB,
			 vtb->vtb_size - at - len);
	} else {
		fillw(c, (void *)p1, vtb->vtb_size - at - len);
		fillw(at2pc98(attr), (void *)(p1 + attr_offset(vtb)),
		      vtb->vtb_size - at - len);
	}
}

void
sc_vtb_ins(sc_vtb_t *vtb, int at, int count, int c, int attr)
{
	vm_offset_t p1, p2;

	p1 = vtb_pointer(vtb, at);
	if (at + count > vtb->vtb_size) {
		count = vtb->vtb_size - at;
	} else {
		p2 = vtb_pointer(vtb, at + count);
		if (vtb->vtb_type == VTB_FRAMEBUFFER) {
			bcopy_io(p1, p2, 
				 (vtb->vtb_size - at - count)*sizeof(u_int16_t)); 
			bcopy_io(p1 + ATTR_OFFSET_FB,
				 p2 + ATTR_OFFSET_FB,
				 (vtb->vtb_size - at - count)*sizeof(u_int16_t)); 
		} else {
			bcopy((void *)p1, (void *)p2,
			      (vtb->vtb_size - at - count)*sizeof(u_int16_t)); 
			bcopy((void *)(p1 + attr_offset(vtb)),
			      (void *)(p2 + attr_offset(vtb)),
			      (vtb->vtb_size - at - count)*sizeof(u_int16_t)); 
		}
	}
	if (vtb->vtb_type == VTB_FRAMEBUFFER) {
		fillw_io(c, p1, count);
		fillw_io(at2pc98(attr), p1 + ATTR_OFFSET_FB, count);
	} else {
		fillw(c, (void *)p1, count);
		fillw(at2pc98(attr), (void *)(p1 + attr_offset(vtb)), count);
	}
}
