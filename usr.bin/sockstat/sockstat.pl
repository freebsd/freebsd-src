#!/usr/bin/perl5
#-
# Copyright (c) 1999 Dag-Erling Coïdan Smgrav
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer
#    in this position and unchanged.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. The name of the author may not be used to endorse or promote products
#    derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
# NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# $FreeBSD$
#

my (%proto, %myaddr, %hisaddr);
my ($user, $cmd, $pid, $fd, $inet, $type, $proto, $sock, $laddr, $faddr);

print <<EOH;
USER     COMMAND    PID   FD PROTO  LOCAL ADDRESS         FOREIGN ADDRESS
EOH
format STDOUT =
@<<<<<<< @<<<<<<< @>>>> @>>> @<<<<< @<<<<<<<<<<<<<<<<<<<< @<<<<<<<<<<<<<<<<<<<<
$user,   $cmd,    $pid, $fd, $proto,$laddr,               $faddr
.

open NETSTAT, "/usr/bin/netstat -Aan |" or die "'netstat' failed: $!";
<NETSTAT>; <NETSTAT>;

while (<NETSTAT>) {
    my ($sock, $proto, $recvq, $sendq, $laddr, $faddr, $state) = split;
    ($proto{$sock}, $myaddr{$sock}, $hisaddr{$sock}) =
	($proto, $laddr, $faddr);
}

close NETSTAT;

open FSTAT, "/usr/bin/fstat |" or die "'fstat' failed: $!\n";

while (<FSTAT>) {
    ($user, $cmd, $pid, $fd, $inet, $type, $proto, $sock) = split;
    chop $fd;
    next unless ($inet =~ m/^internet6?$/) && ($type ne "raw");
    ($proto, $laddr, $faddr) =
	($proto{$sock}, $myaddr{$sock}, $hisaddr{$sock});
    write STDOUT;
}

close FSTAT;
