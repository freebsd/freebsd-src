/*-
 * Copyright (c) 2007, Chelsio Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Neither the name of the Chelsio Corporation nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */
#ifndef _T3CDEV_H_
#define _T3CDEV_H_

#define T3CNAMSIZ 16

/* Get the t3cdev associated with an ifnet */
#define T3CDEV(ifp) (&(((struct port_info *)(ifp)->if_softc))->adapter->tdev)

struct cxgb3_client;

enum t3ctype {
        T3A = 0,
        T3B,
	T3C
};

struct t3cdev {
	char name[T3CNAMSIZ];		    /* T3C device name */
	enum t3ctype type;
	TAILQ_ENTRY(t3cdev) entry;  /* for list linking */
        struct ifnet *lldev;     /* LL dev associated with T3C messages */
	struct adapter *adapter;			    
	int (*send)(struct t3cdev *dev, struct mbuf *m);
	int (*recv)(struct t3cdev *dev, struct mbuf **m, int n);
	int (*ctl)(struct t3cdev *dev, unsigned int req, void *data);
	void (*arp_update)(struct t3cdev *dev, struct rtentry *neigh, uint8_t *enaddr, struct sockaddr *sa);
	void *priv;                         /* driver private data */
	void *l2opt;                        /* optional layer 2 data */
	void *l3opt;                        /* optional layer 3 data */
	void *l4opt;                        /* optional layer 4 data */
	void *ulp;			    /* ulp stuff */
};

#endif /* _T3CDEV_H_ */
