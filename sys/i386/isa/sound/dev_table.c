/*
 * linux/kernel/chr_drv/sound/dev_table.c
 * 
 * Device call tables.
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
 */

#define _DEV_TABLE_C_
#include "sound_config.h"

#ifdef CONFIGURE_SOUNDCARD

long
sndtable_init (long mem_start)
{
  int             i, n = sizeof (supported_drivers) / sizeof (struct card_info);

  for (i = 0; i < (n - 1); i++)
    if (supported_drivers[i].probe (&supported_drivers[i].config))
      {
#ifndef SHORT_BANNERS
	printk ("snd%d",
		supported_drivers[i].card_type);
#endif

	mem_start = supported_drivers[i].attach (mem_start, &supported_drivers[i].config);
#ifndef SHORT_BANNERS
	printk (" at 0x%03x irq %d drq %d\n",
		supported_drivers[i].config.io_base,
		supported_drivers[i].config.irq,
		supported_drivers[i].config.dma);
#endif
      }
  return mem_start;
}

int
sndtable_probe (int unit, struct address_info *hw_config)
{
  int             i, n = sizeof (supported_drivers) / sizeof (struct card_info);

  if (!unit)
    return TRUE;

  for (i = 0; i < (n - 1); i++)
    if (supported_drivers[i].card_type == unit)
      return supported_drivers[i].probe (hw_config);

  return FALSE;
}

int
sndtable_init_card (int unit, struct address_info *hw_config)
{
  int             i, n = sizeof (supported_drivers) / sizeof (struct card_info);

  if (!unit)
    {
      if (sndtable_init (0) != 0)
	panic ("snd: Invalid memory allocation\n");
      return TRUE;
    }

  for (i = 0; i < (n - 1); i++)
    if (supported_drivers[i].card_type == unit)
      {
	if (supported_drivers[i].attach (0, hw_config) != 0)
	  panic ("snd#: Invalid memory allocation\n");
	return TRUE;
      }

  return FALSE;
}

int
sndtable_get_cardcount (void)
{
  return num_dspdevs + num_mixers + num_synths + num_midis;
}

#endif
