/*
 * linux/kernel/chr_drv/sound/dev_table.c
 * 
 * Device call tables.
 * 
 * (C) 1992  Hannu Savolainen (hsavolai@cs.helsinki.fi) See COPYING for further
 * details. Should be distributed with this file.
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
