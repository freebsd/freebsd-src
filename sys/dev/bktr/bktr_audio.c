/* $FreeBSD$ */
/*
 * This is part of the Driver for Video Capture Cards (Frame grabbers)
 * and TV Tuner cards using the Brooktree Bt848, Bt848A, Bt849A, Bt878, Bt879
 * chipset.
 * Copyright Roger Hardiman and Amancio Hasty.
 *
 * bktr_audio : This deals with controlling the audio on TV cards,
 *                controlling the Audio Multiplexer (audio source selector).
 *                controlling any MSP34xx stereo audio decoders.
 *                controlling any DPL35xx dolby surroud sound audio decoders.    
 *                initialising TDA98xx audio devices.
 *
 */

/*
 * 1. Redistributions of source code must retain the
 * Copyright (c) 1997 Amancio Hasty, 1999 Roger Hardiman
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Amancio Hasty and
 *      Roger Hardiman
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/vnode.h>

#include <machine/clock.h>		/* for DELAY */

#include <pci/pcivar.h>

#include <machine/ioctl_meteor.h>
#include <machine/ioctl_bt848.h>        /* extensions to ioctl_meteor.h */
#include <dev/bktr/bktr_reg.h>
#include <dev/bktr/bktr_core.h>
#include <dev/bktr/bktr_tuner.h>
#include <dev/bktr/bktr_card.h>
#include <dev/bktr/bktr_audio.h>


/*
 * Prototypes for the GV_BCTV specific functions.
 */
void    set_bctv_audio( bktr_ptr_t bktr );
void    bctv_gpio_write( bktr_ptr_t bktr, int port, int val );
/*int   bctv_gpio_read( bktr_ptr_t bktr, int port );*/ /* Not used */



/*
 * init_audio_devices
 * Reset any MSP34xx or TDA98xx audio devices.
 */
void init_audio_devices( bktr_ptr_t bktr ) {

        /* enable stereo if appropriate on TDA audio chip */
        if ( bktr->card.dbx )
                init_BTSC( bktr );
 
        /* reset the MSP34xx stereo audio chip */
        if ( bktr->card.msp3400c )
                msp_dpl_reset( bktr, bktr->msp_addr );

        /* reset the DPL35xx dolby audio chip */
        if ( bktr->card.dpl3518a )
                msp_dpl_reset( bktr, bktr->dpl_addr );

}


/*
 * 
 */
#define AUDIOMUX_DISCOVER_NOT
int
set_audio( bktr_ptr_t bktr, int cmd )
{
	bt848_ptr_t	bt848;
	u_long		temp;
	volatile u_char	idx;

#if defined( AUDIOMUX_DISCOVER )
	if ( cmd >= 200 )
		cmd -= 200;
	else
#endif /* AUDIOMUX_DISCOVER */

	/* check for existance of audio MUXes */
	if ( !bktr->card.audiomuxs[ 4 ] )
		return( -1 );

	switch (cmd) {
	case AUDIO_TUNER:
#ifdef BKTR_REVERSEMUTE
		bktr->audio_mux_select = 3;
#else
		bktr->audio_mux_select = 0;
#endif

		if (bktr->reverse_mute ) 
		      bktr->audio_mux_select = 0;
		else	
		    bktr->audio_mux_select = 3;

		break;
	case AUDIO_EXTERN:
		bktr->audio_mux_select = 1;
		break;
	case AUDIO_INTERN:
		bktr->audio_mux_select = 2;
		break;
	case AUDIO_MUTE:
		bktr->audio_mute_state = TRUE;	/* set mute */
		break;
	case AUDIO_UNMUTE:
		bktr->audio_mute_state = FALSE;	/* clear mute */
		break;
	default:
		printf("bktr: audio cmd error %02x\n", cmd);
		return( -1 );
	}


	/* Most cards have a simple audio multiplexer to select the
	 * audio source. The I/O_GV card has a more advanced multiplexer
	 * and requires special handling.
	 */
        if ( bktr->bt848_card == CARD_IO_GV ) {
                set_bctv_audio( bktr );
                return( 0 );
	}

	/* Proceed with the simpler audio multiplexer code for the majority
	 * of Bt848 cards.
	 */

	bt848 =	bktr->base;

	/*
	 * Leave the upper bits of the GPIO port alone in case they control
	 * something like the dbx or teletext chips.  This doesn't guarantee
	 * success, but follows the rule of least astonishment.
	 */

	if ( bktr->audio_mute_state == TRUE ) {
#ifdef BKTR_REVERSEMUTE
		idx = 0;
#else
		idx = 3;
#endif

		if (bktr->reverse_mute )
		  idx  = 3;
		else	
		  idx  = 0;

	}
	else
		idx = bktr->audio_mux_select;

	temp = bt848->gpio_data & ~bktr->card.gpio_mux_bits;
	bt848->gpio_data =
#if defined( AUDIOMUX_DISCOVER )
		bt848->gpio_data = temp | (cmd & 0xff);
		printf("cmd: %d audio mux %x temp %x \n", cmd,bktr->card.audiomuxs[ idx ], temp );
#else
		temp | bktr->card.audiomuxs[ idx ];
#endif /* AUDIOMUX_DISCOVER */

	return( 0 );
}


/*
 * 
 */
void
temp_mute( bktr_ptr_t bktr, int flag )
{
	static int	muteState = FALSE;

	if ( flag == TRUE ) {
		muteState = bktr->audio_mute_state;
		set_audio( bktr, AUDIO_MUTE );		/* prevent 'click' */
	}
	else {
		tsleep( BKTR_SLEEP, PZERO, "tuning", hz/8 );
		if ( muteState == FALSE )
			set_audio( bktr, AUDIO_UNMUTE );
	}
}

/* address of BTSC/SAP decoder chip */
#define TDA9850_WADDR           0xb6
#define TDA9850_RADDR           0xb7


/* registers in the TDA9850 BTSC/dbx chip */
#define CON1ADDR                0x04
#define CON2ADDR                0x05
#define CON3ADDR                0x06 
#define CON4ADDR                0x07
#define ALI1ADDR                0x08 
#define ALI2ADDR                0x09
#define ALI3ADDR                0x0a

/*
 * initialise the dbx chip
 * taken from the Linux bttv driver TDA9850 initialisation code
 */
void 
init_BTSC( bktr_ptr_t bktr )
{
    i2cWrite(bktr, TDA9850_WADDR, CON1ADDR, 0x08); /* noise threshold st */
    i2cWrite(bktr, TDA9850_WADDR, CON2ADDR, 0x08); /* noise threshold sap */
    i2cWrite(bktr, TDA9850_WADDR, CON3ADDR, 0x40); /* stereo mode */
    i2cWrite(bktr, TDA9850_WADDR, CON4ADDR, 0x07); /* 0 dB input gain? */
    i2cWrite(bktr, TDA9850_WADDR, ALI1ADDR, 0x10); /* wideband alignment? */
    i2cWrite(bktr, TDA9850_WADDR, ALI2ADDR, 0x10); /* spectral alignment? */
    i2cWrite(bktr, TDA9850_WADDR, ALI3ADDR, 0x03);
}

/*
 * setup the dbx chip
 * XXX FIXME: alot of work to be done here, this merely unmutes it.
 */
int
set_BTSC( bktr_ptr_t bktr, int control )
{
	return( i2cWrite( bktr, TDA9850_WADDR, CON3ADDR, control ) );
}

/*
 * CARD_GV_BCTV specific functions.
 */

#define BCTV_AUDIO_MAIN              0x10    /* main audio program */
#define BCTV_AUDIO_SUB               0x20    /* sub audio program */
#define BCTV_AUDIO_BOTH              0x30    /* main(L) + sub(R) program */

#define BCTV_GPIO_REG0          1
#define BCTV_GPIO_REG1          3

#define BCTV_GR0_AUDIO_MODE     3
#define BCTV_GR0_AUDIO_MAIN     0       /* main program */
#define BCTV_GR0_AUDIO_SUB      3       /* sub program */
#define BCTV_GR0_AUDIO_BOTH     1       /* main(L) + sub(R) */
#define BCTV_GR0_AUDIO_MUTE     4       /* audio mute */
#define BCTV_GR0_AUDIO_MONO     8       /* force mono */

void
set_bctv_audio( bktr_ptr_t bktr )
{
        int data;

        switch (bktr->audio_mux_select) {
        case 1:         /* external */
        case 2:         /* internal */
                bctv_gpio_write(bktr, BCTV_GPIO_REG1, 0);
                break;
        default:        /* tuner */
                bctv_gpio_write(bktr, BCTV_GPIO_REG1, 1);
                break;
        }
/*      switch (bktr->audio_sap_select) { */
        switch (BCTV_AUDIO_BOTH) {
        case BCTV_AUDIO_SUB:
                data = BCTV_GR0_AUDIO_SUB;
                break;
        case BCTV_AUDIO_BOTH:
                data = BCTV_GR0_AUDIO_BOTH;
                break;
        case BCTV_AUDIO_MAIN:
        default:
                data = BCTV_GR0_AUDIO_MAIN;
                break;
        }
        if (bktr->audio_mute_state == TRUE)
                data |= BCTV_GR0_AUDIO_MUTE;

        bctv_gpio_write(bktr, BCTV_GPIO_REG0, data);

        return;
}

/* gpio_data bit assignment */
#define BCTV_GPIO_ADDR_MASK     0x000300
#define BCTV_GPIO_WE            0x000400
#define BCTV_GPIO_OE            0x000800
#define BCTV_GPIO_VAL_MASK      0x00f000

#define BCTV_GPIO_PORT_MASK     3
#define BCTV_GPIO_ADDR_SHIFT    8
#define BCTV_GPIO_VAL_SHIFT     12

/* gpio_out_en value for read/write */
#define BCTV_GPIO_OUT_RMASK     0x000f00
#define BCTV_GPIO_OUT_WMASK     0x00ff00

#define BCTV_BITS       100

void
bctv_gpio_write( bktr_ptr_t bktr, int port, int val )
{
        bt848_ptr_t bt848 = bktr->base;
        u_long data, outbits;

        port &= BCTV_GPIO_PORT_MASK;
        switch (port) {
        case 1:
        case 3:
                data = ((val << BCTV_GPIO_VAL_SHIFT) & BCTV_GPIO_VAL_MASK) |
                       ((port << BCTV_GPIO_ADDR_SHIFT) & BCTV_GPIO_ADDR_MASK) |
                       BCTV_GPIO_WE | BCTV_GPIO_OE;
                outbits = BCTV_GPIO_OUT_WMASK;
                break;
        default:
                return;
        }
        bt848->gpio_out_en = 0;
        bt848->gpio_data = data;
        bt848->gpio_out_en = outbits;
        DELAY(BCTV_BITS);
        bt848->gpio_data = data & ~BCTV_GPIO_WE;
        DELAY(BCTV_BITS);
        bt848->gpio_data = data;
        DELAY(BCTV_BITS);
        bt848->gpio_data = ~0;
        bt848->gpio_out_en = 0;
}

/* Not yet used
int
bctv_gpio_read( bktr_ptr_t bktr, int port )
{
        bt848_ptr_t bt848 = bktr->base;
        u_long data, outbits, ret;

        port &= BCTV_GPIO_PORT_MASK;
        switch (port) {
        case 1:
        case 3:
                data = ((port << BCTV_GPIO_ADDR_SHIFT) & BCTV_GPIO_ADDR_MASK) |
                       BCTV_GPIO_WE | BCTV_GPIO_OE;
                outbits = BCTV_GPIO_OUT_RMASK;
                break;
        default:
                return( -1 );
        }
        bt848->gpio_out_en = 0;
        bt848->gpio_data = data;
        bt848->gpio_out_en = outbits;
        DELAY(BCTV_BITS);
        bt848->gpio_data = data & ~BCTV_GPIO_OE;
        DELAY(BCTV_BITS);
        ret = bt848->gpio_data;
        DELAY(BCTV_BITS);
        bt848->gpio_data = data;
        DELAY(BCTV_BITS);
        bt848->gpio_data = ~0;
        bt848->gpio_out_en = 0;
        return( (ret & BCTV_GPIO_VAL_MASK) >> BCTV_GPIO_VAL_SHIFT );
}
*/

/*
 * setup the MSP34xx Stereo Audio Chip
 * This uses the Auto Configuration Option on MSP3410D and MSP3415D chips
 * and DBX mode selection for MSP3430G chips.
 * For MSP3400C support, the full programming sequence is required and is
 * not yet supported.
 */

/* Read the MSP version string */
void msp_read_id( bktr_ptr_t bktr ){
    int rev1=0, rev2=0;
    rev1 = msp_dpl_read(bktr, bktr->msp_addr, 0x12, 0x001e);
    rev2 = msp_dpl_read(bktr, bktr->msp_addr, 0x12, 0x001f);

    sprintf(bktr->msp_version_string, "34%02d%c-%c%d",
      (rev2>>8)&0xff, (rev1&0xff)+'@', ((rev1>>8)&0xff)+'@', rev2&0x1f);

}


/* Configure the MSP chip to Auto-detect the audio format */
void msp_autodetect( bktr_ptr_t bktr ) {

  if (strncmp("3430G", bktr->msp_version_string, 5) == 0){

    /* For MSP3430G - countries with mono and DBX stereo */
    msp_dpl_write(bktr, bktr->msp_addr, 0x10, 0x0030,0x2003);/* Enable Auto format detection */
    msp_dpl_write(bktr, bktr->msp_addr, 0x10, 0x0020,0x0020);/* Standard Select Reg. = BTSC-Stereo*/
    msp_dpl_write(bktr, bktr->msp_addr, 0x12, 0x000E,0x2403);/* darned if I know */
    msp_dpl_write(bktr, bktr->msp_addr, 0x12, 0x0008,0x0320);/* Source select = (St or A) */
					 /*   & Ch. Matrix = St */
    msp_dpl_write(bktr, bktr->msp_addr, 0x12, 0x0000,0x7300);/* Set volume to 0db gain */

  } else {

    /* For MSP3410 / 3415 - countries with mono, stereo using 2 FM channels
       and NICAM */
    msp_dpl_write(bktr, bktr->msp_addr, 0x12, 0x0000,0x7300);/* Set volume to 0db gain */
    msp_dpl_write(bktr, bktr->msp_addr, 0x10, 0x0020,0x0001);/* Enable Auto format detection */
    msp_dpl_write(bktr, bktr->msp_addr, 0x10, 0x0021,0x0001);/* Auto selection of NICAM/MONO mode */
  }

  /* uncomment the following line to enable the MSP34xx 1Khz Tone Generator */
  /* turn your speaker volume down low before trying this */
  /* msp_dpl_write(bktr, bktr->msp_addr, 0x12, 0x0014, 0x7f40); */
}

/* Read the DPL version string */
void dpl_read_id( bktr_ptr_t bktr ){
    int rev1=0, rev2=0;
    rev1 = msp_dpl_read(bktr, bktr->dpl_addr, 0x12, 0x001e);
    rev2 = msp_dpl_read(bktr, bktr->dpl_addr, 0x12, 0x001f);

    sprintf(bktr->dpl_version_string, "34%02d%c-%c%d",
      ((rev2>>8)&0xff)-1, (rev1&0xff)+'@', ((rev1>>8)&0xff)+'@', rev2&0x1f);
}

/* Configure the DPL chip to Auto-detect the audio format */
void dpl_autodetect( bktr_ptr_t bktr ) {

    /* The following are empiric values tried from the DPL35xx data sheet */
    msp_dpl_write(bktr, bktr->dpl_addr, 0x12, 0x000c,0x0320);	/* quasi peak detector source dolby
								lr 0x03xx; quasi peak detector matrix
								stereo 0xXX20 */
    msp_dpl_write(bktr, bktr->dpl_addr, 0x12, 0x0040,0x0060);	/* Surround decoder mode;
								ADAPTIVE/3D-PANORAMA, that means two
								speakers and no center speaker, all
								channels L/R/C/S mixed to L and R */
    msp_dpl_write(bktr, bktr->dpl_addr, 0x12, 0x0041,0x0620);	/* surround source matrix;I2S2/STEREO*/
    msp_dpl_write(bktr, bktr->dpl_addr, 0x12, 0x0042,0x1F00);	/* surround delay 31ms max */
    msp_dpl_write(bktr, bktr->dpl_addr, 0x12, 0x0043,0x0000);	/* automatic surround input balance */
    msp_dpl_write(bktr, bktr->dpl_addr, 0x12, 0x0044,0x4000);	/* surround spatial effect 50%
								recommended*/
    msp_dpl_write(bktr, bktr->dpl_addr, 0x12, 0x0045,0x5400);	/* surround panorama effect 66%
								recommended with PANORAMA mode
								in 0x0040 set to panorama */
}

