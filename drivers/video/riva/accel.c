/*
 * linux/drivers/video/accel.c - nVidia RIVA 128/TNT/TNT2 fb driver
 *
 * Copyright 2000 Jindrich Makovicka, Ani Joshi
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive
 * for more details.
 */

#include "rivafb.h"

/* acceleration routines */

inline void wait_for_idle(struct rivafb_info *rinfo)
{
	while (rinfo->riva.Busy(&rinfo->riva));
}

/* set copy ROP, no mask */
static void riva_setup_ROP(struct rivafb_info *rinfo)
{
	RIVA_FIFO_FREE(rinfo->riva, Patt, 5);
	rinfo->riva.Patt->Shape = 0;
	rinfo->riva.Patt->Color0 = 0xffffffff;
	rinfo->riva.Patt->Color1 = 0xffffffff;
	rinfo->riva.Patt->Monochrome[0] = 0xffffffff;
	rinfo->riva.Patt->Monochrome[1] = 0xffffffff;

	RIVA_FIFO_FREE(rinfo->riva, Rop, 1);
	rinfo->riva.Rop->Rop3 = 0xCC;
}

void riva_setup_accel(struct rivafb_info *rinfo)
{
	RIVA_FIFO_FREE(rinfo->riva, Clip, 2);
	rinfo->riva.Clip->TopLeft     = 0x0;
	rinfo->riva.Clip->WidthHeight = 0x80008000;
	riva_setup_ROP(rinfo);
	wait_for_idle(rinfo);
}

static void riva_rectfill(struct rivafb_info *rinfo, int sy,
			  int sx, int height, int width, u_int color)
{
	RIVA_FIFO_FREE(rinfo->riva, Bitmap, 1);
	rinfo->riva.Bitmap->Color1A = color;

	RIVA_FIFO_FREE(rinfo->riva, Bitmap, 2);
	rinfo->riva.Bitmap->UnclippedRectangle[0].TopLeft     = (sx << 16) | sy; 
	rinfo->riva.Bitmap->UnclippedRectangle[0].WidthHeight = (width << 16) | height;
}

static void fbcon_riva_bmove(struct display *p, int sy, int sx, int dy, int dx,
			    int height, int width)
{
	struct rivafb_info *rinfo = (struct rivafb_info *)(p->fb_info);

	sx *= fontwidth(p);
	sy *= fontheight(p);
	dx *= fontwidth(p);
	dy *= fontheight(p);
	width *= fontwidth(p);
	height *= fontheight(p);

	RIVA_FIFO_FREE(rinfo->riva, Blt, 3);
	rinfo->riva.Blt->TopLeftSrc  = (sy << 16) | sx;
	rinfo->riva.Blt->TopLeftDst  = (dy << 16) | dx;
	rinfo->riva.Blt->WidthHeight = (height  << 16) | width;

	wait_for_idle(rinfo);
}

static void riva_clear_margins(struct vc_data *conp, struct display *p,
				int bottom_only, u32 bgx)
{
	struct rivafb_info *rinfo = (struct rivafb_info *)(p->fb_info);

	unsigned int right_start = conp->vc_cols*fontwidth(p);
	unsigned int bottom_start = conp->vc_rows*fontheight(p);
	unsigned int right_width, bottom_width;

	if (!bottom_only && (right_width = p->var.xres - right_start))
		riva_rectfill(rinfo, 0, right_start, p->var.yres_virtual,
			      right_width, bgx);
	if ((bottom_width = p->var.yres - bottom_start))
		riva_rectfill(rinfo, p->var.yoffset + bottom_start, 0,
			      bottom_width, right_start, bgx);
}

static u8 byte_rev[256] = {
	0x00, 0x80, 0x40, 0xc0, 0x20, 0xa0, 0x60, 0xe0, 0x10, 0x90, 0x50, 0xd0, 0x30, 0xb0, 0x70, 0xf0,
	0x08, 0x88, 0x48, 0xc8, 0x28, 0xa8, 0x68, 0xe8, 0x18, 0x98, 0x58, 0xd8, 0x38, 0xb8, 0x78, 0xf8, 
	0x04, 0x84, 0x44, 0xc4, 0x24, 0xa4, 0x64, 0xe4, 0x14, 0x94, 0x54, 0xd4, 0x34, 0xb4, 0x74, 0xf4, 
	0x0c, 0x8c, 0x4c, 0xcc, 0x2c, 0xac, 0x6c, 0xec, 0x1c, 0x9c, 0x5c, 0xdc, 0x3c, 0xbc, 0x7c, 0xfc, 
	0x02, 0x82, 0x42, 0xc2, 0x22, 0xa2, 0x62, 0xe2, 0x12, 0x92, 0x52, 0xd2, 0x32, 0xb2, 0x72, 0xf2, 
	0x0a, 0x8a, 0x4a, 0xca, 0x2a, 0xaa, 0x6a, 0xea, 0x1a, 0x9a, 0x5a, 0xda, 0x3a, 0xba, 0x7a, 0xfa, 
	0x06, 0x86, 0x46, 0xc6, 0x26, 0xa6, 0x66, 0xe6, 0x16, 0x96, 0x56, 0xd6, 0x36, 0xb6, 0x76, 0xf6, 
	0x0e, 0x8e, 0x4e, 0xce, 0x2e, 0xae, 0x6e, 0xee, 0x1e, 0x9e, 0x5e, 0xde, 0x3e, 0xbe, 0x7e, 0xfe, 
	0x01, 0x81, 0x41, 0xc1, 0x21, 0xa1, 0x61, 0xe1, 0x11, 0x91, 0x51, 0xd1, 0x31, 0xb1, 0x71, 0xf1, 
	0x09, 0x89, 0x49, 0xc9, 0x29, 0xa9, 0x69, 0xe9, 0x19, 0x99, 0x59, 0xd9, 0x39, 0xb9, 0x79, 0xf9, 
	0x05, 0x85, 0x45, 0xc5, 0x25, 0xa5, 0x65, 0xe5, 0x15, 0x95, 0x55, 0xd5, 0x35, 0xb5, 0x75, 0xf5, 
	0x0d, 0x8d, 0x4d, 0xcd, 0x2d, 0xad, 0x6d, 0xed, 0x1d, 0x9d, 0x5d, 0xdd, 0x3d, 0xbd, 0x7d, 0xfd, 
	0x03, 0x83, 0x43, 0xc3, 0x23, 0xa3, 0x63, 0xe3, 0x13, 0x93, 0x53, 0xd3, 0x33, 0xb3, 0x73, 0xf3, 
	0x0b, 0x8b, 0x4b, 0xcb, 0x2b, 0xab, 0x6b, 0xeb, 0x1b, 0x9b, 0x5b, 0xdb, 0x3b, 0xbb, 0x7b, 0xfb, 
	0x07, 0x87, 0x47, 0xc7, 0x27, 0xa7, 0x67, 0xe7, 0x17, 0x97, 0x57, 0xd7, 0x37, 0xb7, 0x77, 0xf7, 
	0x0f, 0x8f, 0x4f, 0xcf, 0x2f, 0xaf, 0x6f, 0xef, 0x1f, 0x9f, 0x5f, 0xdf, 0x3f, 0xbf, 0x7f, 0xff,
};

static inline void fbcon_reverse_order(u32 *l)
{
	u8 *a = (u8 *)l;
	*a = byte_rev[*a], a++;
/*	*a = byte_rev[*a], a++;
	*a = byte_rev[*a], a++;*/
	*a = byte_rev[*a];
}

static void fbcon_riva_writechr(struct vc_data *conp, struct display *p,
			        int c, int fgx, int bgx, int yy, int xx)
{
	u8 *cdat;
	struct rivafb_info *rinfo = (struct rivafb_info *)(p->fb_info);
	int w, h;
	volatile u32 *d;
	u32 cdat2;
	int i, j, cnt;

	w = fontwidth(p);
	h = fontheight(p);

	if (w <= 8)
		cdat = p->fontdata + (c & p->charmask) * h;
	else
		cdat = p->fontdata + ((c & p->charmask) * h << 1);

        RIVA_FIFO_FREE(rinfo->riva, Bitmap, 7);
        rinfo->riva.Bitmap->ClipE.TopLeft     = (yy << 16) | (xx & 0xFFFF);
        rinfo->riva.Bitmap->ClipE.BottomRight = ((yy+h) << 16) | ((xx+w) & 0xffff);
        rinfo->riva.Bitmap->Color0E           = bgx;
        rinfo->riva.Bitmap->Color1E           = fgx;
        rinfo->riva.Bitmap->WidthHeightInE  = (h << 16) | 32;
        rinfo->riva.Bitmap->WidthHeightOutE = (h << 16) | 32;
        rinfo->riva.Bitmap->PointE          = (yy << 16) | (xx & 0xFFFF);
	
	d = &rinfo->riva.Bitmap->MonochromeData01E;
	for (i = h; i > 0; i-=16) {
		if (i >= 16)
			cnt = 16;
		else
			cnt = i;
		RIVA_FIFO_FREE(rinfo->riva, Bitmap, cnt);
		for (j = 0; j < cnt; j++) {
			if (w <= 8) 
				cdat2 = *cdat++;
			else
				cdat2 = *((u16*)cdat)++;
			fbcon_reverse_order(&cdat2);
			d[j] = cdat2;
		}
	}
}

#ifdef FBCON_HAS_CFB8
void fbcon_riva8_setup(struct display *p)
{
    p->next_line = p->line_length ? p->line_length : p->var.xres_virtual;
    p->next_plane = 0;
}

static void fbcon_riva8_clear(struct vc_data *conp, struct display *p, int sy,
			     int sx, int height, int width)
{
	u32 bgx;

	struct rivafb_info *rinfo = (struct rivafb_info *)(p->fb_info);

	bgx = attr_bgcol_ec(p, conp);

	sx *= fontwidth(p);
	sy *= fontheight(p);
	width *= fontwidth(p);
	height *= fontheight(p);

	riva_rectfill(rinfo, sy, sx, height, width, bgx);
}

static void fbcon_riva8_putc(struct vc_data *conp, struct display *p, int c,
			    int yy, int xx)
{
	u32 fgx,bgx;

	fgx = attr_fgcol(p,c);
	bgx = attr_bgcol(p,c);
	
	xx *= fontwidth(p);
	yy *= fontheight(p);

	fbcon_riva_writechr(conp, p, c, fgx, bgx, yy, xx);
}

static void fbcon_riva8_putcs(struct vc_data *conp, struct display *p,
			     const unsigned short *s, int count, int yy,
			     int xx)
{
	u16 c;
	u32 fgx,bgx;

	xx *= fontwidth(p);
	yy *= fontheight(p);

	c = scr_readw(s);
	fgx = attr_fgcol(p, c);
	bgx = attr_bgcol(p, c);
	while (count--) {
		c = scr_readw(s++);
		fbcon_riva_writechr(conp, p, c, fgx, bgx, yy, xx);
		xx += fontwidth(p);
	}
}

static void fbcon_riva8_revc(struct display *p, int xx, int yy)
{
	struct rivafb_info *rinfo = (struct rivafb_info *) (p->fb_info);

	xx *= fontwidth(p);
	yy *= fontheight(p);

	RIVA_FIFO_FREE(rinfo->riva, Rop, 1);
	rinfo->riva.Rop->Rop3 = 0x66; // XOR
	riva_rectfill(rinfo, yy, xx, fontheight(p), fontwidth(p), 0x0f);
	RIVA_FIFO_FREE(rinfo->riva, Rop, 1);
	rinfo->riva.Rop->Rop3 = 0xCC; // back to COPY
}

static void fbcon_riva8_clear_margins(struct vc_data *conp, struct display *p,
				       int bottom_only)
{
	riva_clear_margins(conp, p, bottom_only, attr_bgcol_ec(p, conp));
}

struct display_switch fbcon_riva8 = {
	setup:		fbcon_riva8_setup,
	bmove:		fbcon_riva_bmove,
	clear:		fbcon_riva8_clear,
#ifdef __BIG_ENDIAN
	putc:		fbcon_cfb8_putc,
	putcs:		fbcon_cfb8_putcs,
	revc:		fbcon_cfb8_revc,
	clear_margins:	fbcon_cfb8_clear_margins,
#else
	putc:		fbcon_riva8_putc,
	putcs:		fbcon_riva8_putcs,
	revc:		fbcon_riva8_revc,
	clear_margins:	fbcon_riva8_clear_margins,
#endif	
	fontwidthmask:	FONTWIDTHRANGE(4, 16)
};
#endif

#if defined(FBCON_HAS_CFB16) || defined(FBCON_HAS_CFB32)
static void fbcon_riva1632_revc(struct display *p, int xx, int yy)
{
	struct rivafb_info *rinfo = (struct rivafb_info *) (p->fb_info);

	xx *= fontwidth(p);
	yy *= fontheight(p);

	RIVA_FIFO_FREE(rinfo->riva, Rop, 1);
	rinfo->riva.Rop->Rop3 = 0x66; // XOR
	riva_rectfill(rinfo, yy, xx, fontheight(p), fontwidth(p), 0xffffffff);
	RIVA_FIFO_FREE(rinfo->riva, Rop, 1);
	rinfo->riva.Rop->Rop3 = 0xCC; // back to COPY
}
#endif

#ifdef FBCON_HAS_CFB16
void fbcon_riva16_setup(struct display *p)
{
    p->next_line = p->line_length ? p->line_length : p->var.xres_virtual<<1;
    p->next_plane = 0;
}

static void fbcon_riva16_clear(struct vc_data *conp, struct display *p, int sy,
			     int sx, int height, int width)
{
	u32 bgx;

	struct rivafb_info *rinfo = (struct rivafb_info *)(p->fb_info);

	bgx = ((u_int16_t*)p->dispsw_data)[attr_bgcol_ec(p, conp)];

	sx *= fontwidth(p);
	sy *= fontheight(p);
	width *= fontwidth(p);
	height *= fontheight(p);

	riva_rectfill(rinfo, sy, sx, height, width, bgx);
}

static inline void convert_bgcolor_16(u32 *col)
{
	*col = ((*col & 0x00007C00) << 9)
             | ((*col & 0x000003E0) << 6)
             | ((*col & 0x0000001F) << 3)
             |          0xFF000000;
}

static void fbcon_riva16_putc(struct vc_data *conp, struct display *p, int c,
			    int yy, int xx)
{
	u32 fgx,bgx;

	fgx = ((u16 *)p->dispsw_data)[attr_fgcol(p,c)];
	bgx = ((u16 *)p->dispsw_data)[attr_bgcol(p,c)];
	if (p->var.green.length == 6)
		convert_bgcolor_16(&bgx);
	xx *= fontwidth(p);
	yy *= fontheight(p);

	fbcon_riva_writechr(conp, p, c, fgx, bgx, yy, xx);
}

static void fbcon_riva16_putcs(struct vc_data *conp, struct display *p,
			     const unsigned short *s, int count, int yy,
			     int xx)
{
	u16 c;
	u32 fgx,bgx;

	xx *= fontwidth(p);
	yy *= fontheight(p);

	c = scr_readw(s);
	fgx = ((u16 *)p->dispsw_data)[attr_fgcol(p, c)];
	bgx = ((u16 *)p->dispsw_data)[attr_bgcol(p, c)];
	if (p->var.green.length == 6)
		convert_bgcolor_16(&bgx);
	while (count--) {
		c = scr_readw(s++);
		fbcon_riva_writechr(conp, p, c, fgx, bgx, yy, xx);
		xx += fontwidth(p);
	}
}

static void fbcon_riva16_clear_margins(struct vc_data *conp, struct display *p,
				       int bottom_only)
{
	riva_clear_margins(conp, p, bottom_only, ((u16 *)p->dispsw_data)[attr_bgcol_ec(p, conp)]);
}

struct display_switch fbcon_riva16 = {
	setup:		fbcon_riva16_setup,
	bmove:		fbcon_riva_bmove,
	clear:		fbcon_riva16_clear,
#ifdef __BIG_ENDIAN
	putc:		fbcon_cfb16_putc,
	putcs:		fbcon_cfb16_putcs,
	revc:		fbcon_cfb16_revc,
	clear_margins:	fbcon_cfb16_clear_margins,
#else
	putc:		fbcon_riva16_putc,
	putcs:		fbcon_riva16_putcs,
	revc:		fbcon_riva1632_revc,
	clear_margins:	fbcon_riva16_clear_margins,
#endif
	fontwidthmask:	FONTWIDTHRANGE(4, 16)
};
#endif

#ifdef FBCON_HAS_CFB32
void fbcon_riva32_setup(struct display *p)
{
    p->next_line = p->line_length ? p->line_length : p->var.xres_virtual<<2;
    p->next_plane = 0;
}

static void fbcon_riva32_clear(struct vc_data *conp, struct display *p, int sy,
			     int sx, int height, int width)
{
	u32 bgx;

	struct rivafb_info *rinfo = (struct rivafb_info *)(p->fb_info);

	bgx = ((u_int32_t*)p->dispsw_data)[attr_bgcol_ec(p, conp)];

	sx *= fontwidth(p);
	sy *= fontheight(p);
	width *= fontwidth(p);
	height *= fontheight(p);

	riva_rectfill(rinfo, sy, sx, height, width, bgx);
}

static void fbcon_riva32_putc(struct vc_data *conp, struct display *p, int c,
			    int yy, int xx)
{
	u32 fgx,bgx;

	fgx = ((u32 *)p->dispsw_data)[attr_fgcol(p,c)];
	bgx = ((u32 *)p->dispsw_data)[attr_bgcol(p,c)];
	xx *= fontwidth(p);
	yy *= fontheight(p);
	fbcon_riva_writechr(conp, p, c, fgx, bgx, yy, xx);
}

static void fbcon_riva32_putcs(struct vc_data *conp, struct display *p,
			     const unsigned short *s, int count, int yy,
			     int xx)
{
	u16 c;
	u32 fgx,bgx;

	xx *= fontwidth(p);
	yy *= fontheight(p);

	c = scr_readw(s);
	fgx = ((u32 *)p->dispsw_data)[attr_fgcol(p, c)];
	bgx = ((u32 *)p->dispsw_data)[attr_bgcol(p, c)];
	while (count--) {
		c = scr_readw(s++);
		fbcon_riva_writechr(conp, p, c, fgx, bgx, yy, xx);
		xx += fontwidth(p);
	}
}

static void fbcon_riva32_clear_margins(struct vc_data *conp, struct display *p,
				       int bottom_only)
{
	riva_clear_margins(conp, p, bottom_only, ((u32 *)p->dispsw_data)[attr_bgcol_ec(p, conp)]);
}

struct display_switch fbcon_riva32 = {
	setup:		fbcon_riva32_setup,
	bmove:		fbcon_riva_bmove,
	clear:		fbcon_riva32_clear,
#ifdef __BIG_ENDIAN
	putc:		fbcon_cfb32_putc,
	putcs:		fbcon_cfb32_putcs,
	revc:		fbcon_cfb32_revc,
	clear_margins:	fbcon_cfb32_clear_margins,
#else
	putc:		fbcon_riva32_putc,
	putcs:		fbcon_riva32_putcs,
	revc:		fbcon_riva1632_revc,
	clear_margins:	fbcon_riva32_clear_margins,
#endif
	fontwidthmask:	FONTWIDTHRANGE(4, 16)
};
#endif
