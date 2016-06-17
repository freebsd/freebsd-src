#ifndef __LINUX_PCI_GAMEPORT_H
#define __LINUX_PCI_GAMEPORT_H

/*
 *	Public interfaces for attaching a PCI gameport directly to the
 *	soundcard when it shares the same PCI ident
 */
 
#define PCIGAME_4DWAVE		0
#define PCIGAME_VORTEX		1
#define PCIGAME_VORTEX2		2


struct pcigame_data {
	int gcr;	/* Gameport control register */
	int legacy;	/* Legacy port location */
	int axes;	/* Axes start */
	int axsize;	/* Axis field size */
	int axmax;	/* Axis field max value */
	int adcmode;	/* Value to enable ADC mode in GCR */
};

struct pcigame {
	struct gameport gameport;
	struct pci_dev *dev;
        unsigned char *base;
	struct pcigame_data *data;
};


#if defined(CONFIG_INPUT_PCIGAME) || defined(CONFIG_INPUT_PCIGAME_MODULE)
extern struct pcigame *pcigame_attach(struct pci_dev *dev, int type);
extern void pcigame_detach(struct pcigame *game);
#else
#define pcigame_attach(a,b)	NULL
#define pcigame_detach(a)
#endif

#endif
