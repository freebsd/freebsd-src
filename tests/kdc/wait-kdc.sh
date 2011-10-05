#!/bin/sh
#
# Copyright (c) 2006 - 2007 Kungliga Tekniska Högskolan
# (Royal Institute of Technology, Stockholm, Sweden). 
# All rights reserved. 
#
# Redistribution and use in source and binary forms, with or without 
# modification, are permitted provided that the following conditions 
# are met: 
#
# 1. Redistributions of source code must retain the above copyright 
#    notice, this list of conditions and the following disclaimer. 
#
# 2. Redistributions in binary form must reproduce the above copyright 
#    notice, this list of conditions and the following disclaimer in the 
#    documentation and/or other materials provided with the distribution. 
#
# 3. Neither the name of the Institute nor the names of its contributors 
#    may be used to endorse or promote products derived from this software 
#    without specific prior written permission. 
#
# THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND 
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
# ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE 
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS 
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY 
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
# SUCH DAMAGE. 

name=${1:-KDC}
log=${2:-messages.log}
waitfor=${3:-${name} started}

t=0
waitsec=35

echo "Waiting for ${name} to start, looking logfile ${log}"

while true ; do
    t=`expr ${t} + 2`
    sleep 2
    echo "Have waited $t seconds"
    if tail -30 ${log} | grep "${waitfor}" > /dev/null; then
	break
    fi
    if tail -30 ${log} | grep "No sockets" ; then
       echo "The ${name} failed to bind to any sockets, another ${name} running ?"
       exit 1
    fi
    if tail -30 ${log} | grep "bind" | grep "Operation not permitted" ; then
       echo "The ${name} failed to bind to any sockets, another ${name} running ?"
       exit 1
    fi
    if [ "$t" -gt $waitsec ]; then
       echo "Waited for $waitsec for the ${name} to start, and it didnt happen"
       exit 2
    fi
done

exit 0