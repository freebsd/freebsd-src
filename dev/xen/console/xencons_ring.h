/*
 * $FreeBSD$
 *
 */
#ifndef _XENCONS_RING_H
#define _XENCONS_RING_H

int xencons_ring_init(void);
int xencons_ring_send(const char *data, unsigned len);
void xencons_rx(char *buf, unsigned len);
void xencons_tx(void);


typedef void (xencons_receiver_func)(char *buf, unsigned len);
void xencons_ring_register_receiver(xencons_receiver_func *f);

void xencons_handle_input(void *unused);
int xencons_has_input(void);

#endif /* _XENCONS_RING_H */
