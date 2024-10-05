/*
 * Copyright (c) 2002 - 2003
 * NetGroup, Politecnico di Torino (Italy)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Politecnico di Torino nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef __CONFIG_PARAMS_H__
#define __CONFIG_PARAMS_H__

//
// Parameters set from the configuration file.
//

#define MAX_LINE 2048		/* Maximum chars allowed for the host list (in passive mode) */
#define MAX_HOST_LIST 64000
#define MAX_ACTIVE_LIST 10

struct active_pars
{
	char address[MAX_LINE + 1];	// keeps the network address (either numeric or literal) to of the active client
	char port[MAX_LINE + 1];	// keeps the network port to bind to
	int ai_family;			// address family to use
};

extern char hostlist[MAX_HOST_LIST + 1];	//!< Keeps the list of the hosts that are allowed to connect to this server
extern struct active_pars activelist[MAX_ACTIVE_LIST];		//!< Keeps the list of the hosts (host, port) on which I want to connect to (active mode)
extern int nullAuthAllowed;			//!< '1' if we permit NULL authentication, '0' otherwise
extern char loadfile[MAX_LINE + 1];		//!< Name of the file from which we have to load the configuration

#endif
