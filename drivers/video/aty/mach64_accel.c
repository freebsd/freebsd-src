
/*
 *  ATI Mach64 Hardware Acceleration
 */

#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/fb.h>

#include <video/fbcon.h>
#include <video/fbcon-cfb8.h>
#include <video/fbcon-cfb16.h>
#include <video/fbcon-cfb24.h>
#include <video/fbcon-cfb32.h>

#include "mach64.h"
#include "atyfb.h"

    /*
     *  Text console acceleration
     */

static void fbcon_aty_bmove(struct display *p, int sy, int sx, int dy, int dx,
			    int height, int width);
static void fbcon_aty_clear(struct vc_data *conp, struct display *p, int sy,
			    int sx, int height, int width);


    /*
     *  Generic Mach64 routines
     */

void aty_reset_engine(const struct fb_info_aty *info)
{
    /* reset engine */
    aty_st_le32(GEN_TEST_CNTL,
		aty_ld_le32(GEN_TEST_CNTL, info) & ~GUI_ENGINE_ENABLE, info);
    /* enable engine */
    aty_st_le32(GEN_TEST_CNTL,
		aty_ld_le32(GEN_TEST_CNTL, info) | GUI_ENGINE_ENABLE, info);
    /* ensure engine is not locked up by clearing any FIFO or */
    /* HOST errors */
    aty_st_le32(BUS_CNTL, aty_ld_le32(BUS_CNTL, info) | BUS_HOST_ERR_ACK |
			  BUS_FIFO_ERR_ACK, info);
}

static void reset_GTC_3D_engine(const struct fb_info_aty *info)
{
	aty_st_le32(SCALE_3D_CNTL, 0xc0, info);
	mdelay(GTC_3D_RESET_DELAY);
	aty_st_le32(SETUP_CNTL, 0x00, info);
	mdelay(GTC_3D_RESET_DELAY);
	aty_st_le32(SCALE_3D_CNTL, 0x00, info);
	mdelay(GTC_3D_RESET_DELAY);
}

void aty_init_engine(const struct atyfb_par *par, struct fb_info_aty *info)
{
    u32 pitch_value;

    /* determine modal information from global mode structure */
    pitch_value = par->crtc.vxres;

    if (par->crtc.bpp == 24) {
	/* In 24 bpp, the engine is in 8 bpp - this requires that all */
	/* horizontal coordinates and widths must be adjusted */
	pitch_value = pitch_value * 3;
    }

    /* On GTC (RagePro), we need to reset the 3D engine before */
    if (M64_HAS(RESET_3D))
    	reset_GTC_3D_engine(info);

    /* Reset engine, enable, and clear any engine errors */
    aty_reset_engine(info);
    /* Ensure that vga page pointers are set to zero - the upper */
    /* page pointers are set to 1 to handle overflows in the */
    /* lower page */
    aty_st_le32(MEM_VGA_WP_SEL, 0x00010000, info);
    aty_st_le32(MEM_VGA_RP_SEL, 0x00010000, info);

    /* ---- Setup standard engine context ---- */

    /* All GUI registers here are FIFOed - therefore, wait for */
    /* the appropriate number of empty FIFO entries */
    wait_for_fifo(14, info);

    /* enable all registers to be loaded for context loads */
    aty_st_le32(CONTEXT_MASK, 0xFFFFFFFF, info);

    /* set destination pitch to modal pitch, set offset to zero */
    aty_st_le32(DST_OFF_PITCH, (pitch_value / 8) << 22, info);

    /* zero these registers (set them to a known state) */
    aty_st_le32(DST_Y_X, 0, info);
    aty_st_le32(DST_HEIGHT, 0, info);
    aty_st_le32(DST_BRES_ERR, 0, info);
    aty_st_le32(DST_BRES_INC, 0, info);
    aty_st_le32(DST_BRES_DEC, 0, info);

    /* set destination drawing attributes */
    aty_st_le32(DST_CNTL, DST_LAST_PEL | DST_Y_TOP_TO_BOTTOM |
			  DST_X_LEFT_TO_RIGHT, info);

    /* set source pitch to modal pitch, set offset to zero */
    aty_st_le32(SRC_OFF_PITCH, (pitch_value / 8) << 22, info);

    /* set these registers to a known state */
    aty_st_le32(SRC_Y_X, 0, info);
    aty_st_le32(SRC_HEIGHT1_WIDTH1, 1, info);
    aty_st_le32(SRC_Y_X_START, 0, info);
    aty_st_le32(SRC_HEIGHT2_WIDTH2, 1, info);

    /* set source pixel retrieving attributes */
    aty_st_le32(SRC_CNTL, SRC_LINE_X_LEFT_TO_RIGHT, info);

    /* set host attributes */
    wait_for_fifo(13, info);
    aty_st_le32(HOST_CNTL, 0, info);

    /* set pattern attributes */
    aty_st_le32(PAT_REG0, 0, info);
    aty_st_le32(PAT_REG1, 0, info);
    aty_st_le32(PAT_CNTL, 0, info);

    /* set scissors to modal size */
    aty_st_le32(SC_LEFT, 0, info);
    aty_st_le32(SC_TOP, 0, info);
    aty_st_le32(SC_BOTTOM, par->crtc.vyres-1, info);
    aty_st_le32(SC_RIGHT, pitch_value-1, info);

    /* set background color to minimum value (usually BLACK) */
    aty_st_le32(DP_BKGD_CLR, 0, info);

    /* set foreground color to maximum value (usually WHITE) */
    aty_st_le32(DP_FRGD_CLR, 0xFFFFFFFF, info);

    /* set write mask to effect all pixel bits */
    aty_st_le32(DP_WRITE_MASK, 0xFFFFFFFF, info);

    /* set foreground mix to overpaint and background mix to */
    /* no-effect */
    aty_st_le32(DP_MIX, FRGD_MIX_S | BKGD_MIX_D, info);

    /* set primary source pixel channel to foreground color */
    /* register */
    aty_st_le32(DP_SRC, FRGD_SRC_FRGD_CLR, info);

    /* set compare functionality to false (no-effect on */
    /* destination) */
    wait_for_fifo(3, info);
    aty_st_le32(CLR_CMP_CLR, 0, info);
    aty_st_le32(CLR_CMP_MASK, 0xFFFFFFFF, info);
    aty_st_le32(CLR_CMP_CNTL, 0, info);

    /* set pixel depth */
    wait_for_fifo(2, info);
    aty_st_le32(DP_PIX_WIDTH, par->crtc.dp_pix_width, info);
    aty_st_le32(DP_CHAIN_MASK, par->crtc.dp_chain_mask, info);

    wait_for_fifo(5, info);
    aty_st_le32(SCALE_3D_CNTL, 0, info);
    aty_st_le32(Z_CNTL, 0, info);
    aty_st_le32(CRTC_INT_CNTL, aty_ld_le32(CRTC_INT_CNTL, info) & ~0x20, info);
    aty_st_le32(GUI_TRAJ_CNTL, 0x100023, info);

    /* insure engine is idle before leaving */
    wait_for_idle(info);
}


    /*
     *  Accelerated functions
     */

static inline void draw_rect(s16 x, s16 y, u16 width, u16 height,
			     struct fb_info_aty *info)
{
    /* perform rectangle fill */
    wait_for_fifo(2, info);
    aty_st_le32(DST_Y_X, (x << 16) | y, info);
    aty_st_le32(DST_HEIGHT_WIDTH, (width << 16) | height, info);
    info->blitter_may_be_busy = 1;
}

static inline void aty_rectcopy(int srcx, int srcy, int dstx, int dsty,
				u_int width, u_int height,
				struct fb_info_aty *info)
{
    u32 direction = DST_LAST_PEL;
    u32 pitch_value;

    if (!width || !height)
	return;

    pitch_value = info->current_par.crtc.vxres;
    if (info->current_par.crtc.bpp == 24) {
	/* In 24 bpp, the engine is in 8 bpp - this requires that all */
	/* horizontal coordinates and widths must be adjusted */
	pitch_value *= 3;
	srcx *= 3;
	dstx *= 3;
	width *= 3;
    }

    if (srcy < dsty) {
	dsty += height - 1;
	srcy += height - 1;
    } else
	direction |= DST_Y_TOP_TO_BOTTOM;

    if (srcx < dstx) {
	dstx += width - 1;
	srcx += width - 1;
    } else
	direction |= DST_X_LEFT_TO_RIGHT;

    wait_for_fifo(4, info);
    aty_st_le32(DP_SRC, FRGD_SRC_BLIT, info);
    aty_st_le32(SRC_Y_X, (srcx << 16) | srcy, info);
    aty_st_le32(SRC_HEIGHT1_WIDTH1, (width << 16) | height, info);
    aty_st_le32(DST_CNTL, direction, info);
    draw_rect(dstx, dsty, width, height, info);
}

void aty_rectfill(int dstx, int dsty, u_int width, u_int height, u_int color,
		  struct fb_info_aty *info)
{
    if (!width || !height)
	return;

    if (info->current_par.crtc.bpp == 24) {
	/* In 24 bpp, the engine is in 8 bpp - this requires that all */
	/* horizontal coordinates and widths must be adjusted */
	dstx *= 3;
	width *= 3;
    }

    wait_for_fifo(3, info);
    aty_st_le32(DP_FRGD_CLR, color, info);
    aty_st_le32(DP_SRC, BKGD_SRC_BKGD_CLR | FRGD_SRC_FRGD_CLR | MONO_SRC_ONE,
		info);
    aty_st_le32(DST_CNTL, DST_LAST_PEL | DST_Y_TOP_TO_BOTTOM |
			  DST_X_LEFT_TO_RIGHT, info);
    draw_rect(dstx, dsty, width, height, info);
}


    /*
     *  Text console acceleration
     */

static void fbcon_aty_bmove(struct display *p, int sy, int sx, int dy, int dx,
			    int height, int width)
{
#ifdef __sparc__
    struct fb_info_aty *fb = (struct fb_info_aty *)(p->fb_info);

    if (fb->mmaped && (!fb->fb_info.display_fg
	|| fb->fb_info.display_fg->vc_num == fb->vtconsole))
	return;
#endif

    sx *= fontwidth(p);
    sy *= fontheight(p);
    dx *= fontwidth(p);
    dy *= fontheight(p);
    width *= fontwidth(p);
    height *= fontheight(p);

    aty_rectcopy(sx, sy, dx, dy, width, height,
		 (struct fb_info_aty *)p->fb_info);
}

static void fbcon_aty_clear(struct vc_data *conp, struct display *p, int sy,
			    int sx, int height, int width)
{
    u32 bgx;
#ifdef __sparc__
    struct fb_info_aty *fb = (struct fb_info_aty *)(p->fb_info);

    if (fb->mmaped && (!fb->fb_info.display_fg
	|| fb->fb_info.display_fg->vc_num == fb->vtconsole))
	return;
#endif

    bgx = attr_bgcol_ec(p, conp);
    bgx |= (bgx << 8);
    bgx |= (bgx << 16);

    sx *= fontwidth(p);
    sy *= fontheight(p);
    width *= fontwidth(p);
    height *= fontheight(p);

    aty_rectfill(sx, sy, width, height, bgx,
		 (struct fb_info_aty *)p->fb_info);
}

#ifdef __sparc__
#define check_access \
    if (fb->mmaped && (!fb->fb_info.display_fg \
	|| fb->fb_info.display_fg->vc_num == fb->vtconsole)) \
	return;
#else
#define check_access do { } while (0)
#endif

#define DEF_FBCON_ATY_OP(name, call, args...) \
static void name(struct vc_data *conp, struct display *p, args) \
{ \
    struct fb_info_aty *fb = (struct fb_info_aty *)(p->fb_info); \
    check_access; \
    if (fb->blitter_may_be_busy) \
	wait_for_idle((struct fb_info_aty *)p->fb_info); \
    call; \
}

#define DEF_FBCON_ATY(width) \
    DEF_FBCON_ATY_OP(fbcon_aty##width##_putc, \
		     fbcon_cfb##width##_putc(conp, p, c, yy, xx), \
		     int c, int yy, int xx) \
    DEF_FBCON_ATY_OP(fbcon_aty##width##_putcs, \
		     fbcon_cfb##width##_putcs(conp, p, s, count, yy, xx), \
		     const unsigned short *s, int count, int yy, int xx) \
    DEF_FBCON_ATY_OP(fbcon_aty##width##_clear_margins, \
		     fbcon_cfb##width##_clear_margins(conp, p, bottom_only), \
		     int bottom_only) \
 \
const struct display_switch fbcon_aty##width = { \
    setup:		fbcon_cfb##width##_setup, \
    bmove:		fbcon_aty_bmove, \
    clear:		fbcon_aty_clear, \
    putc:		fbcon_aty##width##_putc, \
    putcs:		fbcon_aty##width##_putcs, \
    revc:		fbcon_cfb##width##_revc, \
    clear_margins:	fbcon_aty##width##_clear_margins, \
    fontwidthmask:	FONTWIDTH(4)|FONTWIDTH(8)|FONTWIDTH(12)|FONTWIDTH(16) \
};

#ifdef FBCON_HAS_CFB8
DEF_FBCON_ATY(8)
#endif
#ifdef FBCON_HAS_CFB16
DEF_FBCON_ATY(16)
#endif
#ifdef FBCON_HAS_CFB24
DEF_FBCON_ATY(24)
#endif
#ifdef FBCON_HAS_CFB32
DEF_FBCON_ATY(32)
#endif

