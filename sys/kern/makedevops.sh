#!/bin/sh -
#
# Copyright (c) 1992, 1993
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
# From @(#)vnode_if.sh	8.1 (Berkeley) 6/10/93
# $Id$
#

# Script to produce device front-end sugar.
#
# usage: makedevops.sh srcfile
#
# These awk scripts are not particularly well written, specifically they
# don't use arrays well and figure out the same information repeatedly.
# Please rewrite them if you actually understand how to use awk.  Note,
# they use nawk extensions and gawk's toupper.

if [ $# -ne 2 ] ; then
	echo 'usage: makedevops.sh [-c|-h] srcfile'
	exit 1
fi

makec=0
makeh=0

if [ "$1" = "-c" ]; then
    makec=1
fi

if [ "$1" = "-h" ]; then
    makeh=1
fi

# Name of the source file.
SRC=$2

# Names of the created files.
CTMP=ctmp$$
HTMP=htmp$$

CFILE=`basename $SRC .m`.c
HFILE=`basename $SRC .m`.h

# Awk program (must support nawk extensions and gawk's "toupper")
# Use "awk" at Berkeley, "gawk" elsewhere.
AWK=awk

# Awk script to take file.do and turn it into file.h and file.c
$AWK "
	BEGIN {
		src = \"$SRC\";
		header = \"$HTMP\";
		cfile = \"$CTMP\";
		hfile = \"$HFILE\";
		"'

		printf("/*\n")						> header;
		printf(" * This file is produced automatically.\n")	> header;
		printf(" * Do not modify anything in here by hand.\n")	> header;
		printf(" *\n")						> header;
		printf(" * Created from %s with makedevops.sh\n", src)	> header;
		printf(" */\n\n")					> header;

		printf("/*\n")						> cfile;
		printf(" * This file is produced automatically.\n")	> cfile;
		printf(" * Do not modify anything in here by hand.\n")	> cfile;
		printf(" *\n")						> cfile;
		printf(" * Created from %s with makedevops.sh\n", src)	> cfile;
		printf(" */\n\n")					> cfile;
		printf("#include <sys/param.h>\n")			> cfile;
		printf("#include <sys/queue.h>\n")			> cfile;
		printf("#include <sys/bus_private.h>\n")		> cfile;

		methodcount = 0
	}
	NF == 0 {
		next;
	}
	/^#include/ {
		print $0 > cfile;
	}
	/^#/ {
		next;
	}
	/^INTERFACE/ {
		intname = $2;
		printf("#ifndef _%s_if_h_\n", intname)		> header;
		printf("#define _%s_if_h_\n\n", intname)	> header;
		printf("#include \"%s\"\n\n", hfile)			> cfile;
	}
	/^METHOD/ {
		# Get the function name and return type.
		ret = "";
		sep = "";
		for (i = 2; i < NF - 1; i++) {
			ret = sep $i;
			sep = " ";
		}
		name = $i;

		# Get the function arguments.
		for (c1 = 0;; ++c1) {
			if (getline <= 0)
				exit
			if ($0 ~ "^};")
				break;
			a[c1] = $0;
		}

		methods[methodcount++] = name;

		mname = intname "_" name;
		umname = toupper(mname);

		# Print out the method declaration
		printf("extern struct device_op_desc %s_desc;\n", mname) > header;
		printf("%s %s(", ret, umname) > header;
		sep = ", ";
		for (c2 = 0; c2 < c1; ++c2) {
			if (c2 == c1 - 1)
				sep = " );\n";
			c3 = split(a[c2], t);
			for (c4 = 0; c4 < c3; ++c4)
				printf("%s ", t[c4]) > header;
			beg = match(t[c3], "[^*]");
			end = match(t[c3], ";");
			printf("%s%s%s",
			    substr(t[c4], 0, beg - 1),
			    substr(t[c4], beg, end - beg), sep) > header;
		}

		# Print the method desc
		printf("struct device_op_desc %s_desc = {\n", mname) > cfile;
		printf("\t0,\n") > cfile;
		printf("\t\"%s\"\n", name) > cfile;
		printf("};\n\n") > cfile;

		# Print out the method typedef
		printf("typedef %s %s_t(\n", ret, mname) > cfile;
		sep = ",\n";
		for (c2 = 0; c2 < c1; ++c2) {
			if (c2 == c1 - 1)
				sep = ");\n";
			c3 = split(a[c2], t);
			printf("\t") > cfile;
			for (c4 = 0; c4 < c3; ++c4)
				printf("%s ", t[c4]) > cfile;
			beg = match(t[c3], "[^*]");
			end = match(t[c3], ";");
			printf("%s%s%s",
			    substr(t[c4], 0, beg - 1),
			    substr(t[c4], beg, end - beg), sep) > cfile;
		}

		# Print out the method itself
		printf("%s %s(\n", ret, umname) > cfile;
		sep = ",\n";
		for (c2 = 0; c2 < c1; ++c2) {
			if (c2 == c1 - 1)
				sep = ")\n";
			c3 = split(a[c2], t);
			printf("\t") > cfile;
			for (c4 = 0; c4 < c3; ++c4)
				printf("%s ", t[c4]) > cfile;
			beg = match(t[c3], "[^*]");
			end = match(t[c3], ";");
			printf("%s%s%s",
			    substr(t[c4], 0, beg - 1),
			    substr(t[c4], beg, end - beg), sep) > cfile;
		}
		printf("{\n") > cfile;
		printf("\t%s_t *m = (%s_t *) DEVOPMETH(dev, %s);\n",
			mname, mname, mname) > cfile;
		if (ret != "void")
			printf("\treturn m(") > cfile;
		else
			printf("\tm(") > cfile;
		sep = ", ";
		for (c2 = 0; c2 < c1; ++c2) {
			if (c2 == c1 - 1)
				sep = ");\n";
			c3 = split(a[c2], t);
			beg = match(t[c3], "[^*]");
			end = match(t[c3], ";");
			printf("%s%s", substr(t[c3], beg, end - beg), sep) > cfile;
		}
		printf("}\n\n") > cfile;
	}
	END {
		printf("\n#endif /* _%s_if_h_ */\n", intname)	> header;
	}' < $SRC

if [ $makec = 1 ]; then
    mv $CTMP $CFILE
else
    rm $CTMP
fi

if [ $makeh = 1 ]; then
    mv $HTMP $HFILE
else
    rm $HTMP
fi
