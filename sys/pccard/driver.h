/*-
 * pccard driver interface.
 * Bruce Evans, November 1995.
 * This file is in the public domain.
 *
 * $FreeBSD$
 */

#ifndef _PCCARD_DRIVER_H_
#define	_PCCARD_DRIVER_H_

struct pccard_device;

void	pccard_add_driver(struct pccard_device *);

enum beepstate { BEEP_OFF, BEEP_ON };

void	pccard_insert_beep(void);
void	pccard_remove_beep(void);
void	pccard_success_beep(void);
void	pccard_failure_beep(void);
int	pccard_beep_select(int);

#endif /* !_PCCARD_DRIVER_H_ */
