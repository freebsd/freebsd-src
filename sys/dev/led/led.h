/*-
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@FreeBSD.org> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 */

#ifndef _DEV_LED_H
#define _DEV_LED_H

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

typedef	void led_t(void *, int);
dev_t led_create(led_t *func, void *priv, char const *name);
void led_destroy(dev_t *dev);

#endif /* _DEV_LED_H */
