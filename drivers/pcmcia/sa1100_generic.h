/*
 * linux/include/asm/arch/pcmcia.h
 *
 * Copyright (C) 2000 John G Dorsey <john+@cs.cmu.edu>
 *
 * This file contains definitions for the low-level SA-1100 kernel PCMCIA
 * interface. Please see linux/Documentation/arm/SA1100/PCMCIA for details.
 */
#ifndef _ASM_ARCH_PCMCIA
#define _ASM_ARCH_PCMCIA

/* Ideally, we'd support up to MAX_SOCK sockets, but the SA-1100 only
 * has support for two. This shows up in lots of hardwired ways, such
 * as the fact that MECR only has enough bits to configure two sockets.
 * Since it's so entrenched in the hardware, limiting the software
 * in this way doesn't seem too terrible.
 */
#define SA1100_PCMCIA_MAX_SOCK   (2)

struct pcmcia_init {
  void (*handler)(int irq, void *dev, struct pt_regs *regs);
};

struct pcmcia_state {
  unsigned detect: 1,
            ready: 1,
             bvd1: 1,
             bvd2: 1,
           wrprot: 1,
            vs_3v: 1,
            vs_Xv: 1;
};

struct pcmcia_state_array {
  unsigned int size;
  struct pcmcia_state *state;
};

struct pcmcia_configure {
  unsigned sock: 8,
            vcc: 8,
            vpp: 8,
         output: 1,
        speaker: 1,
          reset: 1,
            irq: 1;
};

struct pcmcia_irq_info {
  unsigned int sock;
  unsigned int irq;
};

struct pcmcia_low_level {
  int (*init)(struct pcmcia_init *);
  int (*shutdown)(void);
  int (*socket_state)(struct pcmcia_state_array *);
  int (*get_irq_info)(struct pcmcia_irq_info *);
  int (*configure_socket)(const struct pcmcia_configure *);

  /*
   * Enable card status IRQs on (re-)initialisation.  This can
   * be called at initialisation, power management event, or
   * pcmcia event.
   */
  int (*socket_init)(int sock);

  /*
   * Disable card status IRQs and PCMCIA bus on suspend.
   */
  int (*socket_suspend)(int sock);
};

extern struct pcmcia_low_level *pcmcia_low_level;

#endif
