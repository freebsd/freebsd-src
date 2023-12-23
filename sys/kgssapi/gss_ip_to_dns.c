/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2008 Isilon Inc http://www.isilon.com/
 * Authors: Doug Rabson <dfr@rabson.org>
 * Developed with Red Inc: Alfred Perlstein <alfred@freebsd.org>
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
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/kobj.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>

#include <kgssapi/gssapi.h>
#include <kgssapi/gssapi_impl.h>

#include "gssd.h"

OM_uint32
gss_ip_to_dns(OM_uint32 *minor_status,
    char *ip_addr,
    char *dns_name)
{
	struct ip_to_dns_res res;
	struct ip_to_dns_args args;
	enum clnt_stat stat;
	CLIENT *cl;

	*minor_status = 0;
	cl = kgss_gssd_client();
	if (cl == NULL)
		return (GSS_S_FAILURE);

	args.ip_addr.ip_addr_len = strlen(ip_addr);
	args.ip_addr.ip_addr_val = mem_alloc(args.ip_addr.ip_addr_len);
	memcpy(args.ip_addr.ip_addr_val, ip_addr, args.ip_addr.ip_addr_len);

	bzero(&res, sizeof(res));
	stat = gssd_ip_to_dns_1(&args, &res, cl);
	CLNT_RELEASE(cl);
	if (stat != RPC_SUCCESS) {
		*minor_status = stat;
		return (GSS_S_FAILURE);
	}

	if (res.major_status != GSS_S_COMPLETE) {
		*minor_status = res.minor_status;
		return (res.major_status);
	}

	if (res.dns_name.dns_name_len == 0 ||
	    res.dns_name.dns_name_len >= NI_MAXHOST) {
		*minor_status = 0;
		return (GSS_S_FAILURE);
	}

	memcpy(dns_name, res.dns_name.dns_name_val, res.dns_name.dns_name_len);
	dns_name[res.dns_name.dns_name_len] = '\0';

	return (GSS_S_COMPLETE);
}
