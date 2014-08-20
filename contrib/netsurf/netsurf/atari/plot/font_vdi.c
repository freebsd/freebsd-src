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

#ifdef WITH_VDI_FONT_DRIVER

#include <mt_gemx.h>

#include "atari/plot/plot.h"
#include "atari/plot/font_vdi.h"

#include "utils/utf8.h"
#include "utils/log.h"

#include "atari/encoding.h"




//static char * lstr = NULL;


static int dtor( FONT_PLOTTER self );
static int str_width( FONT_PLOTTER self,const plot_font_style_t *fstyle, 	const char * str, size_t length, int * width  );
static int str_split( FONT_PLOTTER self, const plot_font_style_t *fstyle,const char *string,
					  size_t length,int x, size_t *char_offset, int *actual_x );
static int pixel_pos( FONT_PLOTTER self, const plot_font_style_t *fstyle,const char *string,
						size_t length,int x, size_t *char_offset, int *actual_x );
static int text( FONT_PLOTTER self,  int x, int y, const char *text, size_t length, const plot_font_style_t *fstyle );

static bool init = false;
static int vdih;

extern struct s_vdi_sysinfo vdi_sysinfo;

static inline void atari_to_vdi_str(char *lstr, int length)
{
	int i, z;

	for (i=z=0; i<length; ) {
		if (((char)lstr[i]==(char)0xC2) && ((char)lstr[i+1] == (char)0xA0)) {
			lstr[z] = ' ';
			lstr[z+1] = ' ';
			i=i+2;
			z=z+2;
		}
		else {
			lstr[z] = lstr[i];
			i++;
			z++;
		}
	}
}

int ctor_font_plotter_vdi( FONT_PLOTTER self )
{
	self->dtor = dtor;
	self->str_width = str_width;
	self->str_split = str_split;
	self->pixel_pos = pixel_pos;
	self->text = text;
	LOG(("%s: %s\n", (char*)__FILE__, __FUNCTION__));
	if( !init ) {
		vdih = self->vdi_handle;
	}
	init = true;
	return( 1 );
}

static int dtor( FONT_PLOTTER self )
{
	return( 1 );
}

static int str_width( FONT_PLOTTER self,const plot_font_style_t *fstyle, const char * str,
					  size_t length, int * width  )
{
	short cw, ch, cellw, cellh;
	short pxsize;
	short fx=0;
	char * lstr = NULL;

	utf8_to_local_encoding(str, length, &lstr);
	assert( lstr != NULL );
	int slen = strlen(lstr);


	atari_to_vdi_str(lstr, slen);

	if( fstyle->flags & FONTF_ITALIC )
		fx |= 4;
	if( fstyle->flags & FONTF_OBLIQUE )
		fx |= 16;
	if( fstyle->weight > 450 )
		fx |= 1;
	vst_effects( self->vdi_handle, fx );
	/* TODO: replace 90 with global dpi setting */
	//pxsize = ceil( (fstyle->size/FONT_SIZE_SCALE) * 90 / 72 );
	//vst_height( self->vdi_handle, pxsize ,&cw, &ch, &cellw, &cellh);
	pxsize = ceil( (fstyle->size/FONT_SIZE_SCALE) * 90  );
	vst_point( self->vdi_handle, pxsize, &cw, &ch, &cellw, &cellh);
/*
	if(slen != utf8_bounded_length(str, length)){
		printf("sl: %d, utl: %d\n ", slen, utf8_bounded_length(str, length));
		printf("s: %s // %s\n", str, lstr );
	}*/


	*width = slen * cellw;
	free((void*)lstr);
	return( 0 );
}

static int str_split( FONT_PLOTTER self, const plot_font_style_t * fstyle, const char *string,
					  size_t length,int x, size_t *char_offset, int *actual_x )
{
	short cw, ch, cellw, cellh;
	short pxsize;
	short fx=0;
	int i;
	char *lstr = NULL;
	size_t slen = 0;
	int last_space_x = 0;
	int last_space_idx = 0;
	size_t nxtchr = 0;

	utf8_to_local_encoding(string, length, &lstr );
	assert( lstr != NULL );
	slen = strlen(lstr);


	atari_to_vdi_str(lstr, slen);

	if( fstyle->flags & FONTF_ITALIC )
		fx |= 4;
	if( fstyle->flags & FONTF_OBLIQUE )
		fx |= 16;
	if( fstyle->weight > 450 )
		fx |= 1;
	vst_effects( self->vdi_handle, fx );
	//pxsize = ceil( (fstyle->size/FONT_SIZE_SCALE) * 90 / 72 );
	//vst_height( self->vdi_handle, pxsize ,&cw, &ch, &cellw, &cellh);

	pxsize = ceil( (fstyle->size/FONT_SIZE_SCALE) * 90  );
	vst_point( self->vdi_handle, pxsize, &cw, &ch, &cellw, &cellh);
	*actual_x = 0;
	//*char_offset = 0;
	int cpos=0;
	while (nxtchr < slen) {
		if( lstr[nxtchr] == ' ' ) {
			last_space_x = *actual_x;
			last_space_idx = nxtchr;
		}
		*actual_x += cellw;
		if (*actual_x > x && last_space_idx != 0) {
			*actual_x = last_space_x;
			*char_offset = last_space_idx;
			//printf("at: %s\n", lstr);
			return(0);
		}

		nxtchr++;
	}
	if(nxtchr >= length){
		nxtchr = length-1;
	}

	*char_offset = nxtchr;

//	for( i=0; i<slen; i++) {
//		if( lstr[i] == ' ' ) {
//			last_space_x = *actual_x;
//			last_space_idx = cpos;
//		}
//		if( *actual_x > x ) {
//			*actual_x = last_space_x;
//			*char_offset = last_space_idx;
//			return true;
//		}
//		*actual_x += cellw;
//		cpos++;
//	}
//	*char_offset = cpos;
	free( (void*)lstr );
	return( 0 );
}

static int pixel_pos( FONT_PLOTTER self, const plot_font_style_t * fstyle,const char *string,
						size_t length,int x, size_t *char_offset, int *actual_x )
{
	short cw, ch, cellw, cellh;
	short pxsize=0;
	short fx=0;

	char *lstr = NULL;
	int i=0;
	int curpx=0;
	utf8_to_local_encoding(string, length, &lstr );
	assert( lstr != NULL );
	int slen = strlen(lstr);

	atari_to_vdi_str(lstr, slen);

	if( fstyle->flags & FONTF_ITALIC )
		fx |= 4;
	if( fstyle->flags & FONTF_OBLIQUE )
		fx |= 16;
	if( fstyle->weight > 450 )
		fx |= 1;
	vst_effects(self->vdi_handle, fx);
	pxsize = ceil( (fstyle->size/FONT_SIZE_SCALE) * 90 / 72 );
	vst_height( self->vdi_handle, pxsize ,&cw, &ch, &cellw, &cellh);
	*actual_x = 0;
	*char_offset = 0;
	for( i=0; i<slen; i++) {
		*actual_x += cellw;
		if( *actual_x > x) {
			*actual_x -= cellw;
			*char_offset = i;
			break;
		}
	}
	free((void*)lstr);
	lstr = NULL;
	return( 0 );
}

static inline void vst_rgbcolor( short vdih, uint32_t cin )
{
#ifdef WITH_8BPP_SUPPORT
	if( vdi_sysinfo.scr_bpp > 8 ) {
#endif
		//unsigned short c[4];
		RGB1000 c;

		rgb_to_vdi1000( (unsigned char*)&cin, &c );
		vs_color( vdih, OFFSET_CUSTOM_COLOR, (unsigned short*)&c);
		vst_color( vdih, OFFSET_CUSTOM_COLOR );
#ifdef WITH_8BPP_SUPPORT
	} else {
		if( vdi_sysinfo.scr_bpp >= 4 )
			vst_color( vdih, RGB_TO_VDI(cin) );
		else
			vst_color( vdih, BLACK );
	}
#endif
}

static int text( FONT_PLOTTER self,  int x, int y, const char *text, size_t length,
				 const plot_font_style_t *fstyle )
{
	/* todo: either limit the string to max 80 chars, or use v_ftext instead of v_gtext */
	short cw, ch, cellw, cellh;
	short pxsize=8;
	short fx=0;
	GRECT canvas;
	char *lstr = NULL;
	assert( utf8_to_local_encoding(text, length, &lstr) == NSERROR_OK);
	assert( lstr != NULL );

	int slen = strlen(lstr);
	if(slen > 800){
		lstr[800]=0;
	}


	atari_to_vdi_str(lstr, slen);

	if( fstyle != NULL){
		if( fstyle->flags & FONTF_ITALIC )
			fx |= 4;
		if( fstyle->flags & FONTF_OBLIQUE )
			fx |= 4;
		if( fstyle->weight > 450 )
			fx |= 1;

		/* TODO: netsurf uses 90 as default dpi ( somewhere defined in libcss),
			use that value or pass it as arg, to reduce netsurf dependency */
		//pxsize = ceil( (fstyle->size/FONT_SIZE_SCALE) * 90 / 72 );
		pxsize = ceil( (fstyle->size/FONT_SIZE_SCALE) * 90 / 72 );
	}
	plot_get_dimensions(&canvas);
	x += canvas.g_x;
	y += canvas.g_y;
	vst_effects( self->vdi_handle, fx );
	vst_alignment(vdih, 0, 0, &cw, &ch );
	vst_point( self->vdi_handle, pxsize, &cw, &ch, &cellw, &cellh);
	//vst_height( self->vdi_handle, pxsize, &cw, &ch, &cellw, &cellh);
	vswr_mode( self->vdi_handle, MD_TRANS );
	vst_rgbcolor(self->vdi_handle, fstyle->foreground);

	if( atari_sysinfo.gdos_FSMC ){
		//printf("\nftext\n");
		v_ftext( self->vdi_handle, x, y, (char*)lstr );
	} else {
		//printf("\ngtext\n");
		v_gtext( self->vdi_handle, x, y, (char*)lstr );
	}
	free( lstr );
	return( 0 );
}

#endif
