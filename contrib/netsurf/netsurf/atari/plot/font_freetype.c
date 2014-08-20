/*
 * Copyright 2005 James Bursa <bursa@users.sourceforge.net>
 *           2008 Vincent Sanders <vince@simtec.co.uk>
 *			 2011 Ole Loots <ole@monochrom.net>
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

#ifdef WITH_FREETYPE_FONT_DRIVER

#include <ft2build.h>
#include FT_CACHE_H

#include "utils/nsoption.h"
#include "atari/plot/plot.h"
#include "atari/plot/font_freetype.h"
#include "atari/findfile.h"

/* -------------------------------------------------------------------------- */
/*  Font Loading & Mapping scheme                                             */
/* -------------------------------------------------------------------------- */
/*

Truetype fonts are loaded in the following priority order:

1. Option values.
2. default resouce names (8.3 compatible).
3. default font package installation path.


Resource font names & their meanings:
--------------------------------------------
s.ttf		=> Serif
sb.ttf		=> Serif Bold
ss.ttf 		=> Sans Serif					*Default Font
ssb.ttf 	=> Sans Serif Bold
ssi.ttf 	=> Sans Serif Italic
ssib.ttf 	=> Sans Serif Italic Bold
mono.ttf	=> Monospaced
monob.ttf 	=> Monospaced Bold
cursive.ttf	=> Cursive
fantasy.ttf => Fantasy
*/

#define FONT_RESOURCE_PATH "fonts/"
#define DEJAVU_PATH 	"/usr/share/fonts/truetype/ttf-dejavu/"
#define BITSTREAM_PATH 	"/usr/share/fonts/truetype/ttf-bitstream-vera/"

#if !defined(USE_BITSTREAM_FONT_PACKAGE) && !defined(USE_DEJAVU_FONT_PACKAGE)
# define USE_BITSTREAM_FONT_PACKAGE
#endif

#if defined(USE_DEJAVU_FONT_PACKAGE)
# define FONT_PKG_PATH					DEJAVU_PATH
# define FONT_FILE_SANS					"DejaVuSans.ttf"
# define FONT_FILE_SANS_BOLD			"DejaVuSans-Bold.ttf"
# define FONT_FILE_SANS_OBLIQUE			"DejaVuSans-Oblique.ttf"
# define FONT_FILE_SANS_BOLD_OBLIQUE	"DejaVuSans-BoldOblique.ttf"
# define FONT_FILE_SERIF				"DejaVuSerif.ttf"
# define FONT_FILE_SERIF_BOLD			"DejaVuSerif-Bold.ttf"
# define FONT_FILE_MONO					"DejaVuSansMono.ttf"
# define FONT_FILE_MONO_BOLD			"DejaVuSerif-Bold.ttf"
# define FONT_FILE_OBLIQUE				"DejaVuSansMono-Oblique.ttf"
# define FONT_FILE_FANTASY				"DejaVuSerifCondensed-Bold.ttf"
#elif  defined(USE_BITSTREAM_FONT_PACKAGE)
# define FONT_PKG_PATH					BITSTREAM_PATH
# define FONT_FILE_SANS					"Vera.ttf"
# define FONT_FILE_SANS_BOLD			"VeraBd.ttf"
# define FONT_FILE_SANS_OBLIQUE			"VeraIt.ttf"
# define FONT_FILE_SANS_BOLD_OBLIQUE	"VeraBI.ttf"
# define FONT_FILE_SERIF				"VeraSe.ttf"
# define FONT_FILE_SERIF_BOLD			"VeraSeBd.ttf"
# define FONT_FILE_MONO					"VeraMono.ttf"
# define FONT_FILE_MONO_BOLD			"VeraMoBd.ttf"
# define FONT_FILE_OBLIQUE				"VeraMoIt.ttf"
# define FONT_FILE_FANTASY				"VeraMoBI.ttf"

#endif

#define CACHE_SIZE 2048
#define CACHE_MIN_SIZE (100 * 1024)
#define BOLD_WEIGHT 700

extern css_fixed nscss_screen_dpi;

extern unsigned long atari_plot_flags;
extern int atari_plot_vdi_handle;

static FT_Library library;
static FTC_Manager ft_cmanager;
static FTC_CMapCache ft_cmap_cache ;
static FTC_ImageCache ft_image_cache;

int ft_load_type;

/* cache manager faceID data to create freetype faceid on demand */
typedef struct ftc_faceid_s {
        char *fontfile; /* path to font */
        int index; /* index of font */
        int cidx; /* character map index for unicode */
} ftc_faceid_t;

static int dtor( FONT_PLOTTER self );
static int str_width( FONT_PLOTTER self,const plot_font_style_t *fstyle,
						const char * str, size_t length, int * width  );
static int str_split( FONT_PLOTTER self, const plot_font_style_t *fstyle,
						const char *string, size_t length,int x,
						size_t *char_offset, int *actual_x );
static int pixel_pos( FONT_PLOTTER self, const plot_font_style_t *fstyle,
							const char *string, size_t length,int x,
							size_t *char_offset, int *actual_x );
static int text( FONT_PLOTTER self,  int x, int y, const char *text,
					size_t length, const plot_font_style_t *fstyle );

static void draw_glyph8(FONT_PLOTTER self, GRECT *clip, GRECT * loc,
						uint8_t * pixdata, int pitch, uint32_t colour);
static void draw_glyph1(FONT_PLOTTER self, GRECT * clip, GRECT * loc,
						uint8_t * pixdata, int pitch, uint32_t colour);

static ftc_faceid_t *font_faces[FONT_FACE_COUNT];
static MFDB tmp;
static int tmp_mfdb_size;
static bool init = false;
static struct bitmap * fontbmp;
static size_t fontbmp_stride;
static int fontbmp_allocated_height;
static int fontbmp_allocated_width;



/* map cache manager handle to face id */
static FT_Error ft_face_requester(FTC_FaceID face_id, FT_Library  library, FT_Pointer request_data, FT_Face *face )
{
	FT_Error error;
	ftc_faceid_t *ft_face = (ftc_faceid_t *)face_id;
	int cidx;

	error = FT_New_Face(library, ft_face->fontfile, ft_face->index, face);
	if (error) {
		LOG(("Could not find font (code %d)\n", error));
	} else {
		error = FT_Select_Charmap(*face, FT_ENCODING_UNICODE);
		if (error) {
			LOG(("Could not select charmap (code %d)\n", error));
		} else {
			for (cidx = 0; cidx < (*face)->num_charmaps; cidx++) {
				if ((*face)->charmap == (*face)->charmaps[cidx]) {
					ft_face->cidx = cidx;
					break;
				}
			}
		}
	}
	LOG(("Loaded face from %s\n", ft_face->fontfile));
	return error;
}

/* create new framebuffer face and cause it to be loaded to check its ok */
static ftc_faceid_t *
ft_new_face(const char *option, const char *resname, const char *fontfile)
{
	ftc_faceid_t *newf;
	FT_Error error;
	FT_Face aface;
	char buf[PATH_MAX];

	newf = calloc(1, sizeof(ftc_faceid_t));
	if (option != NULL) {
		newf->fontfile = strdup(option);
	} else {
		atari_find_resource(buf, resname, fontfile);
		newf->fontfile = strdup(buf);
	}
	error = FTC_Manager_LookupFace(ft_cmanager, (FTC_FaceID)newf, &aface);
	if (error) {
		LOG(("Could not find font face %s (code %d)\n", fontfile, error));
		free(newf);
		newf = font_faces[FONT_FACE_DEFAULT]; /* use default */
	}
	return newf;
}

static void ft_fill_scalar(const plot_font_style_t *fstyle, FTC_Scaler srec)
{
	int selected_face = FONT_FACE_DEFAULT;

	switch (fstyle->family) {

	case PLOT_FONT_FAMILY_SERIF:
		if (fstyle->weight >= BOLD_WEIGHT) {
			selected_face = FONT_FACE_SERIF_BOLD;
		} else {
			selected_face = FONT_FACE_SERIF;
		}
		break;

	case PLOT_FONT_FAMILY_MONOSPACE:
		if (fstyle->weight >= BOLD_WEIGHT) {
			selected_face = FONT_FACE_MONOSPACE_BOLD;
		} else {
			selected_face = FONT_FACE_MONOSPACE;
		}
		break;

	case PLOT_FONT_FAMILY_CURSIVE:
		selected_face = FONT_FACE_CURSIVE;
		break;

	case PLOT_FONT_FAMILY_FANTASY:
		selected_face = FONT_FACE_FANTASY;
		break;

	case PLOT_FONT_FAMILY_SANS_SERIF:
	default:
		if ((fstyle->flags & FONTF_ITALIC) ||
		    (fstyle->flags & FONTF_OBLIQUE)) {
			if (fstyle->weight >= BOLD_WEIGHT) {
				selected_face = FONT_FACE_SANS_SERIF_ITALIC_BOLD;
			} else {
				selected_face = FONT_FACE_SANS_SERIF_ITALIC;
			}
		} else {
			if (fstyle->weight >= BOLD_WEIGHT) {
				selected_face = FONT_FACE_SANS_SERIF_BOLD;
			} else {
				selected_face = FONT_FACE_SANS_SERIF;
			}
		}
	}

	srec->face_id = (FTC_FaceID)font_faces[selected_face];

	srec->width = srec->height = (fstyle->size * 64) / FONT_SIZE_SCALE;
	srec->pixel = 0;

	/* calculate x/y resolution, when nscss_screen_dpi isn't available 	*/
	/* 72 is an good value. 											*/
	/* TODO: because nscss_screen_dpi is to large, calculate that value */
	/*       by VDI values. 											*/
	srec->x_res = srec->y_res = 72; // FIXTOINT(nscss_screen_dpi);
}

static FT_Glyph ft_getglyph(const plot_font_style_t *fstyle, uint32_t ucs4)
{
	FT_UInt glyph_index;
	FTC_ScalerRec srec;
	FT_Glyph glyph;
	FT_Error error;
	ftc_faceid_t *ft_face;

	ft_fill_scalar(fstyle, &srec);
	ft_face = (ftc_faceid_t *)srec.face_id;
	glyph_index = FTC_CMapCache_Lookup(ft_cmap_cache, srec.face_id, ft_face->cidx, ucs4);
	error = FTC_ImageCache_LookupScaler(ft_image_cache,
                                            &srec,
                                            FT_LOAD_RENDER |
                                            FT_LOAD_FORCE_AUTOHINT |
                                            ft_load_type,
                                            glyph_index,
                                            &glyph,
                                            NULL);
	return glyph;
}


/* initialise font handling */
static bool ft_font_init(void)
{
	FT_Error error;
	FT_ULong max_cache_size;
	FT_UInt max_faces = 6;
	int i;

	/* freetype library initialise */
	error = FT_Init_FreeType( &library );
	if (error) {
                LOG(("Freetype could not initialised (code %d)\n", error));
                return false;
	}

	/* set the Glyph cache size up */
	max_cache_size = CACHE_SIZE * 1024;
	if (max_cache_size < CACHE_MIN_SIZE) {
		max_cache_size = CACHE_MIN_SIZE;
	}

	/* cache manager initialise */
	error = FTC_Manager_New(library,
                                max_faces,
                                0,
                                max_cache_size,
                                ft_face_requester,
                                NULL,
                                &ft_cmanager);
	if (error) {
		LOG(("Freetype could not initialise cache manager (code %d)\n", error));
		FT_Done_FreeType(library);
		return false;
	}

	error = FTC_CMapCache_New(ft_cmanager, &ft_cmap_cache);
	error = FTC_ImageCache_New(ft_cmanager, &ft_image_cache);

	/* Optain font faces */


	/* Default font, Sans Serif */
	font_faces[FONT_FACE_SANS_SERIF] = NULL;
	font_faces[FONT_FACE_SANS_SERIF] = ft_new_face(
											nsoption_charp(font_face_sans_serif),
                            				FONT_RESOURCE_PATH "ss.ttf",
                            				FONT_PKG_PATH FONT_FILE_SANS
										);
	if (font_faces[FONT_FACE_SANS_SERIF] == NULL) {
                LOG(("Could not find default font (code %d)\n", error));
                FTC_Manager_Done(ft_cmanager);
                FT_Done_FreeType(library);
                return false;
	}

	/* Sans Serif Bold*/
	font_faces[FONT_FACE_SANS_SERIF_BOLD] =
			ft_new_face(nsoption_charp(font_face_sans_serif_bold),
                            FONT_RESOURCE_PATH "ssb.ttf",
                            FONT_PKG_PATH FONT_FILE_SANS_BOLD);

	/* Sans Serif Italic */
	font_faces[FONT_FACE_SANS_SERIF_ITALIC] =
			ft_new_face(nsoption_charp(font_face_sans_serif_italic),
                            FONT_RESOURCE_PATH "ssi.ttf",
                            FONT_PKG_PATH FONT_FILE_SANS_OBLIQUE);

	/* Sans Serif Italic Bold */
	font_faces[FONT_FACE_SANS_SERIF_ITALIC_BOLD] =
			ft_new_face(nsoption_charp(font_face_sans_serif_italic_bold),
                            FONT_RESOURCE_PATH "ssib.ttf",
                            FONT_PKG_PATH FONT_FILE_SANS_BOLD_OBLIQUE);

	/* Monospaced */
	font_faces[FONT_FACE_MONOSPACE] =
			ft_new_face(nsoption_charp(font_face_monospace),
                            FONT_RESOURCE_PATH "mono.ttf",
                            FONT_PKG_PATH FONT_FILE_MONO);

	/* Mospaced Bold */
	font_faces[FONT_FACE_MONOSPACE_BOLD] =
			ft_new_face(nsoption_charp(font_face_monospace_bold),
                            FONT_RESOURCE_PATH "monob.ttf",
                            FONT_PKG_PATH FONT_FILE_MONO_BOLD);

	/* Serif */
	font_faces[FONT_FACE_SERIF] =
			ft_new_face(nsoption_charp(font_face_serif),
                            FONT_RESOURCE_PATH "s.ttf",
                            FONT_PKG_PATH FONT_FILE_SERIF);

	/* Serif Bold */
	font_faces[FONT_FACE_SERIF_BOLD] =
			ft_new_face(nsoption_charp(font_face_serif_bold),
                            FONT_RESOURCE_PATH "sb.ttf",
                            FONT_PKG_PATH FONT_FILE_SERIF_BOLD);

	/* Cursive */
	font_faces[FONT_FACE_CURSIVE] =
			ft_new_face(nsoption_charp(font_face_cursive),
                            FONT_RESOURCE_PATH "cursive.ttf",
                            FONT_PKG_PATH FONT_FILE_OBLIQUE);

	/* Fantasy */
	font_faces[FONT_FACE_FANTASY] =
			ft_new_face(nsoption_charp(font_face_fantasy),
                            FONT_RESOURCE_PATH "fantasy.ttf",
                            FONT_PKG_PATH FONT_FILE_FANTASY);

	for (i=1; i<FONT_FACE_COUNT; i++) {
		if (font_faces[i] == NULL){
			font_faces[i] = font_faces[FONT_FACE_SANS_SERIF];
		}
	}

	return true;
}


static bool ft_font_finalise(void)
{
	FTC_Manager_Done(ft_cmanager );
	FT_Done_FreeType(library);
	return true;
}

static int str_width( FONT_PLOTTER self,const plot_font_style_t *fstyle,
                         const char *string, size_t length,
                         int *width)
{
	uint32_t ucs4;
	size_t nxtchr = 0;
	FT_Glyph glyph;

	*width = 0;
	while (nxtchr < length) {
		ucs4 = utf8_to_ucs4(string + nxtchr, length - nxtchr);
		nxtchr = utf8_next(string, length, nxtchr);

		glyph = ft_getglyph(fstyle, ucs4);
		if (glyph == NULL)
			continue;
		*width += glyph->advance.x >> 16;
	}
	return(1);
}


static int str_split( FONT_PLOTTER self, const plot_font_style_t *fstyle,
		const char *string, size_t length,
		int x, size_t *char_offset, int *actual_x)
{
	uint32_t ucs4;
	size_t nxtchr = 0;
	int last_space_x = 0;
	int last_space_idx = 0;
	FT_Glyph glyph;

	*actual_x = 0;
	while (nxtchr < length) {
		ucs4 = utf8_to_ucs4(string + nxtchr, length - nxtchr);
      glyph = ft_getglyph(fstyle, ucs4);
      if (glyph == NULL)
			continue;
		if (ucs4 == 0x20) {
			last_space_x = *actual_x;
			last_space_idx = nxtchr;
		}
		*actual_x += glyph->advance.x >> 16;
		if (*actual_x > x && last_space_idx != 0) {
			/* string has exceeded available width and we've
			 * found a space; return previous space */
			*actual_x = last_space_x;
			*char_offset = last_space_idx;
			return true;
		}
		nxtchr = utf8_next(string, length, nxtchr);
	}
	*char_offset = nxtchr;
	return (1);
}


static int pixel_pos( FONT_PLOTTER self, const plot_font_style_t *fstyle,
		const char *string, size_t length,
		int x, size_t *char_offset, int *actual_x)
{
	uint32_t ucs4;
	size_t nxtchr = 0;
	FT_Glyph glyph;
	int prev_x = 0;

	*actual_x = 0;
	while (nxtchr < length) {
		ucs4 = utf8_to_ucs4(string + nxtchr, length - nxtchr);
		glyph = ft_getglyph(fstyle, ucs4);
		if (glyph == NULL)
			continue;
			*actual_x += glyph->advance.x >> 16;
			if (*actual_x > x)
				break;

			prev_x = *actual_x;
			nxtchr = utf8_next(string, length, nxtchr);
	}

	/* choose nearest of previous and last x */
	if (abs(*actual_x - x) > abs(prev_x - x))
		*actual_x = prev_x;
	*char_offset = nxtchr;
	return ( 1 );
}


static void draw_glyph8(FONT_PLOTTER self, GRECT * clip, GRECT * loc, uint8_t * pixdata, int pitch, uint32_t colour)
{
	uint32_t * linebuf;
	uint32_t fontpix;
	int xloop,yloop,xoff,yoff;
	int x,y,w,h;

	x = loc->g_x;
	y = loc->g_y;
	w = loc->g_w;
	h = loc->g_h;

	if( !rc_intersect( clip, loc ) ){
		return;
	}

	xoff = loc->g_x - x;
	yoff = loc->g_y - y;

	assert( loc->g_h <= h );
	assert( loc->g_w <= w );

	h = loc->g_h;
	w = loc->g_w;

    assert( h <= fontbmp_allocated_height );
    assert( w <= fontbmp_allocated_width );

	fontbmp->height = h;
	fontbmp->width = w;
	for( yloop = 0; yloop < MIN(fontbmp_allocated_height, h); yloop++) {
		linebuf = (uint32_t *)(fontbmp->pixdata + (fontbmp_stride * yloop));
		for(xloop = 0; xloop < MIN(fontbmp_allocated_width, w); xloop++){
			fontpix = (uint32_t)(pixdata[(( yoff + yloop ) * pitch) + xloop + xoff]);
			linebuf[xloop] = (uint32_t)(colour | fontpix);
		}
	}
	plot_blit_bitmap(fontbmp, loc->g_x, loc->g_y, 0, BITMAPF_MONOGLYPH);
}

static void draw_glyph1(FONT_PLOTTER self, GRECT * clip, GRECT * loc, uint8_t * pixdata, int pitch, uint32_t colour)
{
	int xloop,yloop,xoff,yoff;
	int x,y,w,h;
	uint8_t bitm;
    const uint8_t *fntd;

	x = loc->g_x;
	y = loc->g_y;
	w = loc->g_w;
	h = loc->g_h;

	if( !rc_intersect( clip, loc ) ){
		return;
	}

	xoff = loc->g_x - x;
	yoff = loc->g_y - y;

	if (h > loc->g_h)
		h = loc->g_h;

	if (w > loc->g_w)
		w = loc->g_w;

	int stride = MFDB_STRIDE( w );
	if( tmp.fd_addr == NULL || tmp_mfdb_size < MFDB_SIZE( 1, stride, h) ){
		tmp_mfdb_size = init_mfdb( 1, w, h,  MFDB_FLAG_STAND | MFDB_FLAG_ZEROMEM, &tmp );
	} else {
		void * buf = tmp.fd_addr;
		int size = init_mfdb( 1, w, h,  MFDB_FLAG_STAND | MFDB_FLAG_NOALLOC, &tmp );
		tmp.fd_addr = buf;
		memset( tmp.fd_addr, 0, size );
	}
	short * buf;
	for( yloop = 0; yloop < h; yloop++) {
		fntd = pixdata + (pitch * (yloop+yoff))+(xoff>>3);
		buf = tmp.fd_addr;
		buf += (tmp.fd_wdwidth*yloop);
		for ( xloop = 0, bitm = (1<<(7-(xoff%8))); xloop < w; xloop++, bitm=(bitm>>1) ) {
				if( (*fntd & bitm) != 0 ){
					short whichbit = (1<<(15-(xloop%16)));
					buf[xloop>>4] = ((buf[xloop>>4])|(whichbit));
				}
				if( bitm == 1 ) {
					fntd++;
					bitm = 128;
				}
		}
	}
#ifdef WITH_8BPP_SUPPORT
	if( app.nplanes > 8 ){
#endif
		plot_blit_mfdb(loc, &tmp, OFFSET_CUSTOM_COLOR, PLOT_FLAG_TRANS );
#ifdef WITH_8BPP_SUPPORT
	} else {
		plot_blit_mfdb(loc, &tmp, colour, PLOT_FLAG_TRANS );
	}
#endif

}

static int text( FONT_PLOTTER self,  int x, int y, const char *text, size_t length,
				 const plot_font_style_t *fstyle )
{
	uint32_t ucs4;
	size_t nxtchr = 0;
	FT_Glyph glyph;
	FT_BitmapGlyph bglyph;
	GRECT loc, clip;
	uint32_t c = fstyle->foreground ;
	struct rect clipping;
	/* in -> BGR */
	/* out -> ARGB */
	if( !(self->flags & FONTPLOT_FLAG_MONOGLYPH) ){
		c = ABGR_TO_RGB(c);
	} else {
#ifdef WITH_8BPP_SUPPORT
		if( app.nplanes > 8 ){
#endif
			RGB1000 out;
			rgb_to_vdi1000( (unsigned char*)&c, &out);
			vs_color(atari_plot_vdi_handle, OFFSET_CUSTOM_COLOR,
						(unsigned short*)&out);
#ifdef WITH_8BPP_SUPPORT
		} else {
			c = RGB_TO_VDI(c);
		}
#endif
	}

	plot_get_clip(&clipping);
	clip.g_x = clipping.x0;
	clip.g_y = clipping.y0;
	clip.g_w = (clipping.x1 - clipping.x0)+1;
	clip.g_h = (clipping.y1 - clipping.y0)+1;

	fontbmp = bitmap_realloc( clip.g_w, clip.g_h,
				4, clip.g_w << 2,
				BITMAP_GROW, fontbmp );
	fontbmp_stride = bitmap_get_rowstride(fontbmp);
	fontbmp_allocated_height = clip.g_h;
	fontbmp_allocated_width = clip.g_w;

	while (nxtchr < length) {
		ucs4 = utf8_to_ucs4(text + nxtchr, length - nxtchr);
		nxtchr = utf8_next(text, length, nxtchr);

		glyph = ft_getglyph(fstyle, ucs4);
		if (glyph == NULL){
			continue;
		}

		if (glyph->format == FT_GLYPH_FORMAT_BITMAP) {
				bglyph = (FT_BitmapGlyph)glyph;
				loc.g_x = x + bglyph->left;
				loc.g_y = y - bglyph->top;
				loc.g_w = bglyph->bitmap.width;
				loc.g_h = bglyph->bitmap.rows;

				if( loc.g_w > 0) {
					self->draw_glyph( self,
						&clip, &loc,
						bglyph->bitmap.buffer,
						bglyph->bitmap.pitch,
						c
					);
				}
		}
		x += glyph->advance.x >> 16;
	}
	return( 0 );
}


int ctor_font_plotter_freetype( FONT_PLOTTER self )
{
	self->dtor = dtor;
	self->str_width = str_width;
	self->str_split = str_split;
	self->pixel_pos = pixel_pos;
	self->text = text;

	/* set the default render mode */
	if( (self->flags & FONTPLOT_FLAG_MONOGLYPH) != 0 ){
		ft_load_type = FT_LOAD_MONOCHROME;
		self->draw_glyph = draw_glyph1;
	}
	else{
		ft_load_type = 0;
		self->draw_glyph = draw_glyph8;
	}

	LOG(("%s: %s\n", (char*)__FILE__, __FUNCTION__));
	if( !init ) {
		ft_font_init();
		fontbmp = bitmap_create(48, 48, 0);
		fontbmp->opaque = false;
		init = true;
	}

	return( 1 );
}

static int dtor( FONT_PLOTTER self )
{
	ft_font_finalise();
	if( fontbmp != NULL ) {
		bitmap_destroy( fontbmp );
        fontbmp = NULL;
    }
	if( tmp.fd_addr != NULL ){
		free( tmp.fd_addr );
	}
	return( 1 );
}

#endif
