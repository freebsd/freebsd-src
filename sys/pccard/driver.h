/*-
 * pccard driver interface.
 * Bruce Evans, November 1995.
 * This file is in the public domain.
 */

#ifndef _PCCARD_DRIVER_H_
#define	_PCCARD_DRIVER_H_

struct lkm_table;
struct pccard_device;
extern struct linker_set pccarddrv_set;

void	pccard_add_driver __P((struct pccard_device *));
#ifdef _I386_ISA_ISA_DEVICE_H_ /* XXX actually if inthand2_t is declared */
int	pccard_alloc_intr __P((u_int imask, inthand2_t *hand, int unit,
			       u_int *maskp, u_int *pcic_imask));
#endif
void	pccard_configure __P((void));
void	pccard_remove_driver __P((struct pccard_device *));
int	pcic_probe __P((void));	/* XXX should be linker set */

enum beepstate { BEEP_ON, BEEP_OFF };

void	pccard_insert_beep __P((void));
void	pccard_remove_beep __P((void));
void	pccard_success_beep __P((void));
void	pccard_failure_beep __P((void));
void	pccard_beep_select __P((enum beepstate));

#endif /* !_PCCARD_DRIVER_H_ */
