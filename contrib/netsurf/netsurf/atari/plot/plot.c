/*
 * Copyright 2010 Ole Loots <ole@monochrom.net>
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <sys/types.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>

#include <mt_gem.h>

#include "image/bitmap.h"
#include "utils/log.h"
#include "utils/utf8.h"
#include "utils/utils.h"
#include "desktop/gui.h"
#include "desktop/plotters.h"

#include "atari/bitmap.h"
#include "atari/gui.h"
#include "utils/nsoption.h"
#include "atari/plot/plot.h"

void vq_scrninfo(VdiHdl handle, short *work_out);

struct s_view {
    short x;                /* drawing (screen) offset x					*/
    short y;                /* drawing (screen) offset y					*/
    short w;                /* width of buffer, not in sync with vis_w		*/
    short h;                /* height of buffer, not in sync with vis_w		*/
    short vis_x;            /* visible rectangle of the screen buffer		*/
    short vis_y;            /* coords are relative to plot location			*/
    short vis_w;            /* clipped to screen dimensions					*/
    short vis_h;            /* visible width								*/
    struct rect abs_clipping;   /* The toplevel clipping rectangle          */
    struct rect clipping;	/* actual clipping rectangle					*/
    float scale;
};

/*
* Capture the screen at x,y location
* param self instance
* param x absolute screen coords
* param y absolute screen coords
* param w width
* param h height
*
* This creates an snapshot in RGBA format (NetSurf's native format)
*
*/
static struct bitmap * snapshot_create(int x, int y, int w, int h);

/* Garbage collection of the snapshot routine */
/* this should be called after you are done with the data returned by snapshot_create */
/* don't access the screenshot after you called this function */
static void snapshot_suspend(void);

/* destroy memory used by screenshot */
static void snapshot_destroy(void);

#ifdef WITH_8BPP_SUPPORT
static unsigned short sys_pal[256][3]; /*RGB*/
static unsigned short pal[256][3];     /*RGB*/
static char rgb_lookup[256][4];
short web_std_colors[6] = {0, 51, 102, 153, 204, 255};

unsigned short vdi_web_pal[216][3] = {
    {0x000,0x000,0x000}, {0x0c8,0x000,0x000}, {0x190,0x000,0x000}, {0x258,0x000,0x000}, {0x320,0x000,0x000}, {0x3e8,0x000,0x000},
    {0x000,0x0c8,0x000}, {0x0c8,0x0c8,0x000}, {0x190,0x0c8,0x000}, {0x258,0x0c8,0x000}, {0x320,0x0c8,0x000}, {0x3e8,0x0c8,0x000},
    {0x000,0x190,0x000}, {0x0c8,0x190,0x000}, {0x190,0x190,0x000}, {0x258,0x190,0x000}, {0x320,0x190,0x000}, {0x3e8,0x190,0x000},
    {0x000,0x258,0x000}, {0x0c8,0x258,0x000}, {0x190,0x258,0x000}, {0x258,0x258,0x000}, {0x320,0x258,0x000}, {0x3e8,0x258,0x000},
    {0x000,0x320,0x000}, {0x0c8,0x320,0x000}, {0x190,0x320,0x000}, {0x258,0x320,0x000}, {0x320,0x320,0x000}, {0x3e8,0x320,0x000},
    {0x000,0x3e8,0x000}, {0x0c8,0x3e8,0x000}, {0x190,0x3e8,0x000}, {0x258,0x3e8,0x000}, {0x320,0x3e8,0x000}, {0x3e8,0x3e8,0x000},
    {0x000,0x000,0x0c8}, {0x0c8,0x000,0x0c8}, {0x190,0x000,0x0c8}, {0x258,0x000,0x0c8}, {0x320,0x000,0x0c8}, {0x3e8,0x000,0x0c8},
    {0x000,0x0c8,0x0c8}, {0x0c8,0x0c8,0x0c8}, {0x190,0x0c8,0x0c8}, {0x258,0x0c8,0x0c8}, {0x320,0x0c8,0x0c8}, {0x3e8,0x0c8,0x0c8},
    {0x000,0x190,0x0c8}, {0x0c8,0x190,0x0c8}, {0x190,0x190,0x0c8}, {0x258,0x190,0x0c8}, {0x320,0x190,0x0c8}, {0x3e8,0x190,0x0c8},
    {0x000,0x258,0x0c8}, {0x0c8,0x258,0x0c8}, {0x190,0x258,0x0c8}, {0x258,0x258,0x0c8}, {0x320,0x258,0x0c8}, {0x3e8,0x258,0x0c8},
    {0x000,0x320,0x0c8}, {0x0c8,0x320,0x0c8}, {0x190,0x320,0x0c8}, {0x258,0x320,0x0c8}, {0x320,0x320,0x0c8}, {0x3e8,0x320,0x0c8},
    {0x000,0x3e8,0x0c8}, {0x0c8,0x3e8,0x0c8}, {0x190,0x3e8,0x0c8}, {0x258,0x3e8,0x0c8}, {0x320,0x3e8,0x0c8}, {0x3e8,0x3e8,0x0c8},
    {0x000,0x000,0x190}, {0x0c8,0x000,0x190}, {0x190,0x000,0x190}, {0x258,0x000,0x190}, {0x320,0x000,0x190}, {0x3e8,0x000,0x190},
    {0x000,0x0c8,0x190}, {0x0c8,0x0c8,0x190}, {0x190,0x0c8,0x190}, {0x258,0x0c8,0x190}, {0x320,0x0c8,0x190}, {0x3e8,0x0c8,0x190},
    {0x000,0x190,0x190}, {0x0c8,0x190,0x190}, {0x190,0x190,0x190}, {0x258,0x190,0x190}, {0x320,0x190,0x190}, {0x3e8,0x190,0x190},
    {0x000,0x258,0x190}, {0x0c8,0x258,0x190}, {0x190,0x258,0x190}, {0x258,0x258,0x190}, {0x320,0x258,0x190}, {0x3e8,0x258,0x190},
    {0x000,0x320,0x190}, {0x0c8,0x320,0x190}, {0x190,0x320,0x190}, {0x258,0x320,0x190}, {0x320,0x320,0x190}, {0x3e8,0x320,0x190},
    {0x000,0x3e8,0x190}, {0x0c8,0x3e8,0x190}, {0x190,0x3e8,0x190}, {0x258,0x3e8,0x190}, {0x320,0x3e8,0x190}, {0x3e8,0x3e8,0x190},
    {0x000,0x000,0x258}, {0x0c8,0x000,0x258}, {0x190,0x000,0x258}, {0x258,0x000,0x258}, {0x320,0x000,0x258}, {0x3e8,0x000,0x258},
    {0x000,0x0c8,0x258}, {0x0c8,0x0c8,0x258}, {0x190,0x0c8,0x258}, {0x258,0x0c8,0x258}, {0x320,0x0c8,0x258}, {0x3e8,0x0c8,0x258},
    {0x000,0x190,0x258}, {0x0c8,0x190,0x258}, {0x190,0x190,0x258}, {0x258,0x190,0x258}, {0x320,0x190,0x258}, {0x3e8,0x190,0x258},
    {0x000,0x258,0x258}, {0x0c8,0x258,0x258}, {0x190,0x258,0x258}, {0x258,0x258,0x258}, {0x320,0x258,0x258}, {0x3e8,0x258,0x258},
    {0x000,0x320,0x258}, {0x0c8,0x320,0x258}, {0x190,0x320,0x258}, {0x258,0x320,0x258}, {0x320,0x320,0x258}, {0x3e8,0x320,0x258},
    {0x000,0x3e8,0x258}, {0x0c8,0x3e8,0x258}, {0x190,0x3e8,0x258}, {0x258,0x3e8,0x258}, {0x320,0x3e8,0x258}, {0x3e8,0x3e8,0x258},
    {0x000,0x000,0x320}, {0x0c8,0x000,0x320}, {0x190,0x000,0x320}, {0x258,0x000,0x320}, {0x320,0x000,0x320}, {0x3e8,0x000,0x320},
    {0x000,0x0c8,0x320}, {0x0c8,0x0c8,0x320}, {0x190,0x0c8,0x320}, {0x258,0x0c8,0x320}, {0x320,0x0c8,0x320}, {0x3e8,0x0c8,0x320},
    {0x000,0x190,0x320}, {0x0c8,0x190,0x320}, {0x190,0x190,0x320}, {0x258,0x190,0x320}, {0x320,0x190,0x320}, {0x3e8,0x190,0x320},
    {0x000,0x258,0x320}, {0x0c8,0x258,0x320}, {0x190,0x258,0x320}, {0x258,0x258,0x320}, {0x320,0x258,0x320}, {0x3e8,0x258,0x320},
    {0x000,0x320,0x320}, {0x0c8,0x320,0x320}, {0x190,0x320,0x320}, {0x258,0x320,0x320}, {0x320,0x320,0x320}, {0x3e8,0x320,0x320},
    {0x000,0x3e8,0x320}, {0x0c8,0x3e8,0x320}, {0x190,0x3e8,0x320}, {0x258,0x3e8,0x320}, {0x320,0x3e8,0x320}, {0x3e8,0x3e8,0x320},
    {0x000,0x000,0x3e8}, {0x0c8,0x000,0x3e8}, {0x190,0x000,0x3e8}, {0x258,0x000,0x3e8}, {0x320,0x000,0x3e8}, {0x3e8,0x000,0x3e8},
    {0x000,0x0c8,0x3e8}, {0x0c8,0x0c8,0x3e8}, {0x190,0x0c8,0x3e8}, {0x258,0x0c8,0x3e8}, {0x320,0x0c8,0x3e8}, {0x3e8,0x0c8,0x3e8},
    {0x000,0x190,0x3e8}, {0x0c8,0x190,0x3e8}, {0x190,0x190,0x3e8}, {0x258,0x190,0x3e8}, {0x320,0x190,0x3e8}, {0x3e8,0x190,0x3e8},
    {0x000,0x258,0x3e8}, {0x0c8,0x258,0x3e8}, {0x190,0x258,0x3e8}, {0x258,0x258,0x3e8}, {0x320,0x258,0x3e8}, {0x3e8,0x258,0x3e8},
    {0x000,0x320,0x3e8}, {0x0c8,0x320,0x3e8}, {0x190,0x320,0x3e8}, {0x258,0x320,0x3e8}, {0x320,0x320,0x3e8}, {0x3e8,0x320,0x3e8},
    {0x000,0x3e8,0x3e8}, {0x0c8,0x3e8,0x3e8}, {0x190,0x3e8,0x3e8}, {0x258,0x3e8,0x3e8}, {0x320,0x3e8,0x3e8}, {0x3e8,0x3e8,0x3e8}
};
#endif

/* Error code translations: */
static const char * plot_error_codes[] = {
    "None",
    "ERR_BUFFERSIZE_EXCEEDS_SCREEN",
    "ERR_NO_MEM",
    "ERR_PLOTTER_NOT_AVAILABLE"
};

FONT_PLOTTER fplotter = NULL;

extern short vdih;

/* temp buffer for bitmap conversion: */
static void * buf_packed;
static int size_buf_packed;

/* temp buffer for bitmap conversion: */
void * buf_planar;
int size_buf_planar;

/* buffer for plot operations that require device format, */
/* currently used for transparent mfdb blits and snapshots: */
static MFDB buf_scr;
static int size_buf_scr;

/* buffer for std form, used during 8bpp snapshot */
MFDB buf_std;
int size_buf_std;

struct bitmap * buf_scr_compat;

/* intermediate bitmap format */
static HermesFormat vfmt;

/* no screen format here, hermes may not suitable for it */

/* netsurf source bitmap format */
static HermesFormat nsfmt;

struct s_vdi_sysinfo vdi_sysinfo;
/* bit depth of framebuffers: */
static int atari_plot_bpp_virt;
static struct s_view view;

static HermesHandle hermes_pal_h; /* hermes palette handle */
static HermesHandle hermes_cnv_h; /* hermes converter instance handle */
static HermesHandle hermes_res_h;

static short prev_vdi_clip[4];
static struct bitmap snapshot;

VdiHdl atari_plot_vdi_handle = -1;
unsigned long atari_plot_flags;
unsigned long atari_font_flags;

typedef bool (*bitmap_convert_fnc)( struct bitmap * img, int x, int y,
                                   GRECT * clip, uint32_t bg, uint32_t flags, MFDB *out  );
static bitmap_convert_fnc bitmap_convert;

const char* plot_err_str(int i)
{
    return(plot_error_codes[abs(i)]);
}

/**
 * Set line drawing color by passing netsurf XBGR "colour" type.
 *
 * \param vdih The vdi handle
 * \param cin  The netsurf colour value
 */
inline static void vsl_rgbcolor(short vdih, colour cin)
{
	#ifdef WITH_8BPP_SUPPORT
	if( vdi_sysinfo.scr_bpp > 8 ) {
	#endif
		//unsigned short c[4];
		RGB1000 c;
		rgb_to_vdi1000( (unsigned char*)&cin, &c);
		vs_color(vdih, OFFSET_CUSTOM_COLOR, (unsigned short*)&c);
		vsl_color(vdih, OFFSET_CUSTOM_COLOR);
	#ifdef WITH_8BPP_SUPPORT
	} else {
		if( vdi_sysinfo.scr_bpp >= 4 ){
			vsl_color(vdih, RGB_TO_VDI(cin));
		}
		else
			vsl_color(vdih, BLACK);
	}
	#endif
}

/**
 * Set fill color by passing netsurf XBGR "colour" type.
 *
 * \param vdih The vdi handle
 * \param cin  The netsurf colour value
 */
inline static void vsf_rgbcolor(short vdih, colour cin)
{
	#ifdef WITH_8BPP_SUPPORT
	if( vdi_sysinfo.scr_bpp > 8 ) {
	#endif
		RGB1000 c;
		rgb_to_vdi1000( (unsigned char*)&cin, &c);
		vs_color( vdih, OFFSET_CUSTOM_COLOR, (unsigned short*)&c);
		vsf_color( vdih, OFFSET_CUSTOM_COLOR );
	#ifdef WITH_8BPP_SUPPORT
	} else {
		if( vdi_sysinfo.scr_bpp >= 4 ){
			vsf_color( vdih, RGB_TO_VDI(cin) );
		}
		else
			vsf_color( vdih, WHITE );
	}
	#endif
}



/**
 * Get current visible coords
 */
inline static void plot_get_visible_grect(GRECT * out)
{
	out->g_x = view.vis_x;
	out->g_y = view.vis_y;
	out->g_w = view.vis_w;
	out->g_h = view.vis_h;
}



/*	calculate visible area of framebuffer in coords relative to framebuffer */
/*	position																*/
/*  result:																	*/
/*	this function should calculates an rectangle relative to the plot origin*/
/*  and size.																*/
/*	If the ploter coords do not fall within the screen region,				*/
/*	all values of the region are set to zero.								*/
inline static void update_visible_rect(void)
{
	GRECT screen;		// dimensions of the screen
	GRECT frame;		// dimensions of the drawing area
	GRECT common;		// dimensions of intersection of both

	screen.g_x = 0;
	screen.g_y = 0;
	screen.g_w = vdi_sysinfo.scr_w;
	screen.g_h = vdi_sysinfo.scr_h;

    common.g_x = frame.g_x = view.x;
	common.g_y = frame.g_y = view.y;
	common.g_w = frame.g_w = view.w;
	common.g_h = frame.g_h = view.h;

	if (rc_intersect(&screen, &common)) {
		view.vis_w = common.g_w;
		view.vis_h = common.g_h;
		if (view.x < screen.g_x)
			view.vis_x = frame.g_w - common.g_w;
		else
			view.vis_x = 0;
		if (view.y <screen.g_y)
			view.vis_y = frame.g_h - common.g_h;
		else
			view.vis_y = 0;
	} else {
		view.vis_w = view.vis_h = 0;
		view.vis_x = view.vis_y = 0;
	}
}

/* Returns the visible parts of the box (relative coords within framebuffer),*/
/*   	relative to screen coords (normally starting at 0,0 ) 				 */
inline static bool fbrect_to_screen(GRECT box, GRECT * ret)
{
	GRECT out, vis, screen;

	screen.g_x = 0;
	screen.g_y = 0;
	screen.g_w = vdi_sysinfo.scr_w;
	screen.g_h = vdi_sysinfo.scr_h;

	/* get visible region: */
    vis.g_x = view.x;
	vis.g_y = view.y;
	vis.g_w = view.w;
	vis.g_h = view.h;

	if ( !rc_intersect( &screen, &vis ) ) {
		return( false );
	}
	vis.g_x = view.w - vis.g_w;
	vis.g_y = view.h - vis.g_h;

	/* clip box to visible region: */
	if( !rc_intersect(&vis, &box) ) {
		return( false );
	}
	out.g_x = box.g_x + view.x;
	out.g_y = box.g_y + view.y;
	out.g_w = box.g_w;
	out.g_h = box.g_h;
	*ret = out;
	return ( true );
}

/* copy an rectangle from the plot buffer to screen */
/* because this is an on-screen plotter, this is an screen to screen copy. */
bool plot_copy_rect(GRECT src, GRECT dst)
{
	MFDB devmf;
	MFDB scrmf;
	short pxy[8];
	GRECT vis;

	/* clip to visible rect, only needed for onscreen renderer: */
	plot_get_visible_grect(&vis );

	if( !rc_intersect(&vis, &src) )
		return(true);
	if( !rc_intersect(&vis, &dst) )
		return(true);

	src.g_x = view.x + src.g_x;
	src.g_y = view.y + src.g_y;
	dst.g_x = view.x + dst.g_x;
	dst.g_y = view.y + dst.g_y;

	devmf.fd_addr = NULL;
	devmf.fd_w = src.g_w;
	devmf.fd_h = src.g_h;
	devmf.fd_wdwidth = 0;
	devmf.fd_stand = 0;
	devmf.fd_nplanes = 0;
	devmf.fd_r1 = devmf.fd_r2 = devmf.fd_r3 = 0;

	scrmf.fd_addr = NULL;
	scrmf.fd_w = dst.g_w;
	scrmf.fd_h = dst.g_h;
	scrmf.fd_wdwidth = 0 ;
	scrmf.fd_stand = 0;
	scrmf.fd_nplanes = 0;
	scrmf.fd_r1 = scrmf.fd_r2 = scrmf.fd_r3 = 0;

	pxy[0] = src.g_x;
	pxy[1] = src.g_y;
	pxy[2] = pxy[0] + src.g_w-1;
	pxy[3] = pxy[1] + src.g_h-1;
	pxy[4] = dst.g_x;
	pxy[5] = dst.g_y;
	pxy[6] = pxy[4] + dst.g_w-1;
	pxy[7] = pxy[5] + dst.g_h-1;
	plot_lock();
	vro_cpyfm( atari_plot_vdi_handle, S_ONLY, (short*)&pxy, &devmf,  &scrmf);
	plot_unlock();

	return(true);
}

/**
 * Fill the screen info structure.
 *
 */
static struct s_vdi_sysinfo * read_vdi_sysinfo(short vdih, struct s_vdi_sysinfo * info) {

    unsigned long cookie_EdDI=0;
    short out[300];
    memset( info, 0, sizeof(struct s_vdi_sysinfo) );

    info->vdi_handle = vdih;
    if ( tos_getcookie(C_EdDI, &cookie_EdDI) == C_NOTFOUND ) {
        info->EdDiVersion = 0;
    } else {
        info->EdDiVersion = EdDI_version( (void *)cookie_EdDI );
    }

    memset( &out, 0, sizeof(short)*300 );
    vq_extnd( vdih, 0, (short*)&out );
    info->scr_w = out[0]+1;
    info->scr_h = out[1]+1;
    if( out[39] == 2 ) {
        info->scr_bpp = 1;
        info->colors = out[39];
    } else {
        info->colors = out[39];
    }

    memset( &out, 0, sizeof(short)*300 );
    vq_extnd( vdih, 1, (short*)&out );
    info->scr_bpp = out[4];
    info->maxpolycoords = out[14];
    info->maxintin = out[15];
    if( out[30] & 1 ) {
        info->rasterscale = true;
    } else {
        info->rasterscale = false;
    }

    switch( info->scr_bpp ) {
    case 8:
        info->pixelsize=1;
        break;
    case 15:
    case 16:
        info->pixelsize=2;
        break;
    case 24:
        info->pixelsize=3;
        break;
    case 32:
        info->pixelsize=4;
        break;
    case 64:
        info->pixelsize=8;
        break;
    default:
        info->pixelsize=1;
        break;

    }
    info->pitch = info->scr_w * info->pixelsize;
    info->vdiformat = ( (info->scr_bpp <= 8) ? VDI_FORMAT_INTER : VDI_FORMAT_PACK);
    info->screensize = ( info->scr_w * info->pixelsize )  * info->scr_h;

    if( info->EdDiVersion >= EDDI_10 ) {
        memset( &out, 0, sizeof(short)*300 );
        vq_scrninfo(vdih, (short*)&out);
        info->vdiformat = out[0];
        info->clut = out[1];
        info->scr_bpp = out[2];
        info->hicolors =  *((unsigned long*) &out[3]);
        if( info->EdDiVersion >= EDDI_11 ) {
            info->pitch = out[5];
            info->screen = (void *) *((unsigned long *) &out[6]);
        }

        switch( info->clut ) {

        case VDI_CLUT_HARDWARE: {

        }
        break;

        case VDI_CLUT_SOFTWARE: {
            int component; /* red, green, blue, alpha, overlay */
            int num_bit;
            unsigned short *tmp_p;

            /* We can build masks with info here */
            tmp_p = (unsigned short *) &out[16];
            for (component=0; component<5; component++) {
                for (num_bit=0; num_bit<16; num_bit++) {
                    unsigned short val;

                    val = *tmp_p++;

                    if (val == 0xffff) {
                        continue;
                    }

                    switch(component) {
                    case 0:
                        info->mask_r |= 1<< val;
                        break;
                    case 1:
                        info->mask_g |= 1<< val;
                        break;
                    case 2:
                        info->mask_b |= 1<< val;
                        break;
                    case 3:
                        info->mask_a |= 1<< val;
                        break;
                    }
                }
            }
        }

        /* Remove lower green bits for Intel endian screen */
        if ((info->mask_g == ((7<<13)|3)) || (info->mask_g == ((7<<13)|7))) {
            info->mask_g &= ~(7<<13);
        }
        break;

        case VDI_CLUT_NONE:
            break;
        }
    }
}


/*
	Convert an RGB color to an VDI Color
*/
inline void rgb_to_vdi1000(unsigned char * in, RGB1000 *out)
{
    double r = ((double)in[3]/255); /* prozentsatz red   */
    double g = ((double)in[2]/255);	/* prozentsatz green */
    double b = ((double)in[1]/255);	/* prozentsatz blue  */
    out->red = 1000 * r + 0.5;
    out->green = 1000 * g + 0.5;
    out->blue = 1000 * b + 0.5;
    return;
}

inline void vdi1000_to_rgb(unsigned short * in, unsigned char * out)
{
    double r = ((double)in[0]/1000); /* prozentsatz red   */
    double g = ((double)in[1]/1000); /* prozentsatz green */
    double b = ((double)in[2]/1000); /* prozentsatz blue  */
    out[2] = 255 * r + 0.5;
    out[1] = 255 * g + 0.5;
    out[0] = 255 * b + 0.5;
    return;
}


#ifdef WITH_8BPP_SUPPORT
/**
 * Set pixel within an 8 bit VDI standard bitmap.
 */
inline static void set_stdpx( MFDB * dst, int wdplanesz, int x, int y, unsigned char val )
{
	short * buf;
	short whichbit = (1<<(15-(x%16)));

	buf = dst->fd_addr;
	buf += ((dst->fd_wdwidth*(y))+(x>>4));

	*buf = (val&1) ? ((*buf)|(whichbit)) : ((*buf)&~(whichbit));

	buf += wdplanesz;
	*buf = (val&(1<<1)) ? ((*buf)|(whichbit)) : ((*buf)&~(whichbit));

	buf += wdplanesz;
	*buf = (val&(1<<2)) ? ((*buf)|(whichbit)) : ((*buf)&~(whichbit));

	buf += wdplanesz;
	*buf = (val&(1<<3)) ? ((*buf)|(whichbit)) : ((*buf)&~(whichbit));

	buf += wdplanesz;
	*buf = (val&(1<<4)) ? ((*buf)|(whichbit)) : ((*buf)&~(whichbit));

	buf += wdplanesz;
	*buf = (val&(1<<5)) ? ((*buf)|(whichbit)) : ((*buf)&~(whichbit));

	buf += wdplanesz;
	*buf = (val&(1<<6)) ? ((*buf)|(whichbit)) : ((*buf)&~(whichbit));

	buf += wdplanesz;
	*buf = (val&(1<<7)) ? ((*buf)|(whichbit)) : ((*buf)&~(whichbit));
}

/**
 * Read pixel from an 8 bit VDI standard bitmap.
 */
inline static unsigned char get_stdpx(MFDB * dst, int wdplanesz, int x, int y)
{
	unsigned char ret=0;
	short * buf;
	short whichbit = (1<<(15-(x%16)));

	buf = dst->fd_addr;
	buf += ((dst->fd_wdwidth*(y))+(x>>4));

	if( *buf & whichbit )
		ret |= 1;

	buf += wdplanesz;
	if( *buf & whichbit )
		ret |= 2;

	buf += wdplanesz;
	if( *buf & whichbit )
		ret |= 4;

	buf += wdplanesz;
	if( *buf & whichbit )
		ret |= 8;

	buf += wdplanesz;
	if( *buf & whichbit )
		ret |= 16;

	buf += wdplanesz;
	if( *buf & whichbit )
		ret |= 32;

	buf += wdplanesz;
	if( *buf & whichbit )
		ret |= 64;

	buf += wdplanesz;
	if( *buf & whichbit )
		ret |= 128;

	return( ret );
}

/*
	Convert an RGB color into an index into the 216 colors web pallette
*/
inline short rgb_to_666_index(unsigned char r, unsigned char g, unsigned char b)
{
    short ret = 0;
    short i;
    unsigned char rgb[3] = {r,g,b};
    unsigned char tval[3];

    int diff_a, diff_b, diff_c;
    diff_a = abs(r-g);
    diff_b = abs(r-b);
    diff_c = abs(r-b);
    if( diff_a < 2 && diff_b < 2 && diff_c < 2 ) {
        if( (r!=0XFF) && (g!=0XFF) && (g!=0XFF)  ) {
            if( ((r&0xF0)>>4) != 0 )
                //printf("conv gray: %x -> %d\n", ((r&0xF0)>>4) , (OFFSET_CUST_PAL) + ((r&0xF0)>>4) );
                return( (OFFSET_CUST_PAL - OFFSET_WEB_PAL) + ((r&0xF0)>>4) );
        }
    }

    /* convert each 8bit color to 6bit web color: */
    for( i=0; i<3; i++) {
        if(0 == rgb[i] % web_std_colors[1] ) {
            tval[i] = rgb[i] / web_std_colors[1];
        } else {
            int pos = ((short)rgb[i] / web_std_colors[1]);
            if( abs(rgb[i] - web_std_colors[pos]) > abs(rgb[i] - web_std_colors[pos+1]) )
                tval[i] = pos+1;
            else
                tval[i] = pos;
        }
    }
    return(tval[2]*36+tval[1]*6+tval[0]);
}
#endif


static void dump_vdi_info(short vdih)
{
    struct s_vdi_sysinfo temp;
    read_vdi_sysinfo( vdih, &temp );
    printf("struct s_vdi_sysinfo {\n");
    printf("	short vdi_handle: %d\n", temp.vdi_handle);
    printf("	short scr_w: %d \n", temp.scr_w);
    printf("	short scr_h: %d\n", temp.scr_h);
    printf("	short scr_bpp: %d\n", temp.scr_bpp);
    printf("	int colors: %d\n", temp.colors);
    printf("	ulong hicolors: %d\n", temp.hicolors);
    printf("	short pixelsize: %d\n", temp.pixelsize);
    printf("	unsigned short pitch: %d\n", temp.pitch);
    printf("	unsigned short vdiformat: %d\n", temp.vdiformat);
    printf("	unsigned short clut: %d\n", temp.clut);
    printf("	void * screen: 0x0%p\n", temp.screen);
    printf("	unsigned long  screensize: %d\n", temp.screensize);
    printf("	unsigned long  mask_r: 0x0%08x\n", temp.mask_r);
    printf("	unsigned long  mask_g: 0x0%08x\n", temp.mask_g);
    printf("	unsigned long  mask_b: 0x0%08x\n", temp.mask_b);
    printf("	unsigned long  mask_a: 0x0%08x\n", temp.mask_a);
    printf("	short maxintin: %d\n", temp.maxintin);
    printf("	short maxpolycoords: %d\n", temp.maxpolycoords);
    printf("	unsigned long EdDiVersion: 0x0%03x\n", temp.EdDiVersion);
    printf("	unsigned short rasterscale: 0x%2x\n", temp.rasterscale);
    printf("};\n");
}

/**
 * Create an snapshot of the screen image in device format.
 */
static MFDB * snapshot_create_native_mfdb(int x, int y, int w, int h)
{
	MFDB scr;
	short pxy[8];

	/* allocate memory for the snapshot */
	{
		int scr_stride = MFDB_STRIDE( w );
		int scr_size = ( ((scr_stride >> 3) * h) * vdi_sysinfo.scr_bpp );
		if(size_buf_scr == 0 ){
			/* init screen mfdb */
			buf_scr.fd_addr = malloc( scr_size );
			size_buf_scr = scr_size;
		} else {
			if( scr_size >size_buf_scr ) {
				buf_scr.fd_addr = realloc(
					buf_scr.fd_addr, scr_size
				);
				size_buf_scr = scr_size;
			}
		}
		if(buf_scr.fd_addr == NULL ) {
			size_buf_scr = 0;
			return( NULL );
		}
		buf_scr.fd_nplanes = vdi_sysinfo.scr_bpp;
		buf_scr.fd_w = scr_stride;
		buf_scr.fd_h = h;
		buf_scr.fd_wdwidth = scr_stride >> 4;
		assert(buf_scr.fd_addr != NULL );
	}
	init_mfdb( 0, w, h, 0, &scr );
	pxy[0] = x;
	pxy[1] = y;
	pxy[2] = pxy[0] + w-1;
	pxy[3] = pxy[1] + h-1;
	pxy[4] = 0;
	pxy[5] = 0;
	pxy[6] = w-1;
	pxy[7] = h-1;
	vro_cpyfm(
			atari_plot_vdi_handle, S_ONLY, (short*)&pxy,
			&scr,  &buf_scr
	);

	return( &buf_scr );
}

/**
 * Create an snapshot of the screen image in VDI standard format (8 bit).
 */
static MFDB * snapshot_create_std_mfdb(int x, int y, int w, int h)
{
	/* allocate memory for the snapshot */
	{
		int scr_stride = MFDB_STRIDE( w );
		int scr_size = ( ((scr_stride >> 3) * h) * vdi_sysinfo.scr_bpp );
		if(size_buf_std == 0 ){
			/* init screen mfdb */
			buf_std.fd_addr = malloc( scr_size );
			size_buf_std = scr_size;
		} else {
			if( scr_size >size_buf_std ) {
				buf_std.fd_addr = realloc(
					buf_std.fd_addr, scr_size
				);
				size_buf_std = scr_size;
			}
		}
		if(buf_std.fd_addr == NULL ) {
			size_buf_std = 0;
			return( NULL );
		}
		buf_std.fd_nplanes = 8;
		buf_std.fd_w = scr_stride;
		buf_std.fd_h = h;
		buf_std.fd_stand = 1;
		buf_std.fd_wdwidth = scr_stride >> 4;
		assert(buf_std.fd_addr != NULL );
	}
	MFDB * native = snapshot_create_native_mfdb(x,y,w,h );
	assert( native );

	vr_trnfm(atari_plot_vdi_handle, native, &buf_std);
	return( &buf_std );
}

/*
 * Create an snapshot of the screen in netsurf ABGR format
 */
static struct bitmap * snapshot_create(int x, int y, int w, int h)
{
	int err;
	MFDB * native;
	// uint32_t start = clock();

	// FIXME: 	This can be optimized a lot.
	//			1. do not copy the snapshot to the bitmap buffer
	//				when the format of screen and bitmap equals.
	//				just point the bitmap to the native mfdb.
	//			2. if we have eddi 1.1, we could optimize that further
	//				make snapshot_create_native_mfdb just returning a pointer
	//				to the screen.

	native = snapshot_create_native_mfdb(x, y, w, h );

	if(vfmt.bits == 32 )
		goto no_copy;

	/* allocate buffer for result bitmap: */
	if(buf_scr_compat == NULL ) {
		buf_scr_compat = bitmap_create(w, h, 0);
	} else {
		buf_scr_compat = bitmap_realloc( w, h,
			buf_scr_compat->bpp,
			w *buf_scr_compat->bpp,
			BITMAP_GROW,
			buf_scr_compat );
	}

	/* convert screen buffer to ns format: */
	err = Hermes_ConverterRequest( hermes_cnv_h,
			&vfmt,
			&nsfmt
	);
	assert( err != 0 );
	err = Hermes_ConverterCopy( hermes_cnv_h,
		native->fd_addr,
		0,			/* x src coord of top left in pixel coords */
		0,			/* y src coord of top left in pixel coords */
		w, h,
		native->fd_w * vdi_sysinfo.pixelsize, /* stride as bytes */
		buf_scr_compat->pixdata,
		0,			/* x dst coord of top left in pixel coords */
		0,			/* y dst coord of top left in pixel coords */
		w, h,
		bitmap_get_rowstride(buf_scr_compat) /* stride as bytes */
	);
	assert( err != 0 );
	return( (struct bitmap * )buf_scr_compat );

no_copy:

	snapshot.width = w;
	snapshot.height = h;
	snapshot.pixdata = native->fd_addr;
	snapshot.native = *native;
	snapshot.rowstride = MFDB_STRIDE( w )*4;

	uint32_t row, col;
	for (row = 0; row<(uint32_t)h; row++) {
		// fd_w matches stride!
		uint32_t *rowptr = ((uint32_t*)native->fd_addr + ((row*native->fd_w)));
		for (col=0; col<(uint32_t)w; col++) {
			*(rowptr+col) = (*(rowptr+col)<<8);
		}
	}
	return( &snapshot );
}

/**
 * Notify the snapshot interface that the last snapshot is no longer in use.
 */
static void snapshot_suspend(void)
{
	if(size_buf_scr > CONV_KEEP_LIMIT  ) {
		buf_scr.fd_addr = realloc(
			buf_scr.fd_addr, CONV_KEEP_LIMIT
		);
		if(buf_scr.fd_addr != NULL ) {
			size_buf_scr = CONV_KEEP_LIMIT;
		} else {
			size_buf_scr = 0;
		}
	}

#ifdef WITH_8BPP_SUPPORT
	if(size_buf_std > CONV_KEEP_LIMIT  ) {
		buf_std.fd_addr = realloc(
			buf_std.fd_addr, CONV_KEEP_LIMIT
		);
		if(buf_std.fd_addr != NULL ) {
			size_buf_std = CONV_KEEP_LIMIT;
		} else {
			size_buf_std = 0;
		}
	}
#endif

	if(buf_scr_compat != NULL ) {
		size_t bs = bitmap_buffer_size(buf_scr_compat );
		if( bs > CONV_KEEP_LIMIT ) {
			int w = 0;
			int h = 1;
			w = (CONV_KEEP_LIMIT /buf_scr_compat->bpp);
			assert( CONV_KEEP_LIMIT == w*buf_scr_compat->bpp );
			buf_scr_compat = bitmap_realloc( w, h,
				buf_scr_compat->bpp,
				CONV_KEEP_LIMIT, BITMAP_SHRINK,buf_scr_compat
			);
		}
	}
}

/**
 * Shut down the snapshot interface.
 */
static void snapshot_destroy(void)
{

	free(buf_scr.fd_addr);
	if( buf_scr_compat != NULL) {
		bitmap_destroy(buf_scr_compat);
	}

	buf_scr.fd_addr = NULL;
	buf_scr_compat = NULL;

#ifdef WITH_8BPP_SUPPORT
	free(buf_std.fd_addr);
	buf_std.fd_addr = NULL;
#endif
}


inline static uint32_t ablend(uint32_t pixel, uint32_t scrpixel)
{
    int opacity = pixel & 0xFF;
    int transp = 0x100 - opacity;
    uint32_t rb, g;
	pixel >>= 8;
	scrpixel >>= 8;
	rb = ((pixel & 0xFF00FF) * opacity +
    	(scrpixel & 0xFF00FF) * transp) >> 8;
    g  = ((pixel & 0x00FF00) * opacity +
    	(scrpixel & 0x00FF00) * transp) >> 8;

    return ((rb & 0xFF00FF) | (g & 0xFF00)) << 8;
}

/*
	Alpha blends an image, using one pixel as the background.
	The bitmap receives the result.
*/
inline static bool ablend_pixel(struct bitmap * img, uint32_t bg, GRECT * clip)
{
	uint32_t * imgrow;
	int img_x, img_y, img_stride;

	img_stride= bitmap_get_rowstride(img);

	for( img_y = 0; img_y < clip->g_h; img_y++) {
		imgrow = (uint32_t *)(img->pixdata + (img_stride * img_y));
		for( img_x = 0; img_x < clip->g_w; img_x++ ) {
			imgrow[img_x] = ablend( imgrow[img_x], bg );
		}
	}
	return(true);
}


/*
	Aplha blends the foreground image (img) onto the
	background images (bg). The background receives the blended
	image pixels.
*/
inline static bool ablend_bitmap( struct bitmap * img, struct bitmap * bg,
						GRECT * img_clip, GRECT * bg_clip )
{
	uint32_t * imgrow;
	uint32_t * screenrow;
	int img_x, img_y, bg_x, bg_y, img_stride, bg_stride;

	bg_clip = bg_clip;
	img_stride= bitmap_get_rowstride(img);
	bg_stride = bitmap_get_rowstride(bg);

	for( img_y = img_clip->g_y, bg_y = 0; bg_y < img_clip->g_h; bg_y++, img_y++) {
		imgrow = (uint32_t *)(img->pixdata + (img_stride * img_y));
		screenrow = (uint32_t *)(bg->pixdata + (bg_stride * bg_y));
		for( img_x = img_clip->g_x, bg_x = 0; bg_x < img_clip->g_w; bg_x++, img_x++ ) {

			// when the pixel isn't fully transparent,...:
			if( (imgrow[img_x] & 0x0FF) != 0 ){
				screenrow[bg_x] = ablend( imgrow[img_x], screenrow[bg_x]);
			}

			// FIXME, maybe this loop would be faster??:
			// ---
			//if( (imgrow[img_x] & 0x0FF) != 0xFF ){
			//	imgrow[bg_x] = ablend( imgrow[img_x], screenrow[bg_x]);
			//}

			// or maybe even this???
			// ---
			//if(  (imgrow[img_x] & 0x0FF) == 0xFF ){
			//	screenrow[bg_x] = imgrow[img_x];
			//} else if( (imgrow[img_x] & 0x0FF) != 0x00 ) {
			//	screenrow[bg_x] = ablend( imgrow[img_x], screenrow[bg_x]);
			//}
		}
	}
	return(false);
}


#ifdef WITH_8BPP_SUPPORT
/**
 * Convert an bitmap to an 8 bit device dependant MFDB
 * \param img the bitmap (only tested with 32bit bitmaps)
 * \param x screen coord of the background
 * \param y screen coord of the background
 * \param clip the region of the image that get's converted
 * \param bg the background used for cheap transparency
 * \param flags
 * \param out receives the converted bitmap (still owned by the plot API)
 *
 */
static bool bitmap_convert_8(struct bitmap * img, int x,
							int y, GRECT * clip, uint32_t bg, uint32_t flags,
							MFDB *out )
{
	MFDB native;
	MFDB stdform;
	int dststride;						/* stride of dest. image */
	int dstsize;						/* size of dest. in byte */
	int err;
	int bw, bh;
	int process_w, process_h;
	struct bitmap * scrbuf = NULL;
	struct bitmap * source;
	bool cache =  ( flags & BITMAPF_BUFFER_NATIVE );
	bool opaque = bitmap_get_opaque( img );

	if( opaque == false ){
		if( ( (atari_plot_flags & PLOT_FLAG_TRANS) == 0)
			&&
			((flags & (BITMAPF_MONOGLYPH|BITMAPF_BUFFER_NATIVE))==0) ){
			opaque = true;
		}
	}

	assert( clip->g_h > 0 );
	assert( clip->g_w > 0 );

	process_w = bw = bitmap_get_width( img );
	process_h = bh = bitmap_get_height( img );

	// The converted bitmap can be saved for subsequent blits, when
	// the bitmap is fully opaque

	if( (opaque == true) || (flags & BITMAPF_BUFFER_NATIVE ) ){
		if( img->converted == true ){
			*out = img->native;
			return( 0 );
		}
		if( ( flags & BITMAPF_MONOGLYPH ) == 0 ){
			cache = true;
		}
	}
	if( ( flags & BITMAPF_MONOGLYPH ) != 0 ){
			assert(cache == false);
		}

	/* (re)allocate buffer for out image: */
	/* altough the buffer is named "buf_packed" on 8bit systems */
	/* it's not... */
	if( cache == false ){
		// the size of the output will match the size of the clipping:
		dststride = MFDB_STRIDE( clip->g_w );
		dstsize = ( ((dststride >> 3) * clip->g_h) * atari_plot_bpp_virt);
		if( dstsize > size_buf_packed) {
			int blocks = (dstsize / (CONV_BLOCK_SIZE-1))+1;
			if( buf_packed == NULL )
				buf_packed =(void*)malloc( blocks * CONV_BLOCK_SIZE);
			 else
				buf_packed =(void*)realloc(buf_packed,blocks * CONV_BLOCK_SIZE);
			assert( buf_packed );
			if( buf_packed == NULL ) {
				return( 0-ERR_NO_MEM );
			}
			size_buf_packed = blocks * CONV_BLOCK_SIZE;
		}
		native.fd_addr = buf_packed;
	}
	else {
		// the output image will be completly saved, so size of the output
		// image will match the input image size.
		dststride = MFDB_STRIDE( bw );
		dstsize = ( ((dststride >> 3) * bh) * atari_plot_bpp_virt);
		assert( out->fd_addr == NULL );
		native.fd_addr = (void*)malloc( dstsize );
		if (native.fd_addr == NULL){
			if (scrbuf != NULL)
				bitmap_destroy(scrbuf);
			return( 0-ERR_NO_MEM );
		}
	}


	/*
		on 8 bit systems we must convert the TC (ABGR) image
		to vdi standard format. ( only tested for 256 colors )
		and then convert it to native format with v_trnfm()
	*/
	// realloc mem for stdform
	if( opaque == false ){
		// point image to snapshot buffer, otherwise allocate mem
		MFDB * bg = snapshot_create_std_mfdb(x, y, clip->g_w, clip->g_h);
		stdform.fd_addr = bg->fd_addr;
		bh = clip->g_h;
	} else {
		if( dstsize > size_buf_planar) {
			int blocks = (dstsize / (CONV_BLOCK_SIZE-1))+1;
			if( buf_planar == NULL )
				buf_planar =(void*)malloc( blocks * CONV_BLOCK_SIZE );
			 else
				buf_planar =(void*)realloc(buf_planar, blocks * CONV_BLOCK_SIZE);
			assert(buf_planar);
			if( buf_planar == NULL ) {
				return( 0-ERR_NO_MEM );
			}
			size_buf_planar = blocks * CONV_BLOCK_SIZE;
		}
		stdform.fd_addr = buf_planar;
	}
	stdform.fd_w = dststride;
	stdform.fd_h = bh;
	stdform.fd_wdwidth = dststride >> 4;
	stdform.fd_stand = 1;
	stdform.fd_nplanes = (short)atari_plot_bpp_virt;
	stdform.fd_r1 = stdform.fd_r2 = stdform.fd_r3 = 0;

	int img_stride = bitmap_get_rowstride(img);
	uint32_t prev_pixel = 0x12345678; //TODO: check for collision in first pixel
	unsigned long col = 0;
	unsigned char val = 0;
	uint32_t * row;
	uint32_t pixel;
	int wdplanesize = stdform.fd_wdwidth*stdform.fd_h;

	if( opaque == false ){
		// apply transparency and convert to vdi std format
		unsigned long bgcol = 0;
		unsigned char prev_col = 0;
		for( y=0; y<clip->g_h; y++ ){
			row = (uint32_t *)(img->pixdata + (img_stride * (y+clip->g_y)));
			for( x=0; x<clip->g_w; x++ ){
				pixel = row[x+clip->g_x];
				if( (pixel&0xFF) == 0 ){
					continue;
				}
				if( (pixel&0xFF) < 0xF0 ){
					col = get_stdpx( &stdform, wdplanesize,x,y );
					if( (col != prev_col) || (y == 0) )
						bgcol = (((rgb_lookup[col][2] << 16) | (rgb_lookup[col][1] << 8) | (rgb_lookup[col][0]))<<8);
					if( prev_col != col || prev_pixel != pixel ){
						prev_col = col;
						pixel = ablend( pixel, bgcol );
						prev_pixel = pixel;
						pixel = pixel >> 8;
						/* convert pixel value to vdi color index: */
						col = ( ((pixel&0xFF)<<16)
									| (pixel&0xFF00)
									| ((pixel&0xFF0000)>>16) );
						val = RGB_TO_VDI( col );
					}
					set_stdpx( &stdform, wdplanesize, x, y, val );
				} else {
					if( pixel != prev_pixel ){
						/* convert pixel value to vdi color index: */
						pixel = pixel >> 8;
						col = ( ((pixel&0xFF)<<16)
									| (pixel&0xFF00)
									| ((pixel&0xFF0000)>>16) );
						val = RGB_TO_VDI( col );
						prev_pixel = pixel;
					}
					set_stdpx( &stdform, wdplanesize, x, y, val );
				}
			}
		}
		// adjust output position:
		clip->g_x = 0;
		clip->g_y = 0;
	} else {
		// convert the whole image data to vdi std format.
		for( y=0; y < bh; y++ ){
			row = (uint32_t *)(img->pixdata + (img_stride * y));
			for( x=0; x < bw; x++ ){
				pixel = row[x];
				if( pixel != prev_pixel ){
					/* convert pixel value to vdi color index: */
					pixel = pixel >> 8;
					col = ( ((pixel&0xFF)<<16)
								| (pixel&0xFF00)
								| ((pixel&0xFF0000)>>16) );
					val = RGB_TO_VDI( col );
					prev_pixel = pixel;
				}
				set_stdpx( &stdform, wdplanesize, x, y, val );
			}
		}
	}

	// convert into native format:
	native.fd_w = stdform.fd_w;
	native.fd_h = stdform.fd_h;
	native.fd_wdwidth = stdform.fd_wdwidth;
	native.fd_stand = 0;
	native.fd_nplanes = (short)atari_plot_bpp_virt;
	native.fd_r1 = native.fd_r2 = native.fd_r3 = 0;
	vr_trnfm(atari_plot_vdi_handle, &stdform, &native );
	*out = native;
	if( cache == true ){
		img->native = native;
		img->converted = true;
	}

	return(0);
}
#endif


/*
*
* Convert bitmap to the native screen format
*	img:	the bitmap
*	x:		coordinate where the bitmap REGION (described in clip)
*			shall be drawn (screen coords)
*	y: 		coordinate where the bitmap REGION (described in clip)
*			shall be drawn (screen coords)
*	clip:	which area of the bitmap shall be drawn
*	bg: 	background color
*	flags:	blit flags
*	out:	the result MFDB
*/
static bool bitmap_convert_tc(struct bitmap * img, int x, int y,
						GRECT * clip, uint32_t bg, uint32_t flags, MFDB *out  )
{
	int dststride;						/* stride of dest. image */
	int dstsize;						/* size of dest. in byte */
	int err;
	int bw, bh;
	struct bitmap * scrbuf = NULL;
	struct bitmap * source = NULL;
	bool cache =  ( flags & BITMAPF_BUFFER_NATIVE );
	bool opaque = bitmap_get_opaque( img );

	if( opaque == false ){
		if( ( (atari_plot_flags & PLOT_FLAG_TRANS) == 0)
			&&
			((flags & (BITMAPF_MONOGLYPH|BITMAPF_BUFFER_NATIVE))==0) ){
			opaque = true;
		}
	}


	assert( clip->g_h > 0 );
	assert( clip->g_w > 0 );

	bw = bitmap_get_width( img );
	bh = bitmap_get_height( img );

	// The converted bitmap can be saved for subsequent blits, WHEN:
	// A.) the bitmap is fully opaque OR
	// B.) the bitmap is completly inside the window
	// the latter one is important for alpha blits,
	// because we must get the window background to apply transparency
	// If the image is not completly within the window,
	// we can't get the whole background for the image.
	// this only works if the image isn't used at several different places.
	// In fact in case of alpha bitmap caching it is only used for the
	// toolbar buttons right now.

	if( (opaque == true) || (flags & BITMAPF_BUFFER_NATIVE ) ){
		if( img->converted == true ){
			*out = img->native;
			return( 0 );
		}
		if( ( flags & BITMAPF_MONOGLYPH ) == 0 ){
			cache = true;
		}
	}

	/* rem. if eddi xy is installed, we could directly access the screen! */
	/* apply transparency to the image: */
	if (( opaque == false )) {
		/* copy the screen to an temp buffer: */
		if ((flags & BITMAPF_BUFFER_NATIVE) == 0) {
			scrbuf = snapshot_create(x, y, clip->g_w, clip->g_h);
			if( scrbuf != NULL ) {

				assert( clip->g_w <= bw );
				assert( clip->g_h <= bh );

				// copy blended pixels to the screen buffer:
				ablend_bitmap( img, scrbuf, clip, NULL );
				/* adjust size which gets converted: */
				bw = clip->g_w;
				bh = clip->g_h;
				/* adjust output position: */
				clip->g_x = 0;
				clip->g_y = 0;
				/* set the source of conversion: */
				source = scrbuf;
			}
		} else {
			/*
				The whole bitmap can be transformed to an mfdb
				(and get's cached)
			*/
			GRECT region = { 0, 0, bw, bh };
			ablend_pixel( img, bg, &region );
			source = img;
		}
	} else {
		source = img;
	}
	/* (re)allocate buffer for converted image: */
	dststride = MFDB_STRIDE(bw);
	dstsize = ( ((dststride >> 3) * bh) * atari_plot_bpp_virt );
	if (cache == false) {
		if (dstsize > size_buf_packed) {
			int blocks = (dstsize / (CONV_BLOCK_SIZE-1))+1;
			if( buf_packed == NULL )
				buf_packed =(void*)malloc( blocks * CONV_BLOCK_SIZE );
			 else
				buf_packed =(void*)realloc(buf_packed,
											blocks * CONV_BLOCK_SIZE);
			assert( buf_packed );
			if( buf_packed == NULL ) {
				if( scrbuf != NULL )
					bitmap_destroy( scrbuf );
				return( 0-ERR_NO_MEM );
			}
			size_buf_packed = blocks * CONV_BLOCK_SIZE;
		}
		out->fd_addr = buf_packed;
	} else {
		assert( out->fd_addr == NULL );
		out->fd_addr = (void*)malloc( dstsize );
		if( out->fd_addr == NULL ){
			if( scrbuf != NULL )
				bitmap_destroy( scrbuf );
			return( 0-ERR_NO_MEM );
		}
	}

	out->fd_w = dststride;
	out->fd_h = bh;
	out->fd_wdwidth = dststride >> 4;
	out->fd_stand = 0;
	out->fd_nplanes = (short)atari_plot_bpp_virt;
	out->fd_r1 = out->fd_r2 = out->fd_r3 = 0;

	err = Hermes_ConverterRequest(
			hermes_cnv_h,
			&nsfmt,
			&vfmt
	);
	assert( err != 0 );

	// FIXME: here we can use the same optimization which is used for
	// the snapshot creation.

	/* convert image to virtual format: */
	err = Hermes_ConverterCopy( hermes_cnv_h,
		source->pixdata,
		0,					/* x src coord of top left in pixel coords */
		0,					/* y src coord of top left in pixel coords */
		bw, bh,
		source->rowstride, 	/* stride as bytes */
		out->fd_addr,
		0,					/* x dst coord of top left in pixel coords */
		0,					/* y dst coord of top left in pixel coords */
		bw, bh,
		(dststride >> 3) *  atari_plot_bpp_virt 			/* stride as bytes */
	);
	assert( err != 0 );

	if( cache == true ){
		img->native = *out;
		img->converted = true;
	}
	return( 0 );

}

 inline static void convert_bitmap_done(void)
{
	if (size_buf_packed > CONV_KEEP_LIMIT) {
		/* free the mem if it was an large allocation ... */
		buf_packed = realloc(buf_packed, CONV_KEEP_LIMIT);
		size_buf_packed = CONV_KEEP_LIMIT;
	}
}


bool plot_blit_bitmap(struct bitmap * bmp, int x, int y,
		unsigned long bg, unsigned long flags )
{
	MFDB src_mf;
	MFDB scrmf;
	short pxy[8];
	GRECT off, clip, vis;
	int screen_x, screen_y;

	src_mf.fd_addr = NULL;
	scrmf.fd_addr = NULL;

	off.g_x = x;
	off.g_y = y;
	off.g_h = bmp->height;
	off.g_w = bmp->width;

	// clip plotter clip rectangle:
	clip.g_x = view.clipping.x0;
	clip.g_y = view.clipping.y0;
	clip.g_w = view.clipping.x1 - view.clipping.x0;
	clip.g_h = view.clipping.y1 - view.clipping.y0;

	if( !rc_intersect( &clip, &off) ) {
		return(true);
	}

	// clip the visible rectangle of the plot area
	// this is the area of the plotter which falls into
	// screen region:
	plot_get_visible_grect(&vis);
	if( !rc_intersect( &vis, &off) ) {
		return(true);
	}

	screen_x = view.x + off.g_x;
	screen_y = view.y + off.g_y;

	// convert the clipping relative to bitmap:
	off.g_x = off.g_x - x;
	off.g_y = off.g_y - y;
	assert( (off.g_x >= 0) && (off.g_y >= 0) );

	/* Convert the Bitmap to native screen format - ready for output.	*/
	/* This includes blending transparent pixels:					 	*/
	if (bitmap_convert(bmp, screen_x, screen_y, &off, bg, flags, &src_mf)
		 != 0 ) {
		return(true);
	}

	// setup the src region:
	pxy[0] = off.g_x;
	pxy[1] = off.g_y;
	pxy[2] = off.g_x + off.g_w-1;
	pxy[3] = off.g_y + off.g_h-1;

	// setup the target region:
	pxy[4] = screen_x;
	pxy[5] = screen_y;
	pxy[6] = screen_x + off.g_w-1;
	pxy[7] = screen_y + off.g_h-1;

	vro_cpyfm(atari_plot_vdi_handle, S_ONLY, (short*)&pxy, &src_mf,  &scrmf);
	convert_bitmap_done();
	snapshot_suspend();
	return(true);
}

bool plot_blit_mfdb(GRECT * loc, MFDB * insrc, short fgcolor,
						uint32_t flags)
{

	MFDB screen, tran;
	MFDB * src;
	short pxy[8];
	short c[2] = {fgcolor, 0};
	GRECT off;

	plot_get_clip_grect(&off);
	if( rc_intersect(loc, &off) == 0 ){
		return( 1 );
	}

	init_mfdb( 0, loc->g_w, loc->g_h, 0, &screen );
//
//	if( insrc->fd_stand){
//		printf("st\n");
//		int size = init_mfdb( insrc->fd_nplanes, loc->g_w, loc->g_h,
//			MFDB_FLAG_NOALLOC,
//			&tran
//		);
//		if( size_buf_scr == 0 ){
//			buf_scr.fd_addr = malloc( size );
//			size_buf_scr = size;
//		} else {
//			if( size > size_buf_scr ) {
//				buf_scr.fd_addr = realloc(
//					buf_scr.fd_addr, size
//				);
//				size_buf_scr = size;
//			}
//		}
//		tran.fd_addr = buf_scr.fd_addr;
//		vr_trnfm(atari_plot_vdi_handle, insrc, &tran );
//		src = &tran;
//	} else {
		src = insrc;
//	}

	pxy[0] = off.g_x - loc->g_x;
	pxy[1] = off.g_y - loc->g_y;
	pxy[2] = pxy[0] + off.g_w - 1;
	pxy[3] = pxy[1] + off.g_h - 1;
	pxy[4] = view.x + off.g_x;
	pxy[5] = view.y + off.g_y;
	pxy[6] = pxy[4] + off.g_w-1;
	pxy[7] = pxy[5] + off.g_h-1;


	if( flags & PLOT_FLAG_TRANS && src->fd_nplanes == 1){
		vrt_cpyfm(atari_plot_vdi_handle, MD_REPLACE/*MD_TRANS*/, (short*)pxy, src, &screen, (short*)&c);
	} else {
		/* this method only plots transparent bitmaps, right now... */
	}
	return( 1 );
}

/*
Init screen and font driver objects.
Returns non-zero value > -1 when the objects could be succesfully created.
Returns value < 0 to indicate an error
*/

int plot_init(char * fdrvrname)
{

    GRECT loc_pos= {0,0,360,400};
    int err=0,i;

    if( nsoption_int(atari_dither) == 1)
        atari_plot_flags |= PLOT_FLAG_DITHER;
    if( nsoption_int(atari_transparency) == 1 )
        atari_plot_flags |= PLOT_FLAG_TRANS;
    if( nsoption_int(atari_font_monochrom) == 1 )
        atari_font_flags |= FONTPLOT_FLAG_MONOGLYPH;

	if(atari_plot_vdi_handle == -1) {

		short dummy;
		short work_in[12] = {Getrez()+2,1,1,1,1,1,1,1,1,1,2,1};
        short work_out[57];
        atari_plot_vdi_handle=graf_handle(&dummy, &dummy, &dummy, &dummy);
        v_opnvwk(work_in, &atari_plot_vdi_handle, work_out);
        LOG(("Plot VDI handle: %d", atari_plot_vdi_handle));
    }
	read_vdi_sysinfo(atari_plot_vdi_handle, &vdi_sysinfo);
    if(verbose_log) {
        dump_vdi_info(atari_plot_vdi_handle) ;
        dump_font_drivers();
    }

    fplotter = new_font_plotter(atari_plot_vdi_handle, fdrvrname,
                                atari_font_flags, &err);
    if(err) {
        const char * desc = plot_err_str(err);
        die(("Unable to load font plotter %s -> %s", fdrvrname, desc ));
    }

    memset(&view, 0, sizeof(struct s_view));
    atari_plot_bpp_virt = vdi_sysinfo.scr_bpp;
	view.x = loc_pos.g_x;
	view.y = loc_pos.g_y;
	view.w = loc_pos.g_w;
	view.h = loc_pos.g_h;
	size_buf_packed = 0;
	size_buf_planar = 0;
	buf_packed = NULL;
	buf_planar = NULL;
	if( vdi_sysinfo.vdiformat == VDI_FORMAT_PACK  ) {
		atari_plot_bpp_virt = vdi_sysinfo.scr_bpp;
	} else {
		atari_plot_bpp_virt = 8;
	}

    plot_set_scale(1.0);
	update_visible_rect();

	struct rect clip;
	clip.x0 = 0;
	clip.y0 = 0;
	clip.x1 = view.w;
	clip.y1 = view.h;
	plot_clip(&clip);

	assert(Hermes_Init());

#ifdef WITH_8BPP_SUPPORT
	bitmap_convert = (vdi_sysinfo.scr_bpp > 8) ? bitmap_convert_tc : bitmap_convert_8;

	/* Setup color lookup tables and palette */
	i = 0;
	unsigned char * col;
	unsigned char rgbcol[4];
	unsigned char graytone=0;
	if( vdi_sysinfo.scr_bpp <= 8 ){
		for( i=0; i<=255; i++ ) {

			// get the current color and save it for restore:
			vq_color(atari_plot_vdi_handle, i, 1, (unsigned short*)&sys_pal[i][0] );
			if( i<OFFSET_WEB_PAL ) {
				pal[i][0] = sys_pal[i][0];
		 		pal[i][1] = sys_pal[i][1];
				pal[i][2] = sys_pal[i][2];
			} else if( vdi_sysinfo.scr_bpp >= 8 ) {
				if ( i < OFFSET_CUST_PAL ){
					pal[i][0] = vdi_web_pal[i-OFFSET_WEB_PAL][0];
					pal[i][1] = vdi_web_pal[i-OFFSET_WEB_PAL][1];
					pal[i][2] = vdi_web_pal[i-OFFSET_WEB_PAL][2];
					//set the new palette color to websafe value:
					vs_color(atari_plot_vdi_handle, i, &pal[i][0]);
				}
				if( i >= OFFSET_CUST_PAL && i<OFFSET_CUST_PAL+16 ) {
					/* here we define 20 additional gray colors... */
					rgbcol[1] = rgbcol[2] = rgbcol[3] = ((graytone&0x0F) << 4);
					rgb_to_vdi1000( &rgbcol[0], &pal[i][0] );
					vs_color(atari_plot_vdi_handle, i, &pal[i][0]);
					graytone++;
				}

			}
			vdi1000_to_rgb( &pal[i][0],  &rgb_lookup[i][0] );
		}

	} else {
		/* no need to change the palette - its application specific */
	}
#else
	bitmap_convert = bitmap_convert_tc;
#endif

	/* Setup Hermes conversion handles */
	unsigned long hermesflags = (atari_plot_flags & PLOT_FLAG_DITHER) ? HERMES_CONVERT_DITHER : 0;
	hermes_cnv_h = Hermes_ConverterInstance(hermesflags);
	assert( hermes_cnv_h );
	hermes_res_h = Hermes_ConverterInstance(hermesflags);
	assert( hermes_res_h );

	/* set up the src & dst format: */
	/* netsurf uses RGBA ... */
	nsfmt.a = 0xFFUL;
	nsfmt.b = 0x0FF00UL;
	nsfmt.g = 0x0FF0000UL;
	nsfmt.r = 0x0FF000000UL;
	nsfmt.bits = 32;
	nsfmt.indexed = false;
	nsfmt.has_colorkey = false;

	vfmt.r = vdi_sysinfo.mask_r;
	vfmt.g = vdi_sysinfo.mask_g;
	vfmt.b = vdi_sysinfo.mask_b;
	vfmt.a = vdi_sysinfo.mask_a;
	vfmt.bits = atari_plot_bpp_virt;
	vfmt.indexed = (atari_plot_bpp_virt <= 8) ? 1 : 0;
	vfmt.has_colorkey = 0;

    return( err );
}

int plot_finalise( void )
{
	int i=0;

    delete_font_plotter(fplotter);

#ifdef WITH_8BPP_SUPPORT
	if (vfmt.indexed) {
		for (i=OFFSET_WEB_PAL; i<OFFSET_CUST_PAL+16; i++) {
			vs_color(atari_plot_vdi_handle, i, &sys_pal[i][0]);
		}
	}
#endif

	/* close Hermes stuff: */
	Hermes_ConverterReturn(hermes_cnv_h);
	Hermes_Done();

	/* free up temporary buffers */
	free(buf_packed );
	free(buf_planar);
	snapshot_destroy();
}

bool plot_lock(void)
{
	if ((atari_plot_flags & PLOT_FLAG_LOCKED) != 0)
		return(true);
	if( !wind_update(BEG_UPDATE|0x100) )
		return(false);
	if( !wind_update(BEG_MCTRL|0x100) ){
		wind_update(END_UPDATE);
		return(false);
	}
	atari_plot_flags |= PLOT_FLAG_LOCKED;
	graf_mouse(M_OFF, NULL);
	return(true);
}

bool plot_unlock(void)
{
	if( (atari_plot_flags & PLOT_FLAG_LOCKED) == 0 )
		return(true);
	wind_update(END_MCTRL);
	wind_update(END_UPDATE);
	graf_mouse(M_ON, NULL);
	vs_clip_off(atari_plot_vdi_handle);
	atari_plot_flags &=  ~PLOT_FLAG_LOCKED;
	return(false);
}

bool plot_rectangle(int x0, int y0, int x1, int y1,
                     const plot_style_t *pstyle )
{
	short pxy[4];
	GRECT r, rclip, sclip;
	int sw = pstyle->stroke_width;
	uint32_t lt;

	/* current canvas clip: */
	rclip.g_x = view.clipping.x0;
	rclip.g_y = view.clipping.y0;
	rclip.g_w = view.clipping.x1 - view.clipping.x0;
	rclip.g_h = view.clipping.y1 - view.clipping.y0;

	/* physical clipping: */
	sclip.g_x = rclip.g_x;
	sclip.g_y = rclip.g_y;
	sclip.g_w = view.vis_w;
	sclip.g_h = view.vis_h;

	rc_intersect(&sclip, &rclip);
	r.g_x = x0;
	r.g_y = y0;
	r.g_w = x1 - x0;
	r.g_h = y1 - y0;

	if (!rc_intersect( &rclip, &r )) {
		return(true);
	}
	if (pstyle->stroke_type != PLOT_OP_TYPE_NONE) {
		/*
			manually draw the line, because we do not need vdi clipping
			for vertical / horizontal line draws.
		*/
		if( sw == 0)
			sw = 1;

		NSLT2VDI(lt, pstyle);
		vsl_type(atari_plot_vdi_handle, (lt&0x0F));
		/*
			if the line style is not available within VDI system,
			define own style:
		*/
		if( (lt&0x0F) == 7 ){
			vsl_udsty(atari_plot_vdi_handle, ((lt&0xFFFF00) >> 8));
		}
		vsl_width(atari_plot_vdi_handle, (short)sw );
		vsl_rgbcolor(atari_plot_vdi_handle, pstyle->stroke_colour);
		/* top border: */
		if( r.g_y == y0){
			pxy[0] = view.x + r.g_x;
			pxy[1] = view.y + r.g_y ;
			pxy[2] = view.x + r.g_x + r.g_w;
			pxy[3] = view.y + r.g_y;
			v_pline(atari_plot_vdi_handle, 2, (short *)&pxy);
		}

		/* right border: */
		if( r.g_x + r.g_w == x1 ){
			pxy[0] = view.x + r.g_x + r.g_w;
			pxy[1] = view.y + r.g_y;
			pxy[2] = view.x + r.g_x + r.g_w;
			pxy[3] = view.y + r.g_y + r.g_h;
			v_pline(atari_plot_vdi_handle, 2, (short *)&pxy);
		}

		/* bottom border: */
		if( r.g_y+r.g_h == y1 ){
			pxy[0] = view.x + r.g_x;
			pxy[1] = view.y + r.g_y+r.g_h;
			pxy[2] = view.x + r.g_x+r.g_w;
			pxy[3] = view.y + r.g_y+r.g_h;
			v_pline(atari_plot_vdi_handle, 2, (short *)&pxy);
		}

		/* left border: */
		if( r.g_x == x0 ){
			pxy[0] = view.x + r.g_x;
			pxy[1] = view.y + r.g_y;
			pxy[2] = view.x + r.g_x;
			pxy[3] = view.y + r.g_y + r.g_h;
			v_pline(atari_plot_vdi_handle, 2, (short *)&pxy);
		}
	}

	if( pstyle->fill_type != PLOT_OP_TYPE_NONE ){
		short stroke_width = (short)(pstyle->stroke_type != PLOT_OP_TYPE_NONE) ?
								pstyle->stroke_width : 0;

		vsf_rgbcolor(atari_plot_vdi_handle, pstyle->fill_colour);
		vsf_perimeter(atari_plot_vdi_handle, 0);
		vsf_interior(atari_plot_vdi_handle, FIS_SOLID);


		pxy[0] = view.x + r.g_x + stroke_width;
		pxy[1] = view.y + r.g_y + stroke_width;
		pxy[2] = view.x + r.g_x + r.g_w -1 - stroke_width;
		pxy[3] = view.y + r.g_y + r.g_h -1 - stroke_width;

		vsf_style(atari_plot_vdi_handle, 1);
		v_bar(atari_plot_vdi_handle, (short*)&pxy);
	}
    return (true);
}

bool plot_line(int x0, int y0, int x1, int y1,
                const plot_style_t *pstyle )
{
	short pxy[4];
	uint32_t lt;
	int sw = pstyle->stroke_width;

	if((x0 < 0 && x1 < 0) || (y0 < 0 && y1 < 0)){
		return(true);
	}

	pxy[0] = view.x + MAX(0,x0);
	pxy[1] = view.y + MAX(0,y0);
	pxy[2] = view.x + MAX(0,x1);
	pxy[3] = view.y + MAX(0,y1);

	if((y0 > view.h-1) && (y1 > view.h-1))
		return(true);

	//printf("view: %d,%d,%d,%d\n", view.x, view.y, view.w, view.h);
	//printf("line: %d,%d,%d,%d\n", x0,  y0,  x1,  y1);


	//plot_vdi_clip(true);
	if( sw == 0)
		sw = 1;
	NSLT2VDI(lt, pstyle)
	vsl_type(atari_plot_vdi_handle, (lt&0x0F));
	/* if the line style is not available within VDI system,define own style: */
	if( (lt&0x0F) == 7 ){
		vsl_udsty(atari_plot_vdi_handle, ((lt&0xFFFF00) >> 8));
	}
	vsl_width(atari_plot_vdi_handle, (short)sw);
	vsl_rgbcolor(atari_plot_vdi_handle, pstyle->stroke_colour);
	v_pline(atari_plot_vdi_handle, 2, (short *)&pxy );
	//plot_vdi_clip(false);
    return (true);
}

static bool plot_polygon(const int *p, unsigned int n,
                         const plot_style_t *pstyle)
{
	short pxy[n*2];
	unsigned int i=0;
	short d[4];
	if (vdi_sysinfo.maxpolycoords > 0)
		assert( (signed int)n < vdi_sysinfo.maxpolycoords);

	vsf_interior(atari_plot_vdi_handle, FIS_SOLID);
	vsf_style(atari_plot_vdi_handle, 1);
	for (i = 0; i<n*2; i=i+2) {
		pxy[i] = (short)view.x+p[i];
		pxy[i+1] = (short)view.y+p[i+1];
	}
	if (pstyle->fill_type == PLOT_OP_TYPE_SOLID) {
		vsf_rgbcolor(atari_plot_vdi_handle, pstyle->fill_colour);
		v_fillarea(atari_plot_vdi_handle, n, (short*)&pxy);
	} else {
		pxy[n*2]=pxy[0];
		pxy[n*2+1]=pxy[1];
		vsl_rgbcolor(atari_plot_vdi_handle, pstyle->stroke_colour);
		v_pline(atari_plot_vdi_handle, n+1,  (short *)&pxy);
	}

    return ( true );
}

/***
 * Set plot origin and canvas size
 * \param x the x origin
 * \param y the y origin
 * \param w the width of the plot area
 * \param h the height of the plot area
 */
bool plot_set_dimensions(int x, int y, int w, int h)
{
	bool doupdate = false;
	struct rect newclip = {0, 0, w, h};
	GRECT absclip = {x, y, w, h};

	if (!(w == view.w && h == view.h)) {
		struct rect newclip = { 0, 0, w-1, h-1 };
		view.w = (short)w;
		view.h = (short)h;
		doupdate = true;
	}
	if (!(x == view.x && y == view.y)) {
		view.x = (short)x;
		view.y = (short)y;
		doupdate = true;
	}
	if (doupdate==true)
		update_visible_rect();

	//dbg_rect("plot_set_dimensions", &newclip);

    plot_set_abs_clipping(&absclip);
	plot_clip(&newclip);
    return(true);
}

/***
 * Get current canvas size
 * \param dst the GRECT * which receives the canvas size
 *
 */
bool plot_get_dimensions(GRECT *dst)
{
	dst->g_x = view.x;
	dst->g_y = view.y;
	dst->g_w = view.w;
	dst->g_h = view.h;
	return(true);
}

/**
 * set scale of plotter.
 * \param scale the new scale value
 * \return the old scale value
 */

float plot_set_scale(float scale)
{
    float ret = view.scale;

    view.scale = scale;

    return(ret);
}

float plot_get_scale(void)
{
    return(view.scale);
}


/**
 *
 * Subsequent calls to plot_clip will be clipped by the absolute clip.
 * \param area the maximum clipping rectangle (absolute screen coords)
 *
*/
void plot_set_abs_clipping(const GRECT *area)
{
    GRECT canvas;

    plot_get_dimensions(&canvas);

    if(!rc_intersect(area, &canvas)){
        view.abs_clipping.x0 = 0;
        view.abs_clipping.x1 = 0;
        view.abs_clipping.y0 = 0;
        view.abs_clipping.y1 = 0;
    }
    else {
        view.abs_clipping.x0 = area->g_x;
        view.abs_clipping.x1 = area->g_x + area->g_w;
        view.abs_clipping.y0 = area->g_y;
        view.abs_clipping.y1 = area->g_y + area->g_h;
    }
}


/***
 * Get the maximum clip extent, in absolute screen coords
 * \param dst the structure that receives the absolute clipping
 */
void plot_get_abs_clipping(struct rect *dst)
{
    *dst = view.abs_clipping;
}


/***
 * Get the maximum clip extent, in absolute screen coords
 * \param dst the structure that receives the absolute clipping
 */
void plot_get_abs_clipping_grect(GRECT *dst)
{
    dst->g_x = view.abs_clipping.x0;
    dst->g_w = view.abs_clipping.x1 - view.abs_clipping.x0;
    dst->g_y = view.abs_clipping.y0;
    dst->g_h = view.abs_clipping.y1 - view.abs_clipping.y0;
}

bool plot_clip(const struct rect *clip)
{
	GRECT canvas, screen, gclip, maxclip;
	short pxy[4];

	screen.g_x = 0;
	screen.g_y = 0;
	screen.g_w = vdi_sysinfo.scr_w;
	screen.g_h = vdi_sysinfo.scr_h;

	plot_get_dimensions(&canvas);

	view.clipping.y0 = clip->y0;
	view.clipping.y1 = clip->y1;
	view.clipping.x0 = clip->x0;
	view.clipping.x1 = clip->x1;

	plot_get_clip_grect(&gclip);

	gclip.g_x += canvas.g_x;
	gclip.g_y += canvas.g_y;

	rc_intersect(&canvas, &gclip);

	if(gclip.g_h < 0){
		gclip.g_h = 0;
	}

	if (!rc_intersect(&screen, &gclip)) {
		//dbg_rect("cliprect: ", &view.clipping);
		//dbg_grect("screen: ", &canvas);
		//dbg_grect("canvas clipped: ", &gclip);
		//assert(1 == 0);
	}

    // When setting VDI clipping, obey to maximum cliping rectangle:
	plot_get_abs_clipping_grect(&maxclip);
	rc_intersect(&maxclip, &gclip);

	//dbg_grect("canvas clipped to screen", &gclip);

	pxy[0] = gclip.g_x;
	pxy[1] = gclip.g_y;
	pxy[2] = pxy[0] + gclip.g_w;
	pxy[3] = pxy[1] + gclip.g_h;

	vs_clip(atari_plot_vdi_handle, 1, (short*)&pxy);

    return ( true );
}

VdiHdl plot_get_vdi_handle(void)
{
	return(atari_plot_vdi_handle);
}

long plot_get_flags(void)
{
	return(atari_plot_flags);
}


bool plot_get_clip(struct rect * out)
{
	out->x0 = view.clipping.x0;
	out->y0 = view.clipping.y0;
	out->x1 = view.clipping.x1;
	out->y1 = view.clipping.y1;
    return( true );
}

void plot_get_clip_grect(GRECT * out)
{
    struct rect clip={0,0,0,0};

    plot_get_clip(&clip);

    out->g_x = clip.x0;
    out->g_y = clip.y0;
    out->g_w = clip.x1 - clip.x0;
    out->g_h = clip.y1 - clip.y0;
}

FONT_PLOTTER plot_get_text_plotter()
{
	return(fplotter);
}

void plot_set_text_plotter(FONT_PLOTTER font_plotter)
{
	fplotter = font_plotter;
}

static bool plot_text(int x, int y, const char *text, size_t length, const plot_font_style_t *fstyle )
{
    if (view.scale != 1.0) {
        plot_font_style_t newstyle = *fstyle;
        newstyle.size = (int)((float)fstyle->size*view.scale);
        fplotter->text(fplotter, x, y, text, length, &newstyle);
    } else {
        fplotter->text(fplotter, x, y, text, length, fstyle);
    }

    return ( true );
}

static bool plot_disc(int x, int y, int radius, const plot_style_t *pstyle)
{
	if (pstyle->fill_type != PLOT_OP_TYPE_SOLID) {
		vsf_rgbcolor(atari_plot_vdi_handle, pstyle->stroke_colour);
		vsf_perimeter(atari_plot_vdi_handle, 1);
		vsf_interior(atari_plot_vdi_handle, 0);
		v_circle(atari_plot_vdi_handle, view.x + x, view.y + y, radius);
	} else {
		vsf_rgbcolor(atari_plot_vdi_handle, pstyle->fill_colour);
		vsf_perimeter(atari_plot_vdi_handle, 0);
		vsf_interior(atari_plot_vdi_handle, FIS_SOLID);
		v_circle(atari_plot_vdi_handle, view.x + x, view.y + y, radius);
	}

    return(true);
}

static bool plot_arc(int x, int y, int radius, int angle1, int angle2,
                     const plot_style_t *pstyle)
{

	vswr_mode(atari_plot_vdi_handle, MD_REPLACE );
	if (pstyle->fill_type == PLOT_OP_TYPE_NONE)
		return(true);
	if ( pstyle->fill_type != PLOT_OP_TYPE_SOLID) {
		vsl_rgbcolor(atari_plot_vdi_handle, pstyle->stroke_colour);
		vsf_perimeter(atari_plot_vdi_handle, 1);
		vsf_interior(atari_plot_vdi_handle, 1 );
		v_arc(atari_plot_vdi_handle, view.x + x, view.y + y, radius, angle1*10, angle2*10);
	} else {
		vsf_rgbcolor(atari_plot_vdi_handle, pstyle->fill_colour);
		vsl_width(atari_plot_vdi_handle, 1 );
		vsf_perimeter(atari_plot_vdi_handle, 1);
		v_arc(atari_plot_vdi_handle, view.x + x, view.y + y, radius, angle1*10, angle2*10);
	}

    return (true);
}

static bool plot_bitmap(int x, int y, int width, int height,
                        struct bitmap *bitmap, colour bg,
                        bitmap_flags_t flags)
{
    struct bitmap * bm = NULL;
    bool repeat_x = (flags & BITMAPF_REPEAT_X);
    bool repeat_y = (flags & BITMAPF_REPEAT_Y);
    int bmpw,bmph;
    struct rect clip = {0,0,0,0};

    bmpw = bitmap_get_width(bitmap);
    bmph = bitmap_get_height(bitmap);

    if(view.scale != 1.0){
        width = (int)(((float)width)*view.scale);
        height = (int)(((float)height)*view.scale);
    }

    if ( repeat_x || repeat_y ) {
        plot_get_clip(&clip);
        if( repeat_x && width == 1 && repeat_y && height == 1 ) {
            width = MAX( width, clip.x1 - x );
            height = MAX( height,  clip.y1 - y );
        } else if( repeat_x && width == 1 ) {
            width = MAX( width, clip.x1 - x);
        } else if( repeat_y && height == 1) {
            height = MAX( height, clip.y1 - y );
        }
    }

    if(  width != bmpw || height != bmph ) {
        bitmap_resize(bitmap, hermes_res_h, &nsfmt, width, height );
        if( bitmap->resized )
            bm = bitmap->resized;
        else
            bm = bitmap;
    } else {
        bm = bitmap;
    }

    /* out of memory? */
    if( bm == NULL ) {
        printf("plot: out of memory! bmp: %p, bmpres: %p\n", bitmap, bitmap->resized );
        return( true );
    }

    if (!(repeat_x || repeat_y) ) {
        plot_blit_bitmap(bm, x, y, bg, flags );
    } else {
        int xf,yf;
        int xoff = x;
        int yoff = y;

        if (yoff > clip.y0 )
            yoff = (clip.y0 - height) + ((yoff - clip.y0) % height);
        if (xoff > clip.x0 )
            xoff = (clip.x0 - width) + ((xoff - clip.x0) % width);
        /* for now, repeating just works in the rigth / down direction */
        /*
        if( repeat_x == true )
        	xoff = clip.x0;
        if(repeat_y == true )
        	yoff = clip.y0;
        */

        for( xf = xoff; xf < clip.x1; xf += width ) {
            for( yf = yoff; yf < clip.y1; yf += height ) {
                plot_blit_bitmap(bm, xf, yf, bg, flags );
                if (!repeat_y)
                    break;
            }
            if (!repeat_x)
                break;
        }
    }
    return ( true );
}

static bool plot_path(const float *p, unsigned int n, colour fill, float width,
                      colour c, const float transform[6])
{
    return ( true );
}



const struct plotter_table atari_plotters = {
    .rectangle = plot_rectangle,
    .line = plot_line,
    .polygon = plot_polygon,
    .clip = plot_clip,
    .text = plot_text,
    .disc = plot_disc,
    .arc = plot_arc,
    .bitmap = plot_bitmap,
    .path = plot_path,
    .flush = NULL,
    .group_start = NULL,
    .group_end = NULL,
    .option_knockout = true
};
