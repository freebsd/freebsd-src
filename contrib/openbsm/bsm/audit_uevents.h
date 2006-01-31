/*
 * Copyright (c) 2004 Apple Computer, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * $P4: //depot/projects/trustedbsd/openbsm/bsm/audit_uevents.h#7 $
 */

#ifndef _BSM_AUDIT_UEVENTS_H_
#define	_BSM_AUDIT_UEVENTS_H_

/*-
 * User level audit event numbers
 *
 * Range of audit event numbers:
 * 0			Reserved, invalid
 * 1     - 2047		Reserved for kernel events
 * 2048  - 32767	Defined by BSM for user events
 * 32768 - 36864	Reserved for Mac OS-X applications
 * 36865 - 65535	Reserved for applications
 *
 */
#define	AUE_at_create		6144
#define	AUE_at_delete		6145
#define	AUE_at_perm		6146
#define	AUE_cron_invoke		6147
#define	AUE_crontab_create	6148
#define	AUE_crontab_delete	6149
#define	AUE_crontab_perm	6150
#define	AUE_inetd_connect	6151
#define	AUE_login		6152
#define	AUE_logout		6153
#define	AUE_telnet		6154
#define	AUE_rlogin		6155
#define	AUE_mountd_mount	6156
#define	AUE_mountd_umount	6157
#define	AUE_rshd		6158
#define	AUE_su			6159
#define	AUE_halt		6160
#define	AUE_reboot		6161
#define	AUE_rexecd		6162
#define	AUE_passwd		6163
#define	AUE_rexd		6164
#define	AUE_ftpd		6165
#define	AUE_init		6166
#define	AUE_uadmin		6167
#define	AUE_shutdown		6168
#define	AUE_poweroff		6169
#define	AUE_crontab_mod		6170
#define	AUE_audit_startup	6171
#define	AUE_audit_shutdown	6172
#define	AUE_allocate_succ	6200
#define	AUE_allocate_fail	6201
#define	AUE_deallocate_succ	6202
#define	AUE_deallocate_fail	6203
#define	AUE_listdevice_succ	6205
#define	AUE_listdevice_fail	6206
#define	AUE_create_user		6207
#define	AUE_modify_user		6208
#define	AUE_delete_user		6209
#define	AUE_disable_user	6210
#define	AUE_enable_user		6211
#define	AUE_sudo		6300
#define	AUE_modify_password	6501	/* Not assigned by Sun. */
#define	AUE_create_group	6511	/* Not assigned by Sun. */
#define	AUE_delete_group	6512	/* Not assigned by Sun. */
#define	AUE_modify_group	6513	/* Not assigned by Sun. */
#define	AUE_add_to_group	6514	/* Not assigned by Sun. */
#define	AUE_remove_from_group	6515	/* Not assigned by Sun. */
#define	AUE_revoke_obj		6521	/* Not assigned by Sun; not used. */
#define	AUE_lw_login		6600	/* Not assigned by Sun; tentative. */
#define	AUE_lw_logout		6601	/* Not assigned by Sun; tentative. */
#define	AUE_auth_user		7000	/* Not assigned by Sun. */
#define	AUE_ssconn		7001	/* Not assigned by Sun. */
#define	AUE_ssauthorize		7002	/* Not assigned by Sun. */
#define	AUE_ssauthint		7003	/* Not assigned by Sun. */
#define	AUE_openssh		32800

#endif /* !_BSM_AUDIT_UEVENTS_H_ */
