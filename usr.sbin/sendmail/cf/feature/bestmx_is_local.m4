divert(-1)
#
# Copyright (c) 1983 Eric P. Allman
# Copyright (c) 1988, 1993
#	The Regents of the University of California.  All rights reserved.
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
#	This product includes software developed by the University of
#	California, Berkeley and its contributors.
# 4. Neither the name of the University nor the names of its contributors
#    may be used to endorse or promote products derived from this software
#    without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#

divert(0)
VERSIONID(`@(#)bestmx_is_local.m4	8.5 (Berkeley) 3/28/97')
divert(-1)

LOCAL_CONFIG
# turn on bestMX lookup table
Kbestmx bestmx

# limit bestmx to these domains
CB`'_ARG_

LOCAL_NET_CONFIG

# If we are the best MX for a site, then we want to accept
# its mail as local.  We assume we've already weeded out mail to
# UUCP sites which are connected to us, which should also have
# listed us as their best MX.
#
# Warning: this may generate a lot of extra DNS traffic -- a
# lower cost method is to list all the expected best MX hosts
# in $=w.  This should be fine (and easier to administer) for
# low to medium traffic hosts.  If you use the limited bestmx
# by passing in a set of possible domains it will improve things.

ifelse(_ARG_, `', `', `#')dnl		unlimited bestmx
R$* < @ $* > $*			$: $1 < @ $2 @@ $(bestmx $2 $) > $3
ifelse(_ARG_, `', `#', `')dnl		limit bestmx to $=B
R$* < @ $* $=B . > $*		$: $1 < @ $2 $3 . @@ $(bestmx $2 $3 . $) > $4
R$* $=O $* < @ $* @@ $=w . > $*	$@ $>97 $1 $2 $3
R$* < @ $* @@ $=w . > $*	$#local $: $1
R$* < @ $* @@ $* > $*		$: $1 < @ $2 > $4
