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

#define ATTR_OFFSET	0x2000

#define vtb_wrap(vtb, at, offset)				\
    (((at) + (offset) + (vtb)->vtb_size)%(vtb)->vtb_size)

static u_int16_t	at2pc98(u_int16_t attr);
static vm_offset_t	sc_vtb_attr_pointer(sc_vtb_t *vtb, int at);

static u_int16_t
at2pc98(u_int16_t attr)
{
	static u_char ibmpc_to_pc98[16] = {
		0x01, 0x21, 0x81, 0xa1, 0x41, 0x61, 0xc1, 0xe1, 
		0x09, 0x29, 0x89, 0xa9, 0x49, 0x69, 0xc9, 0xe9
	};
	static u_char ibmpc_to_pc98rev[16] = {
		0x05, 0x25, 0x85, 0xa5, 0x45, 0x65, 0xc5, 0xe5, 
		0x0d, 0x2d, 0x8d, 0xad, 0x4d, 0x6d, 0xcd, 0xed
	};
	u_char fg_at, bg_at;
	u_int16_t at;

	if (attr & 0x00FF)
		return (attr);

	fg_at = ((attr >> 8) & 0x0F);
	bg_at = ((attr >> 12) & 0x0F);

	if (bg_at) {
		if (bg_at & 0x08) {
			if (bg_at & 0x07) {
				/* reverse & blink */
				at = ibmpc_to_pc98rev[bg_at] | 0x02;
			} else {
				/* normal & blink */
				at = ibmpc_to_pc98[fg_at] | 0x02;
			}
		} else {
			/* reverse */
			at = ibmpc_to_pc98rev[bg_at];
		}
	} else {
		/* normal */
		at = ibmpc_to_pc98[fg_at];
	}
	at |= attr;
	return (at);
}

void
sc_vtb_init(sc_vtb_t *vtb, int type, int cols, int rows, void *buf, int wait)
{
	vtb->vtb_flags = 0;
	vtb->vtb_type = type;
	vtb->vtb_cols = cols;
	vtb->vtb_rows = rows;
	vtb->vtb_size = cols*rows;
	vtb->vtb_buffer = NULL;
	vtb->vtb_tail = 0;

	switch (type) {
	case VTB_MEMORY:
	case VTB_RINGBUFFER:
		if ((buf == NULL) && (cols*rows != 0)) {
			vtb->vtb_buffer =
				(vm_offset_t)malloc(cols*rows*sizeof(u_int16_t)*2,
						    M_DEVBUF, 
						    (wait) ? M_WAITOK : M_NOWAIT);
			if (vtb->vtb_buffer != NULL) {
				bzero((void *)sc_vtb_pointer(vtb, 0),
				      cols*rows*sizeof(u_int16_t)*2);
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

	vtb->vtb_flags = 0;
	vtb->vtb_cols = 0;
	vtb->vtb_rows = 0;
	vtb->vtb_size = 0;
	vtb->vtb_tail = 0;

	p = vtb->vtb_buffer;
	vtb->vtb_buffer = NULL;
	switch (vtb->vtb_type) {
	case VTB_MEMORY:
	case VTB_RINGBUFFER:
		if (p != NULL)
			free((void *)p, M_DEVBUF);
		break;
	default:
		break;
	}
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
	if (vtb->vtb_type == VTB_FRAMEBUFFER)
		return (readw(sc_vtb_pointer(vtb, at)) & 0x00ff);
	else
		return (*(u_int16_t *)sc_vtb_pointer(vtb, at) & 0x00ff);
}

int
sc_vtb_geta(sc_vtb_t *vtb, int at)
{
	if (vtb->vtb_type == VTB_FRAMEBUFFER)
		return (readw(sc_vtb_attr_pointer(vtb, at)) & 0x00ff);
	else
		return (*(u_int16_t *)sc_vtb_attr_pointer(vtb, at) & 0x00ff);
}

void
sc_vtb_putc(sc_vtb_t *vtb, int at, int c, int a)
{
	if (vtb->vtb_type == VTB_FRAMEBUFFER) {
		writew(sc_vtb_pointer(vtb, at), c);
		writew(sc_vtb_attr_pointer(vtb, at), at2pc98(a));
	} else {
		*(u_int16_t *)sc_vtb_pointer(vtb, at) = c;
		*(u_int16_t *)sc_vtb_attr_pointer(vtb, at) = at2pc98(a);
	}
}

vm_offset_t
sc_vtb_putchar(sc_vtb_t *vtb, vm_offset_t p, int c, int a)
{
	if (vtb->vtb_type == VTB_FRAMEBUFFER) {
		writew(p, c);
		writew(p + ATTR_OFFSET, at2pc98(a));
	} else {
		*(u_int16_t *)p = c;
		*(u_int16_t *)(p + vtb->vtb_size*sizeof(u_int16_t)) = at2pc98(a);
	}
	return (p + sizeof(u_int16_t));
}

vm_offset_t
sc_vtb_pointer(sc_vtb_t *vtb, int at)
{
	return (vtb->vtb_buffer + sizeof(u_int16_t)*(at));
}

static vm_offset_t
sc_vtb_attr_pointer(sc_vtb_t *vtb, int at)
{
	return (vtb->vtb_buffer + sizeof(u_int16_t)*(at)
		+ ((vtb->vtb_type == VTB_FRAMEBUFFER) ? 
			ATTR_OFFSET : vtb->vtb_size*sizeof(u_int16_t)));
}

int
sc_vtb_pos(sc_vtb_t *vtb, int pos, int offset)
{
	return ((pos + offset + vtb->vtb_size)%vtb->vtb_size);
}

void
sc_vtb_clear(sc_vtb_t *vtb, int c, int attr)
{
	if (vtb->vtb_type == VTB_FRAMEBUFFER) {
		fillw_io(c, sc_vtb_pointer(vtb, 0), vtb->vtb_size);
		fillw_io(at2pc98(attr), sc_vtb_attr_pointer(vtb, 0), vtb->vtb_size);
	} else {
		fillw(c, (void *)sc_vtb_pointer(vtb, 0), vtb->vtb_size);
		fillw(at2pc98(attr), (void *)sc_vtb_attr_pointer(vtb, 0), vtb->vtb_size);
	}
}

void
sc_vtb_copy(sc_vtb_t *vtb1, int from, sc_vtb_t *vtb2, int to, int count)
{
	if (vtb2->vtb_type == VTB_FRAMEBUFFER) {
		bcopy_toio(sc_vtb_pointer(vtb1, from),
			   sc_vtb_pointer(vtb2, to),
			   count*sizeof(u_int16_t));
		bcopy_toio(sc_vtb_attr_pointer(vtb1, from),
			   sc_vtb_attr_pointer(vtb2, to),
			   count*sizeof(u_int16_t));
	} else if (vtb1->vtb_type == VTB_FRAMEBUFFER) {
		bcopy_fromio(sc_vtb_pointer(vtb1, from),
			     sc_vtb_pointer(vtb2, to),
			     count*sizeof(u_int16_t));
		bcopy_fromio(sc_vtb_attr_pointer(vtb1, from),
			     sc_vtb_attr_pointer(vtb2, to),
			     count*sizeof(u_int16_t));
	} else {
		bcopy((void *)sc_vtb_pointer(vtb1, from),
		      (void *)sc_vtb_pointer(vtb2, to),
		      count*sizeof(u_int16_t));
		bcopy((void *)sc_vtb_attr_pointer(vtb1, from),
		      (void *)sc_vtb_attr_pointer(vtb2, to),
		      count*sizeof(u_int16_t));
	}
}

void
sc_vtb_append(sc_vtb_t *vtb1, int from, sc_vtb_t *vtb2, int count)
{
	int len;

	if (vtb2->vtb_type != VTB_RINGBUFFER)
		return;

	while (count > 0) {
		len = imin(count, vtb2->vtb_size - vtb2->vtb_tail);
		if (vtb1->vtb_type == VTB_FRAMEBUFFER) {
			bcopy_fromio(sc_vtb_pointer(vtb1, from),
				     sc_vtb_pointer(vtb2, vtb2->vtb_tail),
				     len*sizeof(u_int16_t));
			bcopy_fromio(sc_vtb_attr_pointer(vtb1, from),
				     sc_vtb_attr_pointer(vtb2, vtb2->vtb_tail),
				     len*sizeof(u_int16_t));
		} else {
			bcopy((void *)sc_vtb_pointer(vtb1, from),
			      (void *)sc_vtb_pointer(vtb2, vtb2->vtb_tail),
			      len*sizeof(u_int16_t));
			bcopy((void *)sc_vtb_attr_pointer(vtb1, from),
			      (void *)sc_vtb_attr_pointer(vtb2, vtb2->vtb_tail),
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
	if (at + count > vtb->vtb_size)
		count = vtb->vtb_size - at;
	if (vtb->vtb_type == VTB_FRAMEBUFFER) {
		fillw_io(c, sc_vtb_pointer(vtb, at), count);
		fillw_io(at2pc98(attr), sc_vtb_attr_pointer(vtb, at), count);
	} else {
		fillw(c, (void *)sc_vtb_pointer(vtb, at), count);
		fillw(at2pc98(attr), (void *)sc_vtb_attr_pointer(vtb, at), count);
	}
}

void
sc_vtb_move(sc_vtb_t *vtb, int from, int to, int count)
{
	if (from + count > vtb->vtb_size)
		count = vtb->vtb_size - from;
	if (to + count > vtb->vtb_size)
		count = vtb->vtb_size - to;
	if (count <= 0)
		return;
	if (vtb->vtb_type == VTB_FRAMEBUFFER) {
		bcopy_io(sc_vtb_pointer(vtb, from),
			 sc_vtb_pointer(vtb, to), count*sizeof(u_int16_t)); 
		bcopy_io(sc_vtb_attr_pointer(vtb, from),
			 sc_vtb_attr_pointer(vtb, to), count*sizeof(u_int16_t));
	} else {
		bcopy((void *)sc_vtb_pointer(vtb, from),
		      (void *)sc_vtb_pointer(vtb, to), count*sizeof(u_int16_t));
		bcopy((void *)sc_vtb_attr_pointer(vtb, from),
		      (void *)sc_vtb_attr_pointer(vtb, to), count*sizeof(u_int16_t));
	}
}

void
sc_vtb_delete(sc_vtb_t *vtb, int at, int count, int c, int attr)
{
	int len;

	if (at + count > vtb->vtb_size)
		count = vtb->vtb_size - at;
	len = vtb->vtb_size - at - count;
	if (len > 0) {
		if (vtb->vtb_type == VTB_FRAMEBUFFER) {
			bcopy_io(sc_vtb_pointer(vtb, at + count),
				 sc_vtb_pointer(vtb, at),
				 len*sizeof(u_int16_t)); 
			bcopy_io(sc_vtb_attr_pointer(vtb, at + count),
				 sc_vtb_attr_pointer(vtb, at),
				 len*sizeof(u_int16_t)); 
		} else {
			bcopy((void *)sc_vtb_pointer(vtb, at + count),
			      (void *)sc_vtb_pointer(vtb, at),
			      len*sizeof(u_int16_t)); 
			bcopy((void *)sc_vtb_attr_pointer(vtb, at + count),
			      (void *)sc_vtb_attr_pointer(vtb, at),
			      len*sizeof(u_int16_t)); 
		}
	}
	if (vtb->vtb_type == VTB_FRAMEBUFFER) {
		fillw_io(c, sc_vtb_pointer(vtb, at + len),
			 vtb->vtb_size - at - len);
		fillw_io(at2pc98(attr),
			 sc_vtb_attr_pointer(vtb, at + len),
			 vtb->vtb_size - at - len);
	} else {
		fillw(c, (void *)sc_vtb_pointer(vtb, at + len),
		      vtb->vtb_size - at - len);
		fillw(at2pc98(attr),
		      (void *)sc_vtb_attr_pointer(vtb, at + len),
		      vtb->vtb_size - at - len);
	}
}

void
sc_vtb_ins(sc_vtb_t *vtb, int at, int count, int c, int attr)
{
	if (at + count > vtb->vtb_size) {
		count = vtb->vtb_size - at;
	} else {
		if (vtb->vtb_type == VTB_FRAMEBUFFER) {
			bcopy_io(sc_vtb_pointer(vtb, at),
				 sc_vtb_pointer(vtb, at + count),
				 (vtb->vtb_size - at - count)*sizeof(u_int16_t)); 
			bcopy_io(sc_vtb_attr_pointer(vtb, at),
				 sc_vtb_attr_pointer(vtb, at + count),
				 (vtb->vtb_size - at - count)*sizeof(u_int16_t)); 
		} else {
			bcopy((void *)sc_vtb_pointer(vtb, at),
			      (void *)sc_vtb_pointer(vtb, at + count),
			      (vtb->vtb_size - at - count)*sizeof(u_int16_t)); 
			bcopy((void *)sc_vtb_attr_pointer(vtb, at),
			      (void *)sc_vtb_attr_pointer(vtb, at + count),
			      (vtb->vtb_size - at - count)*sizeof(u_int16_t)); 
		}
	}
	if (vtb->vtb_type == VTB_FRAMEBUFFER) {
		fillw_io(c, sc_vtb_pointer(vtb, at), count);
		fillw_io(at2pc98(attr),
			 sc_vtb_attr_pointer(vtb, at), count);
	} else {
		fillw(c, (void *)sc_vtb_pointer(vtb, at), count);
		fillw(at2pc98(attr),
		      (void *)sc_vtb_attr_pointer(vtb, at), count);
	}
}
