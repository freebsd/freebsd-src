/*
 * sound/dev_table.c
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
    if (supported_drivers[i].enabled)
      if (supported_drivers[i].probe (&supported_drivers[i].config))
	{
#ifndef SHORT_BANNERS
	  printk ("snd%d",
		  supported_drivers[i].card_type);
#endif

	  mem_start = supported_drivers[i].attach (mem_start, &supported_drivers[i].config);
#ifndef SHORT_BANNERS
	  printk (" at 0x%x irq %d drq %d\n",
		  supported_drivers[i].config.io_base,
		  supported_drivers[i].config.irq,
		  supported_drivers[i].config.dma);
#endif
	}
      else
	supported_drivers[i].enabled = 0;	/* Mark as not detected */
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
      {
	supported_drivers[i].config.io_base = hw_config->io_base;
	supported_drivers[i].config.irq = hw_config->irq;
	supported_drivers[i].config.dma = hw_config->dma;
	if (supported_drivers[i].probe (hw_config))
	  return 1;
	supported_drivers[i].enabled = 0;	/* Mark as not detected */
	return 0;
      }

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
	supported_drivers[i].config.io_base = hw_config->io_base;
	supported_drivers[i].config.irq = hw_config->irq;
	supported_drivers[i].config.dma = hw_config->dma;

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

#ifdef linux
void
sound_setup (char *str, int *ints)
{
  int             i, n = sizeof (supported_drivers) / sizeof (struct card_info);

  /*
 * First disable all drivers
 */

  for (i = 0; i < n; i++)
    supported_drivers[i].enabled = 0;

  if (ints[0] == 0 || ints[1] == 0)
    return;
  /*
 * Then enable them one by time
 */

  for (i = 1; i <= ints[0]; i++)
    {
      int             card_type, ioaddr, irq, dma, ptr, j;
      unsigned int    val;

      val = (unsigned int) ints[i];

      card_type = (val & 0x0ff00000) >> 20;

      if (card_type > 127)
	{
	  /* Add any future extensions here */
	  return;
	}

      ioaddr = (val & 0x000fff00) >> 8;
      irq = (val & 0x000000f0) >> 4;
      dma = (val & 0x0000000f);

      ptr = -1;
      for (j = 0; j < n && ptr == -1; j++)
	if (supported_drivers[j].card_type == card_type)
	  ptr = j;

      if (ptr == -1)
	printk ("Sound: Invalid setup parameter 0x%08x\n", val);
      else
	{
	  supported_drivers[ptr].enabled = 1;
	  supported_drivers[ptr].config.io_base = ioaddr;
	  supported_drivers[ptr].config.irq = irq;
	  supported_drivers[ptr].config.dma = dma;
	}
    }
}

#else
void
sound_chconf (int card_type, int ioaddr, int irq, int dma)
{
  int             i, n = sizeof (supported_drivers) / sizeof (struct card_info);

  int             ptr, j;

  ptr = -1;
  for (j = 0; j < n && ptr == -1; j++)
    if (supported_drivers[j].card_type == card_type)
      ptr = j;

  if (ptr != -1)
    {
      supported_drivers[ptr].enabled = 1;
      if (ioaddr)
	supported_drivers[ptr].config.io_base = ioaddr;
      if (irq)
	supported_drivers[ptr].config.irq = irq;
      if (dma)
	supported_drivers[ptr].config.dma = dma;
    }
}

#endif

struct address_info *
sound_getconf (int card_type)
{
  int             j, ptr;
  int             n = sizeof (supported_drivers) / sizeof (struct card_info);

  ptr = -1;
  for (j = 0; j < n && ptr == -1; j++)
    if (supported_drivers[j].card_type == card_type)
      ptr = j;

  if (ptr == -1)
    return (struct address_info *) NULL;

  return &supported_drivers[ptr].config;
}

#endif
