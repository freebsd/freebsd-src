#!/usr/bin/awk -f

#-
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
# $FreeBSD$
#
# Script to produce VFS front-end sugar.
#
# usage: vnode_if.awk <srcfile> [-c | -h]
#	(where <srcfile> is currently /sys/kern/vnode_if.src)
#

function usage()
{
	print "usage: vnode_if.awk <srcfile> [-c|-h]";
	exit 1;
}

function die(msg, what)
{
	printf msg "\n", what > "/dev/stderr";
	exit 1;
}

function t_spc(type)
{
	# Append a space if the type is not a pointer
	return (type ~ /\*$/) ? type : type " ";
}

# These are just for convenience ...
function printc(s) {print s > cfile;}
function printh(s) {print s > hfile;}

function add_debug_code(name, arg, pos)
{
	if (arg == "vpp")
		arg = "*vpp";
	if (lockdata[name, arg, pos] && (lockdata[name, arg, pos] != "-")) {
		if (arg ~ /^\*/) {
			printh("\tif ("substr(arg, 2)" != NULL) {");
		}
		printh("\tASSERT_VI_UNLOCKED("arg", \""uname"\");");
		# Add assertions for locking
		if (lockdata[name, arg, pos] == "L")
			printh("\tASSERT_VOP_LOCKED("arg", \""uname"\");");
		else if (lockdata[name, arg, pos] == "U")
			printh("\tASSERT_VOP_UNLOCKED("arg", \""uname"\");");
		else if (0) {
			# XXX More checks!
		}
		if (arg ~ /^\*/) {
			printh("\t}");
		}
	}
}

function add_debug_pre(name)
{
	if (lockdata[name, "pre"]) {
		printh("#ifdef	DEBUG_VFS_LOCKS");
		printh("\t"lockdata[name, "pre"]"(&a);");
		printh("#endif");
	}
}

function add_debug_post(name)
{
	if (lockdata[name, "post"]) {
		printh("#ifdef	DEBUG_VFS_LOCKS");
		printh("\t"lockdata[name, "post"]"(&a, rc);");
		printh("#endif");
	}
}

function find_arg_with_type (type)
{
	for (jj = 0; jj < numargs; jj++) {
		if (types[jj] == type) {
			return "VOPARG_OFFSETOF(struct " \
			    name "_args,a_" args[jj] ")";
		}
	}

	return "VDESC_NO_OFFSET";
}

BEGIN{

# Process the command line
for (i = 1; i < ARGC; i++) {
	arg = ARGV[i];
	if (arg !~ /^-[ch]+$/ && arg !~ /\.src$/)
		usage();
	if (arg ~ /^-.*c/)
		cfile = "vnode_if.c";
	if (arg ~ /^-.*h/)
		hfile = "vnode_if.h";
	if (arg ~ /\.src$/)
		srcfile = arg;
}
ARGC = 1;

if (!cfile && !hfile)
	exit 0;

if (!srcfile)
	usage();

common_head = \
    "/*\n" \
    " * This file is produced automatically.\n" \
    " * Do not modify anything in here by hand.\n" \
    " *\n" \
    " * Created from $FreeBSD$\n" \
    " */\n" \
    "\n";

if (hfile)
	printh(common_head "extern struct vnodeop_desc vop_default_desc;");

if (cfile) {
	printc(common_head \
	    "#include <sys/param.h>\n" \
	    "#include <sys/systm.h>\n" \
	    "#include <sys/vnode.h>\n" \
	    "\n" \
	    "struct vnodeop_desc vop_default_desc = {\n" \
	    "	1,\t\t\t/* special case, vop_default => 1 */\n" \
	    "	\"default\",\n" \
	    "	0,\n" \
	    "	NULL,\n" \
	    "	VDESC_NO_OFFSET,\n" \
	    "	VDESC_NO_OFFSET,\n" \
	    "	VDESC_NO_OFFSET,\n" \
	    "	VDESC_NO_OFFSET,\n" \
	    "	NULL,\n" \
	    "};\n");
}

while ((getline < srcfile) > 0) {
	if (NF == 0)
		continue;
	if ($1 ~ /^#%/) {
		if (NF != 6  ||  $1 != "#%"  || \
		    $2 !~ /^[a-z]+$/  ||  $3 !~ /^[a-z]+$/  || \
		    $4 !~ /^.$/  ||  $5 !~ /^.$/  ||  $6 !~ /^.$/)
			continue;
		if ($3 == "vpp")
			$3 = "*vpp";
		lockdata["vop_" $2, $3, "Entry"] = $4;
		lockdata["vop_" $2, $3, "OK"]    = $5;
		lockdata["vop_" $2, $3, "Error"] = $6;			
		continue;
	}

	if ($1 ~ /^#!/) {
		if (NF != 4 || $1 != "#!")
			continue;
		if ($3 != "pre" && $3 != "post")
			continue;
		lockdata["vop_" $2, $3] = $4;
		continue;
	}
	if ($1 ~ /^#/)
		continue;

	# Get the function name.
	name = $1;
	uname = toupper(name);

	# Start constructing a ktrpoint string
	ctrstr = "\"" uname;
	# Get the function arguments.
	for (numargs = 0; ; ++numargs) {
		if ((getline < srcfile) <= 0) {
			die("Unable to read through the arguments for \"%s\"",
			    name);
		}
		if ($1 ~ /^\};/)
			break;

		# Delete comments, if any.
		gsub (/\/\*.*\*\//, "");

		# Condense whitespace and delete leading/trailing space.
		gsub(/[[:space:]]+/, " ");
		sub(/^ /, "");
		sub(/ $/, "");

		# Pick off direction.
		if ($1 != "INOUT" && $1 != "IN" && $1 != "OUT")
			die("No IN/OUT direction for \"%s\".", $0);
		dirs[numargs] = $1;
		sub(/^[A-Z]* /, "");

		if ((reles[numargs] = $1) == "WILLRELE")
			sub(/^[A-Z]* /, "");
		else
			reles[numargs] = "WONTRELE";

		# kill trailing ;
		if (sub(/;$/, "") < 1)
			die("Missing end-of-line ; in \"%s\".", $0);

		# pick off variable name
		if ((argp = match($0, /[A-Za-z0-9_]+$/)) < 1)
			die("Missing var name \"a_foo\" in \"%s\".", $0);
		args[numargs] = substr($0, argp);
		$0 = substr($0, 1, argp - 1);

		# what is left must be type
		# remove trailing space (if any)
		sub(/ $/, "");
		types[numargs] = $0;

		# We can do a maximum of 6 arguments to CTR*
		if (numargs <= 6) {
			if (numargs == 0)
				ctrstr = ctrstr "(" args[numargs];
			else
				ctrstr = ctrstr ", " args[numargs];
			if (types[numargs] ~ /\*/)
				ctrstr = ctrstr " 0x%lX";
			else
				ctrstr = ctrstr " %ld";
		}
	}
	if (numargs > 6)
		ctrargs = 6;
	else
		ctrargs = numargs;
	ctrstr = "\tCTR" ctrargs "(KTR_VOP, " ctrstr ")\"";
	for (i = 0; i < ctrargs; ++i)
		ctrstr = ctrstr ", " args[i];
	ctrstr = ctrstr ");";

	if (hfile) {
		# Print out the vop_F_args structure.
		printh("struct "name"_args {\n\tstruct vnodeop_desc *a_desc;");
		for (i = 0; i < numargs; ++i)
			printh("\t" t_spc(types[i]) "a_" args[i] ";");
		printh("};");

		# Print out extern declaration.
		printh("extern struct vnodeop_desc " name "_desc;");

		# Print out function.
		printh("static __inline int " uname "(");
		for (i = 0; i < numargs; ++i) {
			printh("\t" t_spc(types[i]) args[i] \
			    (i < numargs - 1 ? "," : ")"));
		}
		printh("{\n\tstruct " name "_args a;");
		printh("\tint rc;");
		printh("\ta.a_desc = VDESC(" name ");");
		for (i = 0; i < numargs; ++i)
			printh("\ta.a_" args[i] " = " args[i] ";");
		for (i = 0; i < numargs; ++i)
			add_debug_code(name, args[i], "Entry");
		add_debug_pre(name);
		printh("\trc = VCALL(" args[0] ", VOFFSET(" name "), &a);");
		printh(ctrstr);
		printh("if (rc == 0) {");
		for (i = 0; i < numargs; ++i)
			add_debug_code(name, args[i], "OK");
		printh("} else {");
		for (i = 0; i < numargs; ++i)
			add_debug_code(name, args[i], "Error");
		printh("}");
		add_debug_post(name);
		printh("\treturn (rc);\n}");
	}

	if (cfile) {
		# Print out the vop_F_vp_offsets structure.  This all depends
		# on naming conventions and nothing else.
		printc("static int " name "_vp_offsets[] = {");
		# as a side effect, figure out the releflags
		releflags = "";
		vpnum = 0;
		for (i = 0; i < numargs; i++) {
			if (types[i] == "struct vnode *") {
				printc("\tVOPARG_OFFSETOF(struct " name \
				    "_args,a_" args[i] "),");
				if (reles[i] == "WILLRELE") {
					releflags = releflags \
					    "|VDESC_VP" vpnum "_WILLRELE";
				}
				vpnum++;
			}
		}

		sub(/^\|/, "", releflags);
		printc("\tVDESC_NO_OFFSET");
		printc("};");

		# Print out the vnodeop_desc structure.
		printc("struct vnodeop_desc " name "_desc = {");
		# offset
		printc("\t0,");
		# printable name
		printc("\t\"" name "\",");
		# flags
		vppwillrele = "";
		for (i = 0; i < numargs; i++) {
			if (types[i] == "struct vnode **" && \
			    reles[i] == "WILLRELE") {
				vppwillrele = "|VDESC_VPP_WILLRELE";
			}
		}

		if (!releflags)
			releflags = "0";
		printc("\t" releflags vppwillrele ",");

		# vp offsets
		printc("\t" name "_vp_offsets,");
		# vpp (if any)
		printc("\t" find_arg_with_type("struct vnode **") ",");
		# cred (if any)
		printc("\t" find_arg_with_type("struct ucred *") ",");
		# thread (if any)
		printc("\t" find_arg_with_type("struct thread *") ",");
		# componentname
		printc("\t" find_arg_with_type("struct componentname *") ",");
		# transport layer information
		printc("\tNULL,\n};\n");
	}
}
 
if (hfile)
	close(hfile);
if (cfile)
	close(cfile);
close(srcfile);

exit 0;

}
