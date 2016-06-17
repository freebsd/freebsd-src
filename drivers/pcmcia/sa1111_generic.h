extern int sa1111_pcmcia_init(struct pcmcia_init *);
extern int sa1111_pcmcia_shutdown(void);
extern int sa1111_pcmcia_socket_state(struct pcmcia_state_array *);
extern int sa1111_pcmcia_get_irq_info(struct pcmcia_irq_info *);
extern int sa1111_pcmcia_configure_socket(const struct pcmcia_configure *);
extern int sa1111_pcmcia_socket_init(int);
extern int sa1111_pcmcia_socket_suspend(int);

/*
 * I'd really like to move the INTPOL stuff to arch/arm/mach-sa1100/sa1111.c
 */
#define SA1111_IRQMASK_LO(x)	(1 << (x - IRQ_SA1111_START))
#define SA1111_IRQMASK_HI(x)	(1 << (x - IRQ_SA1111_START - 32))


