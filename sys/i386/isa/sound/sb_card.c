/*
 * sound/sb_card.c
 * 
 * Detection routine for the SoundBlaster cards.
 * 
 * Copyright by Hannu Savolainen 1993
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer. 2.
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * 
 * Modified: Riccardo Facchetti  24 Mar 1995 - Added the Audio Excel DSP 16
 * initialization routine.
 *
 * Major code cleanup - Luigi Rizzo (luigi@iet.unipi.it) 970711
 */

#include <i386/isa/sound/sound_config.h>

#if NSB > 0
#include  <i386/isa/sound/sbcard.h>

void
attach_sb_card(struct address_info * hw_config)
{
#if defined(CONFIG_AUDIO) || defined(CONFIG_MIDI)

#if 0
    /* why do a detect during the attach ? XXX */
    if (!sb_dsp_detect(hw_config))
	return ;
#endif
    sb_dsp_init(hw_config);
#endif

    return ;
}

int
probe_sb(struct address_info * hw_config)
{

#if defined(CONFIG_AEDSP16) && defined(AEDSP16_SBPRO)
    /*
     * Initialize Audio Excel DSP 16 to SBPRO.
     */
    InitAEDSP16_SBPRO(hw_config);
#endif
    return sb_dsp_detect(hw_config);
}
#endif
