
/*
 * linux/kernel/chr_drv/sound/adlib_card.c
 * 
 * Detection routine for the AdLib card.
 * 
 * (C) 1992  Hannu Savolainen (hsavolai@cs.helsinki.fi) See COPYING for further
 * details. Should be distributed with this file.
 */

#include "sound_config.h"

#if defined(CONFIGURE_SOUNDCARD) && !defined(EXCLUDE_YM3812)

long
attach_adlib_card (long mem_start, struct address_info *hw_config)
{

  if (opl3_detect (FM_MONO))
    {
      mem_start = opl3_init (mem_start);
    }
  return mem_start;
}

int
probe_adlib (struct address_info *hw_config)
{
  return opl3_detect (FM_MONO);
}

#endif
