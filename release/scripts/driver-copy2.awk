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
	print "usage: driver-copy2.awk config_file src_ko_dir dst_ko_dir" > "/dev/stderr";
	exit 1;
}

function err(eval, fmt, what)
{
	printf "driver-copy2.awk: " fmt "\n", what > "/dev/stderr";
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
			flp[arg[2]] = arg[3];
			dsc[arg[2]] = arg[5];
		}
	}
	if (r == -1)
		err(1, "error reading %s", config);
	close(config);
}

BEGIN {
	if (ARGC != 4)
		usage();

	config = ARGV[1];
	srcdir = ARGV[2];
	dstdir = ARGV[3];

	readconfig();

	if (system("test -d " srcdir) != 0)
		err(1, "cannot find %s directory", srcdir);
	if (system("test -d " dstdir) != 0)
		err(1, "cannot find %s directory", dstdir);

	for (f in flp) {
		if (flp[f] == 1) {
			print f ": There's nothing to do with driver on first floppy." > "/dev/stderr";
		} else if (flp[f] == 2) {
			srcfile = srcdir "/" f ".ko";
			dstfile = dstdir "/" f ".ko";
			dscfile = dstdir "/" f ".dsc";
			print "Copying " f ".ko to " dstdir > "/dev/stderr";
			if (system("cp " srcfile " " dstfile) != 0)
				exit 1;
			printf "%s", dsc[f] > dscfile;
			close(dscfile);
		} else if (flp[f] == 3) {
			# third driver floppy (not yet implemented)
			err(1, "%s: 3rd driver floppy support is not implemented", f);
		}
	}
}
