/*
 * hci.c
 */

/*-
 * Copyright (c) 2009 Maksim Yevmenkin <m_evmenkin@yahoo.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <bluetooth.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static char * bt_dev2node (char const *devname, char *nodename, int nnlen);

int
bt_devinfo(struct bt_devinfo *di)
{
	union {
		struct ng_btsocket_hci_raw_node_state		r0;
		struct ng_btsocket_hci_raw_node_bdaddr		r1;
		struct ng_btsocket_hci_raw_node_features	r2;
		struct ng_btsocket_hci_raw_node_buffer		r3;
		struct ng_btsocket_hci_raw_node_stat		r4;
		struct ng_btsocket_hci_raw_node_link_policy_mask r5;
		struct ng_btsocket_hci_raw_node_packet_mask	r6;
		struct ng_btsocket_hci_raw_node_role_switch	r7;
		struct ng_btsocket_hci_raw_node_debug		r8;
	}						rp;
	struct sockaddr_hci				ha;
	int						s, rval;

	if (di == NULL) {
		errno = EINVAL;
		return (-1);
	}

	memset(&ha, 0, sizeof(ha));
	ha.hci_len = sizeof(ha);
	ha.hci_family = AF_BLUETOOTH;

	if (bt_aton(di->devname, &rp.r1.bdaddr)) {
		if (!bt_devname(ha.hci_node, &rp.r1.bdaddr))
			return (-1);
	} else if (bt_dev2node(di->devname, ha.hci_node,
					sizeof(ha.hci_node)) == NULL) {
		errno = ENXIO;
		return (-1);
	}

	s = socket(PF_BLUETOOTH, SOCK_RAW, BLUETOOTH_PROTO_HCI);
	if (s < 0)
		return (-1);

	rval = -1;

	if (bind(s, (struct sockaddr *) &ha, sizeof(ha)) < 0 ||
	    connect(s, (struct sockaddr *) &ha, sizeof(ha)) < 0)
		goto bad;
	strlcpy(di->devname, ha.hci_node, sizeof(di->devname));

	if (ioctl(s, SIOC_HCI_RAW_NODE_GET_STATE, &rp.r0, sizeof(rp.r0)) < 0)
		goto bad;
	di->state = rp.r0.state;

	if (ioctl(s, SIOC_HCI_RAW_NODE_GET_BDADDR, &rp.r1, sizeof(rp.r1)) < 0)
		goto bad;
	bdaddr_copy(&di->bdaddr, &rp.r1.bdaddr);
	
	if (ioctl(s, SIOC_HCI_RAW_NODE_GET_FEATURES, &rp.r2, sizeof(rp.r2)) < 0)
		goto bad;
	memcpy(di->features, rp.r2.features, sizeof(di->features));

	if (ioctl(s, SIOC_HCI_RAW_NODE_GET_BUFFER, &rp.r3, sizeof(rp.r3)) < 0)
		goto bad;
	di->cmd_free = rp.r3.buffer.cmd_free;
	di->sco_size = rp.r3.buffer.sco_size;
	di->sco_pkts = rp.r3.buffer.sco_pkts;
	di->sco_free = rp.r3.buffer.sco_free;
	di->acl_size = rp.r3.buffer.acl_size;
	di->acl_pkts = rp.r3.buffer.acl_pkts;
	di->acl_free = rp.r3.buffer.acl_free;

	if (ioctl(s, SIOC_HCI_RAW_NODE_GET_STAT, &rp.r4, sizeof(rp.r4)) < 0)
		goto bad;
	di->cmd_sent = rp.r4.stat.cmd_sent;
	di->evnt_recv = rp.r4.stat.evnt_recv;
	di->acl_recv = rp.r4.stat.acl_recv;
	di->acl_sent = rp.r4.stat.acl_sent;
	di->sco_recv = rp.r4.stat.sco_recv;
	di->sco_sent = rp.r4.stat.sco_sent;
	di->bytes_recv = rp.r4.stat.bytes_recv;
	di->bytes_sent = rp.r4.stat.bytes_sent;

	if (ioctl(s, SIOC_HCI_RAW_NODE_GET_LINK_POLICY_MASK,
			&rp.r5, sizeof(rp.r5)) < 0)
		goto bad;
	di->link_policy_info = rp.r5.policy_mask;

	if (ioctl(s, SIOC_HCI_RAW_NODE_GET_PACKET_MASK,
			&rp.r6, sizeof(rp.r6)) < 0)
		goto bad;
	di->packet_type_info = rp.r6.packet_mask;

	 if (ioctl(s, SIOC_HCI_RAW_NODE_GET_ROLE_SWITCH,
			&rp.r7, sizeof(rp.r7)) < 0)
		goto bad;
	di->role_switch_info = rp.r7.role_switch;

	if (ioctl(s, SIOC_HCI_RAW_NODE_GET_DEBUG, &rp.r8, sizeof(rp.r8)) < 0)
		goto bad;
	di->debug = rp.r8.debug;

	rval = 0;
bad:
	close(s);

	return (rval);
}

int
bt_devenum(bt_devenum_cb_t cb, void *arg)
{
	struct ng_btsocket_hci_raw_node_list_names	rp;
	struct bt_devinfo				di;
	struct sockaddr_hci				ha;
	int						s, i, count;

	rp.num_names = HCI_DEVMAX;
	rp.names = (struct nodeinfo *) calloc(rp.num_names,
						sizeof(struct nodeinfo));
	if (rp.names == NULL) {
		errno = ENOMEM;
		return (-1);
	}

	memset(&ha, 0, sizeof(ha));
	ha.hci_len = sizeof(ha);
	ha.hci_family = AF_BLUETOOTH;
	ha.hci_node[0] = 'x';

	s = socket(PF_BLUETOOTH, SOCK_RAW, BLUETOOTH_PROTO_HCI);
	if (s < 0) {
		free(rp.names);

		return (-1);
	}

	if (bind(s, (struct sockaddr *) &ha, sizeof(ha)) < 0 ||
	    connect(s, (struct sockaddr *) &ha, sizeof(ha)) < 0 ||
	    ioctl(s, SIOC_HCI_RAW_NODE_LIST_NAMES, &rp, sizeof(rp)) < 0) {
		close(s);
		free(rp.names);

		return (-1);
	}

	for (count = 0, i = 0; i < rp.num_names; i ++) {
		strlcpy(di.devname, rp.names[i].name, sizeof(di.devname));
		if (bt_devinfo(&di) < 0)
			continue;

		count ++;

		if (cb == NULL)
			continue;

		strlcpy(ha.hci_node, rp.names[i].name, sizeof(ha.hci_node));
		if (bind(s, (struct sockaddr *) &ha, sizeof(ha)) < 0 ||
		    connect(s, (struct sockaddr *) &ha, sizeof(ha)) < 0)
			continue;

		if ((*cb)(s, &di, arg) > 0)
			break;
	}

	close (s);
	free(rp.names);

	return (count);
}

static char *
bt_dev2node(char const *devname, char *nodename, int nnlen)
{
	static char const *	 bt_dev_prefix[] = {
		"btccc",	/* 3Com Bluetooth PC-CARD */
		"h4",		/* UART/serial Bluetooth devices */
		"ubt",		/* Bluetooth USB devices */
		NULL		/* should be last */
	};

	static char		_nodename[HCI_DEVNAME_SIZE];
	char const		**p;
	char			*ep;
	int			plen, unit;

	if (nodename == NULL) {
		nodename = _nodename;
		nnlen = HCI_DEVNAME_SIZE;
	}

	for (p = bt_dev_prefix; *p != NULL; p ++) {
		plen = strlen(*p);
		if (strncmp(devname, *p, plen) != 0)
			continue;

		unit = strtoul(devname + plen, &ep, 10);
		if (*ep != '\0' &&
		    strcmp(ep, "hci") != 0 &&
		    strcmp(ep, "l2cap") != 0)
			return (NULL);	/* can't make sense of device name */

		snprintf(nodename, nnlen, "%s%uhci", *p, unit);

		return (nodename);
	}

	return (NULL);
}

