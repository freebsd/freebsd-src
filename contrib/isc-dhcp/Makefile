# Makefile
#
# Copyright (c) 2002 Internet Software Consortium.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. Neither the name of Internet Software Consortium nor the names
#    of its contributors may be used to endorse or promote products derived
#    from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE INTERNET SOFTWARE CONSORTIUM AND
# CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
# INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED.  IN NO EVENT SHALL THE INTERNET SOFTWARE CONSORTIUM OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
# USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
# OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
# This software has been written for the Internet Software Consortium
# by Ted Lemon in cooperation with Vixie Enterprises and Nominum, Inc.
# To learn more about the Internet Software Consortium, see
# ``http://www.isc.org/''.  To learn more about Vixie Enterprises,
# see ``http://www.vix.com''.   To learn more about Nominum, Inc., see
# ``http://www.nominum.com''.
#

all:
	@sysname=`./configure --print-sysname`; \
	 if [ ! -d work.$$sysname ]; then \
	   echo No build directory for $$sysname - please run ./configure.; \
	else \
	   (cd work.$$sysname; make all); \
	fi

install:
	@sysname=`./configure --print-sysname`; \
	 if [ ! -d work.$$sysname ]; then \
	   echo No build directory for $$sysname - please run ./configure.; \
	else \
	   (cd work.$$sysname; make install); \
	fi

depend:
	@sysname=`./configure --print-sysname`; \
	 if [ ! -d work.$$sysname ]; then \
	   echo No build directory for $$sysname - please run ./configure.; \
	else \
	   (cd work.$$sysname; make depend); \
	fi

clean:
	@sysname=`./configure --print-sysname`; \
	 if [ ! -d work.$$sysname ]; then \
	   echo No build directory for $$sysname - please run ./configure.; \
	else \
	   (cd work.$$sysname; make clean); \
	fi

realclean:
	@sysname=`./configure --print-sysname`; \
	 if [ ! -d work.$$sysname ]; then \
	   echo No build directory for $$sysname - please run ./configure.; \
	else \
	   (cd work.$$sysname; make realclean); \
	fi

distclean:
	@sysname=`./configure --print-sysname`; \
	 if [ ! -d work.$$sysname ]; then \
	   echo No build directory for $$sysname - please run ./configure.; \
	else \
	   (cd work.$$sysname; make distclean); \
	fi

links:
	@sysname=`./configure --print-sysname`; \
	 if [ ! -d work.$$sysname ]; then \
	   echo No build directory for $$sysname - please run ./configure.; \
	else \
	   (cd work.$$sysname; make links); \
	fi

