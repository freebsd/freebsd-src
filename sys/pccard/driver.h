/*-
 * pccard driver interface.
 * Bruce Evans, November 1995.
 * This file is in the public domain.
 *
 * $FreeBSD: src/sys/pccard/driver.h,v 1.12 1999/12/02 19:46:40 imp Exp $
 */

#ifndef _PCCARD_DRIVER_H_
#define	_PCCARD_DRIVER_H_

struct pccard_device;

void	pccard_add_driver __P((struct pccard_device *));

enum beepstate { BEEP_OFF, BEEP_ON };

void	pccard_insert_beep __P((void));
void	pccard_remove_beep __P((void));
void	pccard_success_beep __P((void));
void	pccard_failure_beep __P((void));
int	pccard_beep_select __P((enum beepstate));

#endif /* !_PCCARD_DRIVER_H_ */
