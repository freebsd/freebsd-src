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
#	@(#)vnode_if.sh	8.1 (Berkeley) 6/10/93
#

# Script to produce VFS front-end sugar.
#
# usage: vnode_if.sh srcfile
#	(where srcfile is currently /sys/kern/vnode_if.src)
#
# These awk scripts are not particularly well written, specifically they
# don't use arrays well and figure out the same information repeatedly.
# Please rewrite them if you actually understand how to use awk.  Note,
# they use nawk extensions and gawk's toupper.

if [ $# -ne 1 ] ; then
	echo 'usage: vnode_if.sh srcfile'
	exit 1
fi

# Name of the source file.
SRC=$1

# Names of the created files.
CFILE=vnode_if.c
HEADER=vnode_if.h

# Awk program (must support nawk extensions and gawk's "toupper")
# Use "awk" at Berkeley, "gawk" elsewhere.
AWK=awk

# Print out header information for vnode_if.h.
cat << END_OF_LEADING_COMMENT > $HEADER
/*
 * This file is produced automatically.
 * Do not modify anything in here by hand.
 *
 * Created from @(#)vnode_if.sh	8.1 (Berkeley) 6/10/93
 */

extern struct vnodeop_desc vop_default_desc;
END_OF_LEADING_COMMENT

# Awk script to take vnode_if.src and turn it into vnode_if.h.
$AWK '
	NF == 0 || $0 ~ "^#" {
		next;
	}
	{
		# Get the function name.
		name = $1;
		uname = toupper(name);

		# Get the function arguments.
		for (c1 = 0;; ++c1) {
			if (getline <= 0)
				exit
			if ($0 ~ "^};")
				break;
			a[c1] = $0;
		}

		# Print out the vop_F_args structure.
		printf("struct %s_args {\n\tstruct vnodeop_desc *a_desc;\n",
		    name);
		for (c2 = 0; c2 < c1; ++c2) {
			c3 = split(a[c2], t);
			printf("\t");
			if (t[2] ~ "WILLRELE")
				c4 = 3;
			else 
				c4 = 2;
			for (; c4 < c3; ++c4)
				printf("%s ", t[c4]);
			beg = match(t[c3], "[^*]");
			printf("%sa_%s\n",
			    substr(t[c4], 0, beg - 1), substr(t[c4], beg));
		}
		printf("};\n");

		# Print out extern declaration.
		printf("extern struct vnodeop_desc %s_desc;\n", name);

		# Print out inline struct.
		printf("static inline int %s(", uname);
		sep = ", ";
		for (c2 = 0; c2 < c1; ++c2) {
			if (c2 == c1 - 1)
				sep = ")\n";
			c3 = split(a[c2], t);
			beg = match(t[c3], "[^*]");
			end = match(t[c3], ";");
			printf("%s%s", substr(t[c3], beg, end - beg), sep);
		}
		for (c2 = 0; c2 < c1; ++c2) {
			c3 = split(a[c2], t);
			printf("\t");
			if (t[2] ~ "WILLRELE")
				c4 = 3;
			else
				c4 = 2;
			for (; c4 < c3; ++c4)
				printf("%s ", t[c4]);
			beg = match(t[c3], "[^*]");
			printf("%s%s\n",
			    substr(t[c4], 0, beg - 1), substr(t[c4], beg));
		}
		printf("{\n\tstruct %s_args a;\n\n", name);
		printf("\ta.a_desc = VDESC(%s);\n", name);
		for (c2 = 0; c2 < c1; ++c2) {
			c3 = split(a[c2], t);
			printf("\t");
			beg = match(t[c3], "[^*]");
			end = match(t[c3], ";");
			printf("a.a_%s = %s\n",
			    substr(t[c3], beg, end - beg), substr(t[c3], beg));
		}
		c1 = split(a[0], t);
		beg = match(t[c1], "[^*]");
		end = match(t[c1], ";");
		printf("\treturn (VCALL(%s, VOFFSET(%s), &a));\n}\n",
		    substr(t[c1], beg, end - beg), name);
	}' < $SRC >> $HEADER

# Print out header information for vnode_if.c.
cat << END_OF_LEADING_COMMENT > $CFILE
/*
 * This file is produced automatically.
 * Do not modify anything in here by hand.
 *
 * Created from @(#)vnode_if.sh	8.1 (Berkeley) 6/10/93
 */

#include <sys/param.h>
#include <sys/mount.h>
#include <sys/vnode.h>

struct vnodeop_desc vop_default_desc = {
	0,
	"default",
	0,
	NULL,
	VDESC_NO_OFFSET,
	VDESC_NO_OFFSET,
	VDESC_NO_OFFSET,
	VDESC_NO_OFFSET,
	NULL,
};

END_OF_LEADING_COMMENT

# Awk script to take vnode_if.src and turn it into vnode_if.c.
$AWK 'function kill_surrounding_ws (s) {
		sub (/^[ \t]*/, "", s);
		sub (/[ \t]*$/, "", s);
		return s;
	}

	function read_args() {
		numargs = 0;
		while (getline ln) {
			if (ln ~ /}/) {
				break;
			};
	
			# Delete comments, if any.
			gsub (/\/\*.*\*\//, "", ln);
			
			# Delete leading/trailing space.
			ln = kill_surrounding_ws(ln);
	
			# Pick off direction.
			if (1 == sub(/^INOUT[ \t]+/, "", ln))
				dir = "INOUT";
			else if (1 == sub(/^IN[ \t]+/, "", ln))
				dir = "IN";
			else if (1 == sub(/^OUT[ \t]+/, "", ln))
				dir = "OUT";
			else
				bail("No IN/OUT direction for \"" ln "\".");

			# check for "WILLRELE"
			if (1 == sub(/^WILLRELE[ \t]+/, "", ln)) {
				rele = "WILLRELE";
			} else {
				rele = "WONTRELE";
			};
	
			# kill trailing ;
			if (1 != sub (/;$/, "", ln)) {
				bail("Missing end-of-line ; in \"" ln "\".");
			};
	
			# pick off variable name
			if (!(i = match(ln, /[A-Za-z0-9_]+$/))) {
				bail("Missing var name \"a_foo\" in \"" ln "\".");
			};
			arg = substr (ln, i);
			# Want to <<substr(ln, i) = "";>>, but nawk cannot.
			# Hack around this.
			ln = substr(ln, 1, i-1);
	
			# what is left must be type
			# (put clean it up some)
			type = ln;
			gsub (/[ \t]+/, " ", type);   # condense whitespace
			type = kill_surrounding_ws(type);
	
			# (boy this was easier in Perl)
	
			numargs++;
			dirs[numargs] = dir;
			reles[numargs] = rele;
			types[numargs] = type;
			args[numargs] = arg;
		};
	}

	function generate_operation_vp_offsets() {
		printf ("int %s_vp_offsets[] = {\n", name);
		# as a side effect, figure out the releflags
		releflags = "";
		vpnum = 0;
		for (i=1; i<=numargs; i++) {
			if (types[i] == "struct vnode *") {
				printf ("\tVOPARG_OFFSETOF(struct %s_args,a_%s),\n",
					name, args[i]);
				if (reles[i] == "WILLRELE") {
					releflags = releflags "|VDESC_VP" vpnum "_WILLRELE";
				};
				vpnum++;
			};
		};
		sub (/^\|/, "", releflags);
		print "\tVDESC_NO_OFFSET";
		print "};";
	}
	
	function find_arg_with_type (type) {
		for (i=1; i<=numargs; i++) {
			if (types[i] == type) {
				return "VOPARG_OFFSETOF(struct " name "_args,a_" args[i] ")";
			};
		};
		return "VDESC_NO_OFFSET";
	}
	
	function generate_operation_desc() {
		printf ("struct vnodeop_desc %s_desc = {\n", name);
		# offset
		printf ("\t0,\n");
		# printable name
		printf ("\t\"%s\",\n", name);
		# flags
		vppwillrele = "";
		for (i=1; i<=numargs; i++) {
			if (types[i] == "struct vnode **" &&
				(reles[i] == "WILLRELE")) {
				vppwillrele = "|VDESC_VPP_WILLRELE";
			};
		};
		if (releflags == "") {
			printf ("\t0%s,\n", vppwillrele);
		} else {
			printf ("\t%s%s,\n", releflags, vppwillrele);
		};
		# vp offsets
		printf ("\t%s_vp_offsets,\n", name);
		# vpp (if any)
		printf ("\t%s,\n", find_arg_with_type("struct vnode **"));
		# cred (if any)
		printf ("\t%s,\n", find_arg_with_type("struct ucred *"));
		# proc (if any)
		printf ("\t%s,\n", find_arg_with_type("struct proc *"));
		# componentname
		printf ("\t%s,\n", find_arg_with_type("struct componentname *"));
		# transport layer information
		printf ("\tNULL,\n};\n");
	}

	NF == 0 || $0 ~ "^#" {
		next;
	}
	{
		# get the function name
		name = $1;

		# get the function arguments
		read_args();

		# Print out the vop_F_vp_offsets structure.  This all depends
		# on naming conventions and nothing else.
		generate_operation_vp_offsets();

		# Print out the vnodeop_desc structure.
		generate_operation_desc();

		printf "\n";

	}' < $SRC >> $CFILE
# THINGS THAT DON'T WORK RIGHT YET.
# 
# Two existing BSD vnodeops (bwrite and strategy) don't take any vnodes as
# arguments.  This means that these operations can't function successfully
# through a bypass routine.
#
# Bwrite and strategy will be replaced when the VM page/buffer cache
# integration happens.
#
# To get around this problem for now we handle these ops as special cases.

cat << END_OF_SPECIAL_CASES >> $HEADER
#include <sys/buf.h>
struct vop_strategy_args {
	struct vnodeop_desc *a_desc;
	struct buf *a_bp;
};
extern struct vnodeop_desc vop_strategy_desc;
static inline int VOP_STRATEGY(bp)
	struct buf *bp;
{
	struct vop_strategy_args a;

	a.a_desc = VDESC(vop_strategy);
	a.a_bp = bp;
	return (VCALL((bp)->b_vp, VOFFSET(vop_strategy), &a));
}

struct vop_bwrite_args {
	struct vnodeop_desc *a_desc;
	struct buf *a_bp;
};
extern struct vnodeop_desc vop_bwrite_desc;
static inline int VOP_BWRITE(bp)
	struct buf *bp;
{
	struct vop_bwrite_args a;

	a.a_desc = VDESC(vop_bwrite);
	a.a_bp = bp;
	return (VCALL((bp)->b_vp, VOFFSET(vop_bwrite), &a));
}
END_OF_SPECIAL_CASES

cat << END_OF_SPECIAL_CASES >> $CFILE
int vop_strategy_vp_offsets[] = {
	VDESC_NO_OFFSET
};
struct vnodeop_desc vop_strategy_desc = {
	0,
	"vop_strategy",
	0,
	vop_strategy_vp_offsets,
	VDESC_NO_OFFSET,
	VDESC_NO_OFFSET,
	VDESC_NO_OFFSET,
	VDESC_NO_OFFSET,
	NULL,
};
int vop_bwrite_vp_offsets[] = {
	VDESC_NO_OFFSET
};
struct vnodeop_desc vop_bwrite_desc = {
	0,
	"vop_bwrite",
	0,
	vop_bwrite_vp_offsets,
	VDESC_NO_OFFSET,
	VDESC_NO_OFFSET,
	VDESC_NO_OFFSET,
	VDESC_NO_OFFSET,
	NULL,
};
END_OF_SPECIAL_CASES

# Add the vfs_op_descs array to the C file.
$AWK '
	BEGIN {
		printf("\nstruct vnodeop_desc *vfs_op_descs[] = {\n");
		printf("\t&vop_default_desc,	/* MUST BE FIRST */\n");
		printf("\t&vop_strategy_desc,	/* XXX: SPECIAL CASE */\n");
		printf("\t&vop_bwrite_desc,	/* XXX: SPECIAL CASE */\n");
	}
	END {
		printf("\tNULL\n};\n");
	}
	NF == 0 || $0 ~ "^#" {
		next;
	}
	{
		# Get the function name.
		printf("\t&%s_desc,\n", $1);

		# Skip the function arguments.
		for (;;) {
			if (getline <= 0)
				exit
			if ($0 ~ "^};")
				break;
		}
	}' < $SRC >> $CFILE

