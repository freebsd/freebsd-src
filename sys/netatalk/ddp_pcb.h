/*
 * Copyright (c) 1990,1994 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 *
 * $FreeBSD$
 */

#ifndef _NETATALK_DDP_PCB_H_
#define	_NETATALK_DDP_PCB_H_

int	at_pcballoc(struct socket *so);
int	at_pcbconnect(struct ddpcb *ddp, struct sockaddr *addr, 
	    struct thread *td);
void	at_pcbdetach(struct socket *so, struct ddpcb *ddp);
void	at_pcbdisconnect(struct ddpcb *ddp);
int	at_pcbsetaddr(struct ddpcb *ddp, struct sockaddr *addr,
	    struct thread *td);
void	at_sockaddr(struct ddpcb *ddp, struct sockaddr **addr);

#endif
