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
#include <i386/isa/sound/sound_config.h>

#if NSND > 0

int             sound_started = 0;

int             sndtable_get_cardcount(void);
int             snd_find_driver(int type);
void            sndtable_init(void);
int             sndtable_probe(int unit, struct address_info * hw_config);
int             sndtable_init_card(int unit, struct address_info * hw_config);
int             sndtable_identify_card(char *name);
void            sound_chconf(int card_type, int ioaddr, int irq, int dma);
static void     start_services(void);
static void     start_cards(void);
struct address_info *sound_getconf(int card_type);

int
snd_find_driver(int type)
{
    int   i, n = num_sound_drivers;

    for (i = 0; i < n; i++)
	if (sound_drivers[i].card_type == type)
	    return i;

    return -1;
}

static void
start_services()
{
    int   soundcards_installed;

    if (!(soundcards_installed = sndtable_get_cardcount()))
	return ;	/* No cards detected */

#ifdef CONFIG_AUDIO
    if (num_audiodevs)	/* Audio devices present */
	DMAbuf_init();
#endif

#ifdef CONFIG_MIDI
    if (num_midis)
	/* MIDIbuf_init(0) */;
#endif

#ifdef CONFIG_SEQUENCER
    if (num_midis + num_synths)
	sequencer_init();
#endif
}

static void
start_cards()
{
    int  drv, i, n = num_sound_cards;
    struct card_info *ci = snd_installed_cards ;

    sound_started = 1;
    if (trace_init)
	printf("Sound initialization started\n");

    /*
     * Check the number of cards actually defined in the table
     */

    for (i = 0; i < n && snd_installed_cards[i].card_type; i++)
	num_sound_cards = i + 1;

    for (i = 0; i < n && ci->card_type; ci++, i++)
	if (ci->enabled) {
	    if ((drv = snd_find_driver(ci->card_type)) == -1) {
		ci->enabled = 0;	/* Mark as not detected */
		continue;
	    }
	    ci->config.card_subtype = sound_drivers[drv].card_subtype;

	    if (sound_drivers[drv].probe(&(ci->config)))
		sound_drivers[drv].attach(&(ci->config));
	    else
		ci->enabled = 0;	/* Mark as not detected */
	}
    if (trace_init)
	printf("Sound initialization complete\n");
}

void
sndtable_init()
{
    start_cards();
}

/*
 * sndtable_probe probes a specific device. unit is the voxware unit number.
 */

int
sndtable_probe(int unit, struct address_info * hw_config)
{
    int  i, sel = -1, n = num_sound_cards;
    struct card_info *ci = snd_installed_cards ;

    DDB(printf("-- sndtable_probe(%d)\n", unit));


    /*
     * for some reason unit 0 always succeeds ?
     */
    if (!unit)
	return TRUE;

    sound_started = 1;

    for (i=0; i<n && sel== -1 && ci->card_type; ci++, i++)
	if ( (ci->enabled) && (ci->card_type == unit) ) {
	    /* DDB(printf("-- found card %d\n", i) ); */
	    sel = i; /* and break */
	}

    /*
     * not found. Creates a new entry in the table for this unit.
     */
    if (sel == -1 && num_sound_cards < max_sound_cards) {
	int   i;

	i = sel = (num_sound_cards++);
	DDB(printf("-- installing card %d\n", i) );

	ci = &snd_installed_cards[sel] ;
	ci->card_type = unit;
	ci->enabled = 1;
    }
    /* DDB(printf("-- installed card %d\n", sel) ); */
    if (sel != -1) {
	int   drv;

	ci->config.io_base = hw_config->io_base;
	ci->config.irq = hw_config->irq;
	ci->config.dma = hw_config->dma;
	ci->config.dma2 = hw_config->dma2;
	ci->config.name = hw_config->name;
	ci->config.always_detect = hw_config->always_detect;
	ci->config.card_subtype = hw_config->card_subtype;
	ci->config.osp = hw_config->osp;

	if ((drv = snd_find_driver(ci->card_type)) == -1) {
	    ci->enabled = 0;
	    DDB(printf("Failed to find driver\n"));
	    return FALSE;
	}
	DDB(printf("-- Driver name '%s' probe 0x%08x\n",
		sound_drivers[drv].name, sound_drivers[drv].probe));

	hw_config->card_subtype =
	ci->config.card_subtype = sound_drivers[drv].card_subtype;

	if (sound_drivers[drv].probe(hw_config)) {
	    DDB(printf("-- Hardware probed OK\n"));
	    return TRUE;
	}
	DDB(printf("-- Failed to find hardware\n"));
	ci->enabled = 0;	/* mark as not detected */
	return FALSE;
    }
    return FALSE;
}

int
sndtable_init_card(int unit, struct address_info * hw_config)
{
    int     i, n = num_sound_cards;
    struct card_info *ci = snd_installed_cards ;

    DDB(printf("sndtable_init_card(%d) entered\n", unit));

    if (!unit) {
	sndtable_init() ;
	return TRUE;
    }
    for (i = 0; i < n && ci->card_type; ci++, i++)
	if (ci->card_type == unit) {
	    int             drv;

	    ci->config.io_base = hw_config->io_base;
	    ci->config.irq = hw_config->irq;
	    ci->config.dma = hw_config->dma;
	    ci->config.dma2 = hw_config->dma2;
	    ci->config.name = hw_config->name;
	    ci->config.always_detect = hw_config->always_detect;
	    ci->config.card_subtype = hw_config->card_subtype;
	    ci->config.osp = hw_config->osp;

	    if ((drv = snd_find_driver(ci->card_type)) == -1)
		ci->enabled = 0; /* Mark not fnd */
	    else {
		DDB(printf("Located card - calling attach routine\n"));
		sound_drivers[drv].attach(hw_config) ;
		DDB(printf("attach routine finished\n"));
	    }
	    start_services();
	    return TRUE;
	}
	DDB(printf("sndtable_init_card: No card defined with type=%d, num cards: %d\n",
	       unit, num_sound_cards));
    return FALSE;
}

int
sndtable_get_cardcount(void)
{
    return num_audiodevs + num_mixers + num_synths + num_midis;
}

int
sndtable_identify_card(char *name)
{
    int  i, n = num_sound_drivers;

    if (name == NULL)
	return 0;

    for (i = 0; i < n; i++)
	if (sound_drivers[i].driver_id != NULL) {
	    char           *id = sound_drivers[i].driver_id;
	    int             j;

	    for (j = 0; j < 80 && name[j] == id[j]; j++)
		if (id[j] == 0 && name[j] == 0)	/* Match */
		    return sound_drivers[i].card_type;
	}
    return 0;
}

void
sound_chconf(int card_type, int ioaddr, int irq, int dma)
{
    int   j, ptr = -1, n = num_sound_cards;

    for (j = 0; j < n && ptr == -1 && snd_installed_cards[j].card_type; j++)
	if (snd_installed_cards[j].card_type == card_type &&
	    !snd_installed_cards[j].enabled)	/* Not already found */
	    ptr = j;

    if (ptr != -1) {
	snd_installed_cards[ptr].enabled = 1;
	if (ioaddr)
	    snd_installed_cards[ptr].config.io_base = ioaddr;
	if (irq)
	    snd_installed_cards[ptr].config.irq = irq;
	if (dma)
	    snd_installed_cards[ptr].config.dma = dma;
	snd_installed_cards[ptr].config.dma2 = -1;
    }
}


struct address_info *
sound_getconf(int card_type)
{
    int    j, ptr = -1, n = num_sound_cards;

    for (j = 0; j < n && ptr == -1 && snd_installed_cards[j].card_type; j++)
	if (snd_installed_cards[j].card_type == card_type)
	    ptr = j;

    if (ptr == -1)
	return (struct address_info *) NULL;

    return &snd_installed_cards[ptr].config;
}

#endif
