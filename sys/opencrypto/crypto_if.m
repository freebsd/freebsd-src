#
# Copyright (c) 2002, Sam Leffler
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. All advertising materials mentioning features or use of this software
#    must display the following acknowledgement:
#    This product includes software developed by Boris Popov.
# 4. Neither the name of the author nor the names of any co-contributors
#    may be used to endorse or promote products derived from this software
#    without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
# $FreeBSD$
#

#include <crypto/cryptodev.h>

INTERFACE crypto;

METHOD int32_t get_driverid {
	u_int32_t	flags;
};

# XXX define typedefs to work around inadequate parser
HEADER {
	typedef int crypto_newsession_cb(void*, u_int32_t*, struct cryptoini*);
	typedef int crypto_freesession_cb(void*, u_int64_t*);
	typedef int crypto_process_cb(void*, struct cryptop*);
	typedef int crypto_kprocess_cb(void*, struct cryptkop*);
};

METHOD int register {
	u_int32_t	driverid;
	int		alg;
	u_int16_t	maxoplen;
	u_int32_t	flags;
	crypto_newsession_cb* newses;
	crypto_freesession_cb* freeses;
	crypto_process_cb* process;
	void		*arg;
};

METHOD int kregister {
	u_int32_t	driverid;
	int		kalg;
	u_int32_t	flags;
	crypto_kprocess_cb* kprocess;
	void		*arg;
};

METHOD int unregister {
	u_int32_t	driverid;
	int		alg;
};

METHOD int unregister_all {
	u_int32_t	driverid;
};

METHOD int newsession {
	u_int64_t	*sid;
	struct cryptoini *cri;
	int		hard;
};

METHOD int freesession {
	u_int64_t	sid;
};

METHOD int dispatch {
	struct cryptop	*crp;
};

METHOD int kdispatch {
	struct cryptkop	*krp;
};

METHOD int crypto_unblock {
	u_int32_t	driverid;
	int		what;
};

METHOD int invoke {
	struct cryptop	*crp;
};

METHOD int kinvoke {
	struct cryptkop	*krp;
};

METHOD struct cryptop * getreq {
	int	num;
};

METHOD void freereq {
	struct cryptop	*crp;
};

METHOD void done {
	struct cryptop	*crp;
};

METHOD void kdone {
	struct cryptkop	*krp;
};

METHOD int getfeat {
	int		*featp;
};
