
/*
 * linux/kernel/chr_drv/sound/sb_card.c
 * 
 * Detection routine for the SoundBlaster cards.
 * 
 * (C) 1992  Hannu Savolainen (hsavolai@cs.helsinki.fi) See COPYING for further
 * details. Should be distributed with this file.
 */

#include "sound_config.h"

#if defined(CONFIGURE_SOUNDCARD) && !defined(EXCLUDE_SB)

long
attach_sb_card (long mem_start, struct address_info *hw_config)
{
#if !defined(EXCLUDE_AUDIO) || !defined(EXCLUDE_MIDI)
  if (!sb_dsp_detect (hw_config))
    return mem_start;
  mem_start = sb_dsp_init (mem_start, hw_config);
#endif

  return mem_start;
}

int
probe_sb (struct address_info *hw_config)
{
  return sb_dsp_detect (hw_config);
}

#endif
