#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2025 FreeBSD Foundation
#
# This sofware was developed by Cy Schubert <cy@FreeBSD.org>
# under sponsorship from the FreeBSD Foundation.
#

.PATH:	${KRB5_DIR}/lib/krb5/os

SRCS+=	accessor.c \
	addr.c \
	c_ustime.c \
	ccdefname.c \
	changepw.c \
	dnsglue.c \
	dnssrv.c \
	expand_path.c \
	full_ipadr.c \
	gen_port.c \
	gen_rname.c \
	genaddrs.c \
	hostaddr.c \
	hostrealm.c \
	hostrealm_dns.c \
	hostrealm_domain.c \
	hostrealm_profile.c \
	hostrealm_registry.c \
	init_os_ctx.c \
	krbfileio.c \
	ktdefname.c \
	localaddr.c \
	localauth.c \
	localauth_an2ln.c \
	localauth_k5login.c \
	localauth_names.c \
	localauth_rule.c \
	locate_kdc.c \
	lock_file.c \
	mk_faddr.c \
	net_read.c \
	net_write.c \
	port2ip.c \
	prompter.c \
	read_msg.c \
	read_pwd.c \
	realm_dom.c \
	sendto_kdc.c \
	sn2princ.c \
	thread_safe.c \
	timeofday.c \
	toffset.c \
	trace.c \
	unlck_file.c \
	ustime.c \
	write_msg.c
