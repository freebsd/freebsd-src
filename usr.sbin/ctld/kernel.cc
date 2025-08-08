/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2003, 2004 Silicon Graphics International Corp.
 * Copyright (c) 1997-2007 Kenneth D. Merry
 * Copyright (c) 2012 The FreeBSD Foundation
 * Copyright (c) 2017 Jakub Wojciech Klama <jceel@FreeBSD.org>
 * All rights reserved.
 *
 * Portions of this software were developed by Edward Tomasz Napierala
 * under sponsorship from the FreeBSD Foundation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    substantially similar to the "NO WARRANTY" disclaimer below
 *    ("Disclaimer") and any redistribution must be conditioned upon
 *    including a substantially similar Disclaimer requirement for further
 *    binary redistribution.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGES.
 *
 */

#include <sys/param.h>
#include <sys/capsicum.h>
#include <sys/callout.h>
#include <sys/cnv.h>
#include <sys/ioctl.h>
#include <sys/linker.h>
#include <sys/module.h>
#include <sys/queue.h>
#include <sys/sbuf.h>
#include <sys/stat.h>
#include <assert.h>
#include <bsdxml.h>
#include <capsicum_helpers.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <cam/scsi/scsi_all.h>
#include <cam/scsi/scsi_message.h>
#include <cam/ctl/ctl.h>
#include <cam/ctl/ctl_io.h>
#include <cam/ctl/ctl_backend.h>
#include <cam/ctl/ctl_ioctl.h>
#include <cam/ctl/ctl_util.h>
#include <cam/ctl/ctl_scsi_all.h>

#include "ctld.hh"

#ifdef ICL_KERNEL_PROXY
#include <netdb.h>
#endif

#define	NVLIST_BUFSIZE	1024

int	ctl_fd = 0;

void
kernel_init(void)
{
	int retval, saved_errno;

	ctl_fd = open(CTL_DEFAULT_DEV, O_RDWR);
	if (ctl_fd < 0 && errno == ENOENT) {
		saved_errno = errno;
		retval = kldload("ctl");
		if (retval != -1)
			ctl_fd = open(CTL_DEFAULT_DEV, O_RDWR);
		else
			errno = saved_errno;
	}
	if (ctl_fd < 0)
		log_err(1, "failed to open %s", CTL_DEFAULT_DEV);
#ifdef	WANT_ISCSI
	else {
		saved_errno = errno;
		if (modfind("cfiscsi") == -1 && kldload("cfiscsi") == -1)
			log_warn("couldn't load cfiscsi");
		errno = saved_errno;
	}
#endif
}

/*
 * Backend LUN information.
 */
using attr_list_t = std::list<std::pair<std::string, std::string>>;

struct cctl_lun {
	uint64_t lun_id;
	std::string backend_type;
	uint8_t device_type;
	uint64_t size_blocks;
	uint32_t blocksize;
	std::string serial_number;
	std::string device_id;
	std::string ctld_name;
	attr_list_t attr_list;
};

struct cctl_port {
	uint32_t port_id;
	std::string port_frontend;
	std::string port_name;
	int pp;
	int vp;
	uint16_t portid;
	int cfiscsi_state;
	std::string cfiscsi_target;
	std::string nqn;
	uint16_t cfiscsi_portal_group_tag;
	std::string ctld_portal_group_name;
	std::string ctld_transport_group_name;
	attr_list_t attr_list;
};

struct cctl_devlist_data {
	std::list<cctl_lun> lun_list;
	struct cctl_lun *cur_lun = nullptr;
	std::list<cctl_port> port_list;
	struct cctl_port *cur_port = nullptr;
	u_int level = 0;
	struct sbuf *cur_sb[32] = {};
};

static void
cctl_start_element(void *user_data, const char *name, const char **attr)
{
	int i;
	struct cctl_devlist_data *devlist;
	struct cctl_lun *cur_lun;

	devlist = (struct cctl_devlist_data *)user_data;
	cur_lun = devlist->cur_lun;
	devlist->level++;
	if (devlist->level >= nitems(devlist->cur_sb))
		log_errx(1, "%s: too many nesting levels, %zu max", __func__,
		     nitems(devlist->cur_sb));

	devlist->cur_sb[devlist->level] = sbuf_new_auto();
	if (devlist->cur_sb[devlist->level] == NULL)
		log_err(1, "%s: unable to allocate sbuf", __func__);

	if (strcmp(name, "lun") == 0) {
		if (cur_lun != NULL)
			log_errx(1, "%s: improper lun element nesting",
			    __func__);

		devlist->lun_list.emplace_back();
		cur_lun = &devlist->lun_list.back();

		devlist->cur_lun = cur_lun;

		for (i = 0; attr[i] != NULL; i += 2) {
			if (strcmp(attr[i], "id") == 0) {
				cur_lun->lun_id = strtoull(attr[i+1], NULL, 0);
			} else {
				log_errx(1, "%s: invalid LUN attribute %s = %s",
				     __func__, attr[i], attr[i+1]);
			}
		}
	}
}

static void
cctl_end_element(void *user_data, const char *name)
{
	struct cctl_devlist_data *devlist;
	struct cctl_lun *cur_lun;
	std::string str;

	devlist = (struct cctl_devlist_data *)user_data;
	cur_lun = devlist->cur_lun;

	if ((cur_lun == NULL)
	 && (strcmp(name, "ctllunlist") != 0))
		log_errx(1, "%s: cur_lun == NULL! (name = %s)", __func__, name);

	if (devlist->cur_sb[devlist->level] == NULL)
		log_errx(1, "%s: no valid sbuf at level %d (name %s)", __func__,
		     devlist->level, name);

	sbuf_finish(devlist->cur_sb[devlist->level]);
	str = sbuf_data(devlist->cur_sb[devlist->level]);

	sbuf_delete(devlist->cur_sb[devlist->level]);
	devlist->cur_sb[devlist->level] = NULL;
	devlist->level--;

	if (strcmp(name, "backend_type") == 0) {
		cur_lun->backend_type = std::move(str);
	} else if (strcmp(name, "lun_type") == 0) {
		if (str.empty())
			log_errx(1, "%s: %s missing its argument", __func__, name);
		cur_lun->device_type = strtoull(str.c_str(), NULL, 0);
	} else if (strcmp(name, "size") == 0) {
		if (str.empty())
			log_errx(1, "%s: %s missing its argument", __func__, name);
		cur_lun->size_blocks = strtoull(str.c_str(), NULL, 0);
	} else if (strcmp(name, "blocksize") == 0) {
		if (str.empty())
			log_errx(1, "%s: %s missing its argument", __func__, name);
		cur_lun->blocksize = strtoul(str.c_str(), NULL, 0);
	} else if (strcmp(name, "serial_number") == 0) {
		cur_lun->serial_number = std::move(str);
	} else if (strcmp(name, "device_id") == 0) {
		cur_lun->device_id = std::move(str);
	} else if (strcmp(name, "ctld_name") == 0) {
		cur_lun->ctld_name = std::move(str);
	} else if (strcmp(name, "lun") == 0) {
		devlist->cur_lun = NULL;
	} else if (strcmp(name, "ctllunlist") == 0) {
		/* Nothing. */
	} else {
		cur_lun->attr_list.emplace_back(name, std::move(str));
	}
}

static void
cctl_start_pelement(void *user_data, const char *name, const char **attr)
{
	int i;
	struct cctl_devlist_data *devlist;
	struct cctl_port *cur_port;

	devlist = (struct cctl_devlist_data *)user_data;
	cur_port = devlist->cur_port;
	devlist->level++;
	if (devlist->level >= nitems(devlist->cur_sb))
		log_errx(1, "%s: too many nesting levels, %zu max", __func__,
		     nitems(devlist->cur_sb));

	devlist->cur_sb[devlist->level] = sbuf_new_auto();
	if (devlist->cur_sb[devlist->level] == NULL)
		log_err(1, "%s: unable to allocate sbuf", __func__);

	if (strcmp(name, "targ_port") == 0) {
		if (cur_port != NULL)
			log_errx(1, "%s: improper port element nesting (%s)",
			    __func__, name);

		devlist->port_list.emplace_back();
		cur_port = &devlist->port_list.back();
		devlist->cur_port = cur_port;

		for (i = 0; attr[i] != NULL; i += 2) {
			if (strcmp(attr[i], "id") == 0) {
				cur_port->port_id = strtoul(attr[i+1], NULL, 0);
			} else {
				log_errx(1, "%s: invalid LUN attribute %s = %s",
				     __func__, attr[i], attr[i+1]);
			}
		}
	}
}

static void
cctl_end_pelement(void *user_data, const char *name)
{
	struct cctl_devlist_data *devlist;
	struct cctl_port *cur_port;
	std::string str;

	devlist = (struct cctl_devlist_data *)user_data;
	cur_port = devlist->cur_port;

	if ((cur_port == NULL)
	 && (strcmp(name, "ctlportlist") != 0))
		log_errx(1, "%s: cur_port == NULL! (name = %s)", __func__, name);

	if (devlist->cur_sb[devlist->level] == NULL)
		log_errx(1, "%s: no valid sbuf at level %d (name %s)", __func__,
		     devlist->level, name);

	sbuf_finish(devlist->cur_sb[devlist->level]);
	str = sbuf_data(devlist->cur_sb[devlist->level]);

	sbuf_delete(devlist->cur_sb[devlist->level]);
	devlist->cur_sb[devlist->level] = NULL;
	devlist->level--;

	if (strcmp(name, "frontend_type") == 0) {
		cur_port->port_frontend = std::move(str);
	} else if (strcmp(name, "port_name") == 0) {
		cur_port->port_name = std::move(str);
	} else if (strcmp(name, "physical_port") == 0) {
		if (str.empty())
			log_errx(1, "%s: %s missing its argument", __func__, name);
		cur_port->pp = strtoul(str.c_str(), NULL, 0);
	} else if (strcmp(name, "virtual_port") == 0) {
		if (str.empty())
			log_errx(1, "%s: %s missing its argument", __func__, name);
		cur_port->vp = strtoul(str.c_str(), NULL, 0);
	} else if (strcmp(name, "cfiscsi_target") == 0) {
		cur_port->cfiscsi_target = std::move(str);
	} else if (strcmp(name, "cfiscsi_state") == 0) {
		if (str.empty())
			log_errx(1, "%s: %s missing its argument", __func__, name);
		cur_port->cfiscsi_state = strtoul(str.c_str(), NULL, 0);
	} else if (strcmp(name, "cfiscsi_portal_group_tag") == 0) {
		if (str.empty())
			log_errx(1, "%s: %s missing its argument", __func__, name);
		cur_port->cfiscsi_portal_group_tag = strtoul(str.c_str(), NULL, 0);
	} else if (strcmp(name, "ctld_portal_group_name") == 0) {
		cur_port->ctld_portal_group_name = std::move(str);
	} else if (strcmp(name, "ctld_transport_group_name") == 0) {
		cur_port->ctld_transport_group_name = std::move(str);
	} else if (strcmp(name, "nqn") == 0) {
		cur_port->nqn = std::move(str);
	} else if (strcmp(name, "portid") == 0) {
		if (str.empty())
			log_errx(1, "%s: %s missing its argument", __func__, name);
		cur_port->portid = strtoul(str.c_str(), NULL, 0);
	} else if (strcmp(name, "targ_port") == 0) {
		devlist->cur_port = NULL;
	} else if (strcmp(name, "ctlportlist") == 0) {
		/* Nothing. */
	} else {
		cur_port->attr_list.emplace_back(name, std::move(str));
	}
}

static void
cctl_char_handler(void *user_data, const XML_Char *str, int len)
{
	struct cctl_devlist_data *devlist;

	devlist = (struct cctl_devlist_data *)user_data;

	sbuf_bcat(devlist->cur_sb[devlist->level], str, len);
}

static bool
parse_kernel_config(struct cctl_devlist_data &devlist)
{
	struct ctl_lun_list list;
	XML_Parser parser;
	int retval;

	std::vector<char> buf(4096);
retry:
	bzero(&list, sizeof(list));
	list.alloc_len = buf.size();
	list.status = CTL_LUN_LIST_NONE;
	list.lun_xml = buf.data();

	if (ioctl(ctl_fd, CTL_LUN_LIST, &list) == -1) {
		log_warn("error issuing CTL_LUN_LIST ioctl");
		return (false);
	}

	if (list.status == CTL_LUN_LIST_ERROR) {
		log_warnx("error returned from CTL_LUN_LIST ioctl: %s",
		    list.error_str);
		return (false);
	}

	if (list.status == CTL_LUN_LIST_NEED_MORE_SPACE) {
		buf.resize(buf.size() << 1);
		goto retry;
	}

	parser = XML_ParserCreate(NULL);
	if (parser == NULL) {
		log_warnx("unable to create XML parser");
		return (false);
	}

	XML_SetUserData(parser, &devlist);
	XML_SetElementHandler(parser, cctl_start_element, cctl_end_element);
	XML_SetCharacterDataHandler(parser, cctl_char_handler);

	retval = XML_Parse(parser, buf.data(), strlen(buf.data()), 1);
	XML_ParserFree(parser);
	if (retval != 1) {
		log_warnx("XML_Parse failed");
		return (false);
	}

retry_port:
	bzero(&list, sizeof(list));
	list.alloc_len = buf.size();
	list.status = CTL_LUN_LIST_NONE;
	list.lun_xml = buf.data();

	if (ioctl(ctl_fd, CTL_PORT_LIST, &list) == -1) {
		log_warn("error issuing CTL_PORT_LIST ioctl");
		return (false);
	}

	if (list.status == CTL_LUN_LIST_ERROR) {
		log_warnx("error returned from CTL_PORT_LIST ioctl: %s",
		    list.error_str);
		return (false);
	}

	if (list.status == CTL_LUN_LIST_NEED_MORE_SPACE) {
		buf.resize(buf.size() << 1);
		goto retry_port;
	}

	parser = XML_ParserCreate(NULL);
	if (parser == NULL) {
		log_warnx("unable to create XML parser");
		return (false);
	}

	XML_SetUserData(parser, &devlist);
	XML_SetElementHandler(parser, cctl_start_pelement, cctl_end_pelement);
	XML_SetCharacterDataHandler(parser, cctl_char_handler);

	retval = XML_Parse(parser, buf.data(), strlen(buf.data()), 1);
	XML_ParserFree(parser);
	if (retval != 1) {
		log_warnx("XML_Parse failed");
		return (false);
	}

	return (true);
}

void
add_iscsi_port(struct kports &kports, struct conf *conf,
    const struct cctl_port &port, std::string &name)
{
	if (port.cfiscsi_target.empty()) {
		log_debugx("CTL port %u \"%s\" wasn't managed by ctld; ",
		    port.port_id, name.c_str());
		if (!kports.has_port(name)) {
			if (!kports.add_port(name, port.port_id)) {
				log_warnx("kports::add_port failed");
				return;
			}
		}
		return;
	}
	if (port.cfiscsi_state != 1) {
		log_debugx("CTL port %ju is not active (%d); ignoring",
		    (uintmax_t)port.port_id, port.cfiscsi_state);
		return;
	}

	const char *t_name = port.cfiscsi_target.c_str();
	struct target *targ = conf->find_target(t_name);
	if (targ == nullptr) {
		targ = conf->add_target(t_name);
		if (targ == nullptr) {
			log_warnx("Failed to add target \"%s\"", t_name);
			return;
		}
	}

	if (port.ctld_portal_group_name.empty())
		return;

	const char *pg_name = port.ctld_portal_group_name.c_str();
	struct portal_group *pg = conf->find_portal_group(pg_name);
	if (pg == nullptr) {
		pg = conf->add_portal_group(pg_name);
		if (pg == nullptr) {
			log_warnx("Failed to add portal-group \"%s\"", pg_name);
			return;
		}
	}
	pg->set_tag(port.cfiscsi_portal_group_tag);
	if (!conf->add_port(targ, pg, port.port_id)) {
		log_warnx("Failed to add port for target \"%s\" and portal-group \"%s\"",
		    t_name, pg_name);
	}
}

void
add_nvmf_port(struct conf *conf, const struct cctl_port &port,
    std::string &name)
{
	if (port.nqn.empty() || port.ctld_transport_group_name.empty()) {
		log_debugx("CTL port %u \"%s\" wasn't managed by ctld; ",
		    port.port_id, name.c_str());
		return;
	}

	const char *nqn = port.nqn.c_str();
	struct target *targ = conf->find_controller(nqn);
	if (targ == nullptr) {
		targ = conf->add_controller(nqn);
		if (targ == nullptr) {
			log_warnx("Failed to add controller \"%s\"", nqn);
			return;
		}
	}

	const char *tg_name = port.ctld_transport_group_name.c_str();
	struct portal_group *pg = conf->find_transport_group(tg_name);
	if (pg == nullptr) {
		pg = conf->add_transport_group(tg_name);
		if (pg == nullptr) {
			log_warnx("Failed to add transport-group \"%s\"",
			    tg_name);
			return;
		}
	}
	pg->set_tag(port.portid);
	if (!conf->add_port(targ, pg, port.port_id)) {
		log_warnx("Failed to add port for controller \"%s\" and transport-group \"%s\"",
		    nqn, tg_name);
	}
}

conf_up
conf_new_from_kernel(struct kports &kports)
{
	struct cctl_devlist_data devlist;

	log_debugx("obtaining previously configured CTL luns from the kernel");

	if (!parse_kernel_config(devlist))
		return {};

	conf_up conf = std::make_unique<struct conf>();

	for (const auto &port : devlist.port_list) {
		if (port.port_frontend == "ha")
			continue;

		std::string name = port.port_name;
		if (port.pp != 0) {
			name += "/" + std::to_string(port.pp);
			if (port.vp != 0)
				name += "/" + std::to_string(port.vp);
		}

		if (port.port_frontend == "iscsi") {
			add_iscsi_port(kports, conf.get(), port, name);
		} else if (port.port_frontend == "nvmf") {
			add_nvmf_port(conf.get(), port, name);
		} else {
			/* XXX: Treat all unknown ports as iSCSI? */
			add_iscsi_port(kports, conf.get(), port, name);
		}
	}

	for (const auto &lun : devlist.lun_list) {
		if (lun.ctld_name.empty()) {
			log_debugx("CTL lun %ju wasn't managed by ctld; "
			    "ignoring", (uintmax_t)lun.lun_id);
			continue;
		}

		const char *l_name = lun.ctld_name.c_str();
		struct lun *cl = conf->find_lun(l_name);
		if (cl != NULL) {
			log_warnx("found CTL lun %ju \"%s\", "
			    "also backed by CTL lun %d; ignoring",
			    (uintmax_t)lun.lun_id, l_name,
			    cl->ctl_lun());
			continue;
		}

		log_debugx("found CTL lun %ju \"%s\"",
		    (uintmax_t)lun.lun_id, l_name);

		cl = conf->add_lun(l_name);
		if (cl == NULL) {
			log_warnx("lun_new failed");
			continue;
		}
		cl->set_backend(lun.backend_type.c_str());
		cl->set_device_type(lun.device_type);
		cl->set_blocksize(lun.blocksize);
		cl->set_device_id(lun.device_id.c_str());
		cl->set_serial(lun.serial_number.c_str());
		cl->set_size(lun.size_blocks * lun.blocksize);
		cl->set_ctl_lun(lun.lun_id);

		for (const auto &pair : lun.attr_list) {
			const char *key = pair.first.c_str();
			const char *value = pair.second.c_str();
			if (pair.first == "file" || pair.first == "dev") {
				cl->set_path(value);
				continue;
			}
			if (!cl->add_option(key, value))
				log_warnx("unable to add CTL lun option "
				    "%s for CTL lun %ju \"%s\"",
				    key, (uintmax_t)lun.lun_id,
				    cl->name());
		}
	}

	return (conf);
}

bool
lun::kernel_add()
{
	struct ctl_lun_req req;
	int error;

	bzero(&req, sizeof(req));

	strlcpy(req.backend, l_backend.c_str(), sizeof(req.backend));
	req.reqtype = CTL_LUNREQ_CREATE;

	req.reqdata.create.blocksize_bytes = l_blocksize;

	if (l_size != 0)
		req.reqdata.create.lun_size_bytes = l_size;

	if (l_ctl_lun >= 0) {
		req.reqdata.create.req_lun_id = l_ctl_lun;
		req.reqdata.create.flags |= CTL_LUN_FLAG_ID_REQ;
	}

	req.reqdata.create.flags |= CTL_LUN_FLAG_DEV_TYPE;
	req.reqdata.create.device_type = l_device_type;

	if (!l_serial.empty()) {
		strncpy((char *)req.reqdata.create.serial_num, l_serial.c_str(),
			sizeof(req.reqdata.create.serial_num));
		req.reqdata.create.flags |= CTL_LUN_FLAG_SERIAL_NUM;
	}

	if (!l_device_id.empty()) {
		strncpy((char *)req.reqdata.create.device_id,
		    l_device_id.c_str(), sizeof(req.reqdata.create.device_id));
		req.reqdata.create.flags |= CTL_LUN_FLAG_DEVID;
	}

	freebsd::nvlist_up nvl = options();
	req.args = nvlist_pack(nvl.get(), &req.args_len);
	if (req.args == NULL) {
		log_warn("error packing nvlist");
		return (false);
	}

	error = ioctl(ctl_fd, CTL_LUN_REQ, &req);
	free(req.args);

	if (error != 0) {
		log_warn("error issuing CTL_LUN_REQ ioctl");
		return (false);
	}

	switch (req.status) {
	case CTL_LUN_ERROR:
		log_warnx("LUN creation error: %s", req.error_str);
		return (false);
	case CTL_LUN_WARNING:
		log_warnx("LUN creation warning: %s", req.error_str);
		break;
	case CTL_LUN_OK:
		break;
	default:
		log_warnx("unknown LUN creation status: %d",
		    req.status);
		return (false);
	}

	l_ctl_lun = req.reqdata.create.req_lun_id;
	return (true);
}

bool
lun::kernel_modify() const
{
	struct ctl_lun_req req;
	int error;

	bzero(&req, sizeof(req));

	strlcpy(req.backend, l_backend.c_str(), sizeof(req.backend));
	req.reqtype = CTL_LUNREQ_MODIFY;

	req.reqdata.modify.lun_id = l_ctl_lun;
	req.reqdata.modify.lun_size_bytes = l_size;

	freebsd::nvlist_up nvl = options();
	req.args = nvlist_pack(nvl.get(), &req.args_len);
	if (req.args == NULL) {
		log_warn("error packing nvlist");
		return (false);
	}

	error = ioctl(ctl_fd, CTL_LUN_REQ, &req);
	free(req.args);

	if (error != 0) {
		log_warn("error issuing CTL_LUN_REQ ioctl");
		return (false);
	}

	switch (req.status) {
	case CTL_LUN_ERROR:
		log_warnx("LUN modification error: %s", req.error_str);
		return (false);
	case CTL_LUN_WARNING:
		log_warnx("LUN modification warning: %s", req.error_str);
		break;
	case CTL_LUN_OK:
		break;
	default:
		log_warnx("unknown LUN modification status: %d",
		    req.status);
		return (false);
	}

	return (true);
}

bool
lun::kernel_remove() const
{
	struct ctl_lun_req req;

	bzero(&req, sizeof(req));

	strlcpy(req.backend, l_backend.c_str(), sizeof(req.backend));
	req.reqtype = CTL_LUNREQ_RM;

	req.reqdata.rm.lun_id = l_ctl_lun;

	if (ioctl(ctl_fd, CTL_LUN_REQ, &req) == -1) {
		log_warn("error issuing CTL_LUN_REQ ioctl");
		return (false);
	}

	switch (req.status) {
	case CTL_LUN_ERROR:
		log_warnx("LUN removal error: %s", req.error_str);
		return (false);
	case CTL_LUN_WARNING:
		log_warnx("LUN removal warning: %s", req.error_str);
		break;
	case CTL_LUN_OK:
		break;
	default:
		log_warnx("unknown LUN removal status: %d", req.status);
		return (false);
	}

	return (true);
}

bool
ctl_create_port(const char *driver, const nvlist_t *nvl, uint32_t *ctl_port)
{
	struct ctl_req req;
	char result_buf[NVLIST_BUFSIZE];
	int error;

	bzero(&req, sizeof(req));
	req.reqtype = CTL_REQ_CREATE;

	strlcpy(req.driver, driver, sizeof(req.driver));
	req.args = nvlist_pack(nvl, &req.args_len);
	if (req.args == NULL) {
		log_warn("error packing nvlist");
		return (false);
	}

	req.result = result_buf;
	req.result_len = sizeof(result_buf);
	error = ioctl(ctl_fd, CTL_PORT_REQ, &req);
	free(req.args);

	if (error != 0) {
		log_warn("error issuing CTL_PORT_REQ ioctl");
		return (false);
	}
	if (req.status == CTL_LUN_ERROR) {
		log_warnx("error returned from port creation request: %s",
		    req.error_str);
		return (false);
	}
	if (req.status != CTL_LUN_OK) {
		log_warnx("unknown port creation request status %d",
		    req.status);
		return (false);
	}

	freebsd::nvlist_up result_nvl(nvlist_unpack(result_buf, req.result_len,
	    0));
	if (result_nvl == NULL) {
		log_warnx("error unpacking result nvlist");
		return (false);
	}

	*ctl_port = nvlist_get_number(result_nvl.get(), "port_id");
	return (true);
}

bool
ioctl_port::kernel_create_port()
{
	freebsd::nvlist_up nvl(nvlist_create(0));
	nvlist_add_stringf(nvl.get(), "pp", "%d", p_ioctl_pp);
	nvlist_add_stringf(nvl.get(), "vp", "%d", p_ioctl_vp);

	return (ctl_create_port("ioctl", nvl.get(), &p_ctl_port));
}

bool
kernel_port::kernel_create_port()
{
	struct ctl_port_entry entry;
	struct target *targ = p_target;

	p_ctl_port = p_pport->ctl_port();

	if (strncmp(targ->name(), "naa.", 4) == 0 &&
	    strlen(targ->name()) == 20) {
		bzero(&entry, sizeof(entry));
		entry.port_type = CTL_PORT_NONE;
		entry.targ_port = p_ctl_port;
		entry.flags |= CTL_PORT_WWNN_VALID;
		entry.wwnn = strtoull(targ->name() + 4, NULL, 16);
		if (ioctl(ctl_fd, CTL_SET_PORT_WWNS, &entry) == -1)
			log_warn("CTL_SET_PORT_WWNS ioctl failed");
	}
	return (true);
}

bool
port::kernel_add()
{
	struct ctl_port_entry entry;
	struct ctl_lun_map lm;
	struct target *targ = p_target;
	int error, i;

	if (!kernel_create_port())
		return (false);

	/* Explicitly enable mapping to block any access except allowed. */
	lm.port = p_ctl_port;
	lm.plun = UINT32_MAX;
	lm.lun = 0;
	error = ioctl(ctl_fd, CTL_LUN_MAP, &lm);
	if (error != 0)
		log_warn("CTL_LUN_MAP ioctl failed");

	/* Map configured LUNs */
	for (i = 0; i < MAX_LUNS; i++) {
		if (targ->lun(i) == nullptr)
			continue;
		lm.port = p_ctl_port;
		lm.plun = i;
		lm.lun = targ->lun(i)->ctl_lun();
		error = ioctl(ctl_fd, CTL_LUN_MAP, &lm);
		if (error != 0)
			log_warn("CTL_LUN_MAP ioctl failed");
	}

	/* Enable port */
	bzero(&entry, sizeof(entry));
	entry.targ_port = p_ctl_port;
	error = ioctl(ctl_fd, CTL_ENABLE_PORT, &entry);
	if (error != 0) {
		log_warn("CTL_ENABLE_PORT ioctl failed");
		return (false);
	}

	return (true);
}

bool
port::kernel_update(const struct port *oport)
{
	struct ctl_lun_map lm;
	struct target *targ = p_target;
	struct target *otarg = oport->p_target;
	int error, i;
	uint32_t olun;

	p_ctl_port = oport->p_ctl_port;

	/* Map configured LUNs and unmap others */
	for (i = 0; i < MAX_LUNS; i++) {
		lm.port = p_ctl_port;
		lm.plun = i;
		if (targ->lun(i) == nullptr)
			lm.lun = UINT32_MAX;
		else
			lm.lun = targ->lun(i)->ctl_lun();
		if (otarg->lun(i) == nullptr)
			olun = UINT32_MAX;
		else
			olun = otarg->lun(i)->ctl_lun();
		if (lm.lun == olun)
			continue;
		error = ioctl(ctl_fd, CTL_LUN_MAP, &lm);
		if (error != 0)
			log_warn("CTL_LUN_MAP ioctl failed");
	}
	return (true);
}

bool
ctl_remove_port(const char *driver, nvlist_t *nvl)
{
	struct ctl_req req;
	int error;

	strlcpy(req.driver, driver, sizeof(req.driver));
	req.reqtype = CTL_REQ_REMOVE;
	req.args = nvlist_pack(nvl, &req.args_len);
	if (req.args == NULL) {
		log_warn("error packing nvlist");
		return (false);
	}

	error = ioctl(ctl_fd, CTL_PORT_REQ, &req);
	free(req.args);

	if (error != 0) {
		log_warn("error issuing CTL_PORT_REQ ioctl");
		return (false);
	}
	if (req.status == CTL_LUN_ERROR) {
		log_warnx("error returned from port removal request: %s",
		    req.error_str);
		return (false);
	}
	if (req.status != CTL_LUN_OK) {
		log_warnx("unknown port removal request status %d", req.status);
		return (false);
	}
	return (true);
}

bool
ioctl_port::kernel_remove_port()
{
	freebsd::nvlist_up nvl(nvlist_create(0));
	nvlist_add_stringf(nvl.get(), "port_id", "%d", p_ctl_port);

	return (ctl_remove_port("ioctl", nvl.get()));
}

bool
kernel_port::kernel_remove_port()
{
	struct ctl_lun_map lm;
	int error;

	/* Disable LUN mapping. */
	lm.port = p_ctl_port;
	lm.plun = UINT32_MAX;
	lm.lun = UINT32_MAX;
	error = ioctl(ctl_fd, CTL_LUN_MAP, &lm);
	if (error != 0)
		log_warn("CTL_LUN_MAP ioctl failed");
	return (true);
}

bool
port::kernel_remove()
{
	struct ctl_port_entry entry;
	int error;

	/* Disable port */
	bzero(&entry, sizeof(entry));
	entry.targ_port = p_ctl_port;
	error = ioctl(ctl_fd, CTL_DISABLE_PORT, &entry);
	if (error != 0) {
		log_warn("CTL_DISABLE_PORT ioctl failed");
		return (false);
	}

	return (kernel_remove_port());
}

#ifdef ICL_KERNEL_PROXY
void
kernel_listen(struct addrinfo *ai, bool iser, int portal_id)
{
	struct ctl_iscsi req;

	bzero(&req, sizeof(req));

	req.type = CTL_ISCSI_LISTEN;
	req.data.listen.iser = iser;
	req.data.listen.domain = ai->ai_family;
	req.data.listen.socktype = ai->ai_socktype;
	req.data.listen.protocol = ai->ai_protocol;
	req.data.listen.addr = ai->ai_addr;
	req.data.listen.addrlen = ai->ai_addrlen;
	req.data.listen.portal_id = portal_id;

	if (ioctl(ctl_fd, CTL_ISCSI, &req) == -1)
		log_err(1, "error issuing CTL_ISCSI ioctl");

	if (req.status != CTL_ISCSI_OK) {
		log_errx(1, "error returned from CTL iSCSI listen: %s",
		    req.error_str);
	}
}

void
kernel_accept(int *connection_id, int *portal_id,
    struct sockaddr *client_sa, socklen_t *client_salen)
{
	struct ctl_iscsi req;
	struct sockaddr_storage ss;

	bzero(&req, sizeof(req));

	req.type = CTL_ISCSI_ACCEPT;
	req.data.accept.initiator_addr = (struct sockaddr *)&ss;

	if (ioctl(ctl_fd, CTL_ISCSI, &req) == -1)
		log_err(1, "error issuing CTL_ISCSI ioctl");

	if (req.status != CTL_ISCSI_OK) {
		log_errx(1, "error returned from CTL iSCSI accept: %s",
		    req.error_str);
	}

	*connection_id = req.data.accept.connection_id;
	*portal_id = req.data.accept.portal_id;
	*client_salen = req.data.accept.initiator_addrlen;
	memcpy(client_sa, &ss, *client_salen);
}

void
kernel_send(struct pdu *pdu)
{
	struct ctl_iscsi req;

	bzero(&req, sizeof(req));

	req.type = CTL_ISCSI_SEND;
	req.data.send.connection_id = pdu->pdu_connection->conn_socket;
	req.data.send.bhs = pdu->pdu_bhs;
	req.data.send.data_segment_len = pdu->pdu_data_len;
	req.data.send.data_segment = pdu->pdu_data;

	if (ioctl(ctl_fd, CTL_ISCSI, &req) == -1) {
		log_err(1, "error issuing CTL_ISCSI ioctl; "
		    "dropping connection");
	}

	if (req.status != CTL_ISCSI_OK) {
		log_errx(1, "error returned from CTL iSCSI send: "
		    "%s; dropping connection", req.error_str);
	}
}

void
kernel_receive(struct pdu *pdu)
{
	struct connection *conn;
	struct ctl_iscsi req;

	conn = pdu->pdu_connection;
	pdu->pdu_data = malloc(conn->conn_max_recv_data_segment_length);
	if (pdu->pdu_data == NULL)
		log_err(1, "malloc");

	bzero(&req, sizeof(req));

	req.type = CTL_ISCSI_RECEIVE;
	req.data.receive.connection_id = conn->conn_socket;
	req.data.receive.bhs = pdu->pdu_bhs;
	req.data.receive.data_segment_len =
	    conn->conn_max_recv_data_segment_length;
	req.data.receive.data_segment = pdu->pdu_data;

	if (ioctl(ctl_fd, CTL_ISCSI, &req) == -1) {
		log_err(1, "error issuing CTL_ISCSI ioctl; "
		    "dropping connection");
	}

	if (req.status != CTL_ISCSI_OK) {
		log_errx(1, "error returned from CTL iSCSI receive: "
		    "%s; dropping connection", req.error_str);
	}

}

#endif /* ICL_KERNEL_PROXY */

/*
 * XXX: I CANT INTO LATIN
 */
void
kernel_capsicate(void)
{
	cap_rights_t rights;
	const unsigned long cmds[] = { CTL_ISCSI, CTL_NVMF };

	cap_rights_init(&rights, CAP_IOCTL);
	if (caph_rights_limit(ctl_fd, &rights) < 0)
		log_err(1, "cap_rights_limit");

	if (caph_ioctls_limit(ctl_fd, cmds, nitems(cmds)) < 0)
		log_err(1, "cap_ioctls_limit");

	if (caph_enter() < 0)
		log_err(1, "cap_enter");

	if (cap_sandboxed())
		log_debugx("Capsicum capability mode enabled");
	else
		log_warnx("Capsicum capability mode not supported");
}

