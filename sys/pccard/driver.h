/*-
 * pccard driver interface.
 * Bruce Evans, November 1995.
 * This file is in the public domain.
 */

#ifndef _PCCARD_DRIVER_H_
#define	_PCCARD_DRIVER_H_

struct lkm_table;
struct pccard_drv;

void	pccard_add_driver __P((struct pccard_drv *));
#ifdef _I386_ISA_ISA_DEVICE_H_ /* XXX actually if inthand2_t is declared */
int	pccard_alloc_intr __P((u_int imask, inthand2_t *hand, int unit,
			       u_int *maskp, u_int *pcic_imask));
#endif
void	pccard_configure __P((void));
void	pccard_remove_driver __P((struct pccard_drv *dp));
int	pcic_probe __P((void));	/* XXX should be linker set */

#endif /* !_PCCARD_DRIVER_H_ */
