#!/usr/bin/awk -f
# 
# Copyright (c) 2000  "HOSOKAWA, Tatsumi" <hosokawa@FreeBSD.org>
# Copyright (c) 2002  Ruslan Ermilov <ru@FreeBSD.org>
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

function usage()
{
	print "usage: driver-remove.awk config_file BOOTMFS" > "/dev/stderr";
	exit 1;
}

function err(eval, fmt, what)
{
	printf "driver-remove.awk: " fmt "\n", what > "/dev/stderr";
	exit eval;
}

function readconfig()
{
	while ((r = (getline < config)) > 0) {
		sub("#.*$", "");
		if (sub(/^[[:alnum:]_]+[ \t]+[[:alnum:]_]+[ \t]+[0-9]+[ \t]+[[:alnum:]_]+[ \t]+\".*\"[ \t]*$/, "&")) {
			sub(/[ \t]+/, "#");
			sub(/[ \t]+/, "#");
			sub(/[ \t]+/, "#");
			sub(/[ \t]+/, "#");
			sub(/\"/, "");
			sub(/\"/, "");
			split($0, arg, "#");
			if (arg[4] == "options")
				options[arg[1]] = 1;
			else
				drivers[arg[1]] = 1;
		}
	}
	if (r == -1)
		err(1, "error reading %s", config);
	close(config);
}

BEGIN {
	if (ARGC != 3)
		usage();

	config = ARGV[1];
	bootmfs = ARGV[2];

	readconfig();

	lines = 0;
	while ((r = (getline < bootmfs)) > 0) {
		if (/^device[ \t]+[[:alnum:]_]+/) {
			dev = $0;
			sub(/^device[ \t]+/, "", dev);
			sub(/[ \t]+.*$/, "", dev);
			if (dev in drivers)
				continue;
		} else if (/^options[ \t]+[[:alnum:]_]+/) {
			opt = $0;
			sub(/^options[ \t]+/, "", opt);
			sub(/[ \t]+.*$/, "", opt);
			if (opt in options)
				continue;
		}
		line[lines++] = $0;
	}
	if (r == -1)
		err(1, "error reading %s", bootmfs);
	close(bootmfs);
	printf "" > bootmfs;
	for (i = 0; i < lines; i++)
		print line[i] >> bootmfs;
	close(bootmfs);
}
