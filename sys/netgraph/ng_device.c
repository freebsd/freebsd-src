/*
 * Copyright (c) 2002 Mark Santcroos <marks@ripe.net>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Netgraph "device" node
 *
 * This node presents a /dev/ngd%d device that interfaces to an other 
 * netgraph node.
 *
 * $FreeBSD$
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/uio.h>
#include <sys/queue.h>
#include <sys/malloc.h>
#include <sys/conf.h>
#include <sys/poll.h>
#include <sys/ioccom.h>

#include <netgraph/ng_message.h>
#include <netgraph/netgraph.h>

#include "ng_device.h"

/* turn this on for verbose messages */
#define NGD_DEBUG

/* Netgraph methods */
static ng_constructor_t	ng_device_cons;
static ng_rcvmsg_t	ng_device_rcvmsg;
static ng_newhook_t	ng_device_newhook;
static ng_connect_t 	ng_device_connect;
static ng_rcvdata_t	ng_device_rcvdata;
static ng_disconnect_t	ng_device_disconnect;
static int              ng_device_mod_event(module_t mod, int event, void *data);

static int ng_device_init(void);
static int get_free_unit(void);

/* Netgraph type */
static struct ng_type typestruct = {
	NG_ABI_VERSION,			/* version */
	NG_DEVICE_NODE_TYPE,		/* name */
	ng_device_mod_event,		/* modevent */
	ng_device_cons, 		/* constructor */
	ng_device_rcvmsg, 		/* receive msg */
	NULL, 				/* shutdown */
	ng_device_newhook, 		/* newhook */
	NULL,				/* findhook */
	ng_device_connect, 		/* connect */
	ng_device_rcvdata, 		/* receive data */
	ng_device_disconnect, 		/* disconnect */
	NULL
};
NETGRAPH_INIT(device, &typestruct);

/* per hook data */
struct ngd_connection {
	SLIST_ENTRY(ngd_connection) links;

	dev_t 	ngddev;
	struct 	ng_hook *active_hook;
	char	*readq;
	int 	loc;
	int 	unit;
};

/* global data */
struct ngd_softc {
	SLIST_HEAD(, ngd_connection) head;

	node_p node;
	char nodename[NG_NODELEN + 1];
} ngd_softc;

/* the per connection receiving queue maximum */
#define NGD_QUEUE_SIZE (1024*10)

/* Maximum number of NGD devices */
#define MAX_NGD	25 		/* should be more than enough for now */

static d_close_t ngdclose;
static d_open_t ngdopen;
static d_read_t ngdread;
static d_write_t ngdwrite;
static d_ioctl_t ngdioctl;
static d_poll_t ngdpoll;

#define NGD_CDEV_MAJOR 20
static struct cdevsw ngd_cdevsw = {
        /* open */      ngdopen,
        /* close */     ngdclose,
        /* read */      ngdread,
        /* write */     ngdwrite,
        /* ioctl */     ngdioctl,
        /* poll */      ngdpoll,
        /* mmap */      nommap,
        /* strategy */  nostrategy,
        /* name */      "ngd",
        /* maj */       NGD_CDEV_MAJOR,
        /* dump */      nodump,
        /* psize */     nopsize,
        /* flags */     0,
};

/* 
 * this holds all the stuff that should be done at load time 
 */
static int
ng_device_mod_event(module_t mod, int event, void *data)
{
	int error = 0;

#ifdef NGD_DEBUG
	printf("%s()\n",__func__);
#endif /* NGD_DEBUG */

	switch (event) {
		case MOD_LOAD:

			ng_device_init();
			break;

		case MOD_UNLOAD:
			/* XXX do we need to do something specific ? */
			/* ng_device_breakdown */
			break;
		
		default:
			error = EOPNOTSUPP;
			break;
	}

	return(error);
}


static int
ng_device_init()
{
        struct ngd_softc *sc = &ngd_softc;
	
#ifdef NGD_DEBUG
	printf("%s()\n",__func__);
#endif /* NGD_DEBUG */ 

	SLIST_INIT(&sc->head);

        if (ng_make_node_common(&typestruct, &sc->node) != 0) {
                printf("%s(): ng_make_node_common failed\n",__func__);
                return(ENXIO);
        }
        sprintf(sc->nodename, "%s", NG_DEVICE_NODE_TYPE);
        if (ng_name_node(sc->node, sc->nodename)) {
                NG_NODE_UNREF(sc->node); /* make it go away again */
                printf("%s(): ng_name_node failed\n",__func__);
                return(ENXIO);
        }
        NG_NODE_SET_PRIVATE(sc->node, sc);

	return(0);
}

/* 
 * don't allow to be created, only the device can do that 
 */
static int
ng_device_cons(node_p node)
{

#ifdef NGD_DEBUG
	printf("%s()\n",__func__);
#endif /* NGD_DEBUG */
	
	return(EINVAL);
}

/*
 * Receive control message. We just bounce it back as a reply.
 */
static int
ng_device_rcvmsg(node_p node, item_p item, hook_p lasthook)
{
        struct ngd_softc *sc = &ngd_softc;
	struct ng_mesg *msg;
	int error = 0;
	struct ngd_connection * connection = NULL;
	struct ngd_connection *tmp = NULL;

#ifdef NGD_DEBUG
	printf("%s()\n",__func__);
#endif /* NGD_DEBUG */

	NGI_GET_MSG(item, msg); 

	SLIST_FOREACH(tmp,&sc->head,links) {
		if(tmp->active_hook == lasthook) {
			connection = tmp;
		}
	}
	if(connection == NULL) {
		printf("%s(): connection is still NULL, no hook found\n",__func__);
		return(-1);
	}

	return(error);
}

static int
get_free_unit()
{
	struct ngd_connection *tmp = NULL;
	struct ngd_softc *sc = &ngd_softc;
	int n = 0;
	int unit = -1;

#ifdef NGD_DEBUG
	printf("%s()\n",__func__);
#endif /* NGD_DEBUG */

	/* When there is no list yet, the first device unit is always 0. */
	if SLIST_EMPTY(&sc->head) {
		unit = 0;
		return(unit);
	}

	/* Just do a brute force loop to find the first free unit that is
	 * smaller than MAX_NGD.
	 * Set MAX_NGD to a large value, doesn't impact performance.
	 */
	for(n = 0;n<MAX_NGD && unit == -1;n++) {
		SLIST_FOREACH(tmp,&sc->head,links) {

			if(tmp->unit == n) {
				unit = -1;
				break;
			}
			unit = n;
		}
	}

	return(unit);
}

/*
 * incoming hook
 */
static int
ng_device_newhook(node_p node, hook_p hook, const char *name)
{
	struct ngd_softc *sc = &ngd_softc;
	struct ngd_connection * new_connection = NULL;

#ifdef NGD_DEBUG
	printf("%s()\n",__func__);
#endif /* NGD_DEBUG */

	new_connection = malloc(sizeof(struct ngd_connection), M_DEVBUF, M_NOWAIT);
	if(new_connection == NULL) {
		printf("%s(): ERROR: new_connection == NULL\n",__func__);
		return(-1);
	}

	new_connection->unit = get_free_unit();
	if(new_connection->unit<0) {
		printf("%s: No free unit found by get_free_unit(), "
				"increas MAX_NGD\n",__func__);
		return(-1);
	}
	new_connection->ngddev = make_dev(&ngd_cdevsw, new_connection->unit, 0, 0,0600,"ngd%d",new_connection->unit);
	if(new_connection->ngddev == NULL) {
		printf("%s(): make_dev failed\n",__func__);
		return(-1);
	}

	new_connection->readq = malloc(sizeof(char)*NGD_QUEUE_SIZE, M_DEVBUF, M_NOWAIT | M_ZERO);
	if(new_connection->readq == NULL) {
		printf("%s(): readq malloc failed\n",__func__);
		return(-1);
	}

	/* point to begin of buffer */
	new_connection->loc = 0;
	new_connection->active_hook = hook;

	SLIST_INSERT_HEAD(&sc->head, new_connection, links);

	return(0);
}

/*
 * we gave ok to a new hook
 * now connect
 */
static int
ng_device_connect(hook_p hook)
{

#ifdef NGD_DEBUG
	printf("%s()\n",__func__);
#endif /* NGD_DEBUG */

	return(0);
}


/*
 * Receive data from hook
 */
static int
ng_device_rcvdata(hook_p hook, item_p item)
{
	struct mbuf *m;
	struct ngd_softc *sc = &ngd_softc;
	struct ngd_connection * connection = NULL;
	struct ngd_connection * tmp;
	char *buffer;

#ifdef NGD_DEBUG
	printf("%s()\n",__func__);
#endif /* NGD_DEBUG */

	SLIST_FOREACH(tmp,&sc->head,links) {
		if(tmp->active_hook == hook) {
			connection = tmp;
		}
	}
	if(connection == NULL) {
		printf("%s(): connection is still NULL, no hook found\n",__func__);
		return(-1);
	}

	NGI_GET_M(item, m);
	NG_FREE_ITEM(item);

	m = m_pullup(m,m->m_len);
	if(m == NULL) {
		printf("%s(): ERROR: m_pullup failed\n",__func__);
		return(-1);
	}

	buffer = malloc(sizeof(char)*m->m_len, M_DEVBUF, M_NOWAIT | M_ZERO);
	if(buffer == NULL) {
		printf("%s(): ERROR: buffer malloc failed\n",__func__);
		return(-1);
	}

	buffer = mtod(m,char *);

	if( (connection->loc+m->m_len) < NGD_QUEUE_SIZE) {
	        memcpy(connection->readq+connection->loc, buffer, m->m_len);
		connection->loc += m->m_len;
	} else
		printf("%s(): queue full, first read out a bit\n",__func__);

	free(buffer,M_DEVBUF);

	return(0);
}

/*
 * Removal of the last link destroys the node
 */
static int
ng_device_disconnect(hook_p hook)
{
	struct ngd_softc *sc = &ngd_softc;
	struct ngd_connection * connection = NULL;
	struct ngd_connection * tmp;

#ifdef NGD_DEBUG
	printf("%s()\n",__func__);
#endif /* NGD_DEBUG */

	SLIST_FOREACH(tmp,&sc->head,links) {
		if(tmp->active_hook == hook) {
			connection = tmp;
		}
	}
	if(connection == NULL) {
		printf("%s(): connection is still NULL, no hook found\n",__func__);
		return(-1);
	}

        free(connection->readq,M_DEVBUF);

	destroy_dev(connection->ngddev);

	SLIST_REMOVE(&sc->head,connection,ngd_connection,links);

	return(0);
}
/*
 * the device is opened 
 */
static int
ngdopen(dev_t dev, int flag, int mode, struct thread *td)
{

#ifdef NGD_DEBUG
	printf("%s()\n",__func__);
#endif /* NGD_DEBUG */

	return(0);
}

/*
 * the device is closed 
 */
static int
ngdclose(dev_t dev, int flag, int mode, struct thread *td)
{

#ifdef NGD_DEBUG
	printf("%s()\n",__func__);
#endif

	return(0);
}


/*
 * process ioctl
 *
 * they are translated into netgraph messages and passed on
 * 
 */
static int
ngdioctl(dev_t dev, u_long cmd, caddr_t addr, int flag, struct thread *td)
{
	struct ngd_softc *sc = &ngd_softc;
	struct ngd_connection * connection = NULL;
	struct ngd_connection * tmp;
	int error = 0;
	struct ng_mesg *msg;
        struct ngd_param_s * datap;

#ifdef NGD_DEBUG
	printf("%s()\n",__func__);
#endif /* NGD_DEBUG */

	SLIST_FOREACH(tmp,&sc->head,links) {
		if(tmp->ngddev == dev) {
			connection = tmp;
		}
	}
	if(connection == NULL) {
		printf("%s(): connection is still NULL, no dev found\n",__func__);
		return(-1);
	}

	/* NG_MKMESSAGE(msg, cookie, cmdid, len, how) */
	NG_MKMESSAGE(msg, NGM_DEVICE_COOKIE, cmd, sizeof(struct ngd_param_s), 
			M_NOWAIT);
	if (msg == NULL) {
		printf("%s(): msg == NULL\n",__func__);
		goto nomsg;
	}

	/* pass the ioctl data into the ->data area */
	datap = (struct ngd_param_s *)msg->data;
        datap->p = addr;

	/* NG_SEND_MSG_HOOK(error, here, msg, hook, retaddr) */
	NG_SEND_MSG_HOOK(error, sc->node, msg, connection->active_hook, NULL);
	if(error)
		printf("%s(): NG_SEND_MSG_HOOK error: %d\n",__func__,error);

nomsg:

	return(0);
}


/*
 * This function is called when a read(2) is done to our device.
 * We pass the data available in kernelspace on into userland using
 * uiomove.
 */
static int
ngdread(dev_t dev, struct uio *uio, int flag)
{
	int ret = 0, amnt;
	char buffer[uio->uio_resid+1];
	struct ngd_softc *sc = &ngd_softc;
	struct ngd_connection * connection = NULL;
	struct ngd_connection * tmp;

#ifdef NGD_DEBUG
	printf("%s()\n",__func__);
#endif /* NGD_DEBUG */

	SLIST_FOREACH(tmp,&sc->head,links) {
		if(tmp->ngddev == dev) {
			connection = tmp;
		}
	}
	if(connection == NULL) {
		printf("%s(): connection is still NULL, no dev found\n",__func__);
		return(-1);
	}

	while ( ( uio->uio_resid > 0 ) && ( connection->loc > 0 ) ) {
		amnt = MIN(uio->uio_resid,connection->loc);

		memcpy(buffer,connection->readq, amnt);
		memcpy(connection->readq, connection->readq+amnt,
				connection->loc-amnt);
		connection->loc -= amnt;

		ret = uiomove((caddr_t)buffer, amnt, uio);
		if(ret != 0)
			goto error;

	}
	return(0);

error:
	printf("%s(): uiomove returns error %d\n",__func__,ret);
	/* do error cleanup here */
	return(ret);
}


/* 
 * This function is called when our device is written to.
 * We read the data from userland into our local buffer and pass it on
 * into the remote hook.
 *
 */
static int
ngdwrite(dev_t dev, struct uio *uio, int flag)
{
	int ret;
	int error = 0;
	struct mbuf *m;
	char buffer[uio->uio_resid];
	int len = uio->uio_resid;
	struct ngd_softc *sc =& ngd_softc;
	struct ngd_connection * connection = NULL;
	struct ngd_connection * tmp;

#ifdef NGD_DEBUG
	printf("%s()\n",__func__);
#endif /* NGD_DEBUG */

	SLIST_FOREACH(tmp,&sc->head,links) {
		if(tmp->ngddev == dev) {
			connection = tmp;
		}
	}

	if(connection == NULL) {
		printf("%s(): connection is still NULL, no dev found\n",__func__);
		return(-1);
	}

	if (len > 0) {
		if ((ret = uiomove((caddr_t)buffer, len, uio)) != 0)
			goto error;
	} else
		printf("%s(): len <= 0 : is this supposed to happen?!\n",__func__);

	m = m_devget(buffer,len,0,NULL,NULL);

	NG_SEND_DATA_ONLY(error,connection->active_hook,m);

	return(0);

error:
	/* do error cleanup here */
	printf("%s(): uiomove returned err: %d\n",__func__,ret);

	return(ret);
}

/*
 * we are being polled/selected
 * check if there is data available for read
 */
static int
ngdpoll(dev_t dev, int events, struct thread *td)
{
	int revents = 0;
	struct ngd_softc *sc = &ngd_softc;
	struct ngd_connection * connection = NULL;
	struct ngd_connection * tmp;


	if (events & (POLLIN | POLLRDNORM)) {
		/* get the connection we have to know the loc from */
		SLIST_FOREACH(tmp,&sc->head,links) {
			if(tmp->ngddev == dev) {
				connection = tmp;
			}
		}
		if(connection == NULL) {
			printf("%s(): ERROR: connection is still NULL,"
				"no dev found\n",__func__);
			return(-1);
		}

		if (connection->loc > 0)
			revents |= events & (POLLIN | POLLRDNORM);
	}

	return(revents);
}
