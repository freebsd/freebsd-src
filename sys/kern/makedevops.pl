#!/usr/bin/perl -w
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
#	From @(#)vnode_if.sh	8.1 (Berkeley) 6/10/93
#	From Id: makedevops.sh,v 1.1 1998/06/14 13:53:12 dfr Exp
#	$Id$
#

use strict;
use IO::Handle;
use IO::File;
use Getopt::Std;

sub main {
    my (%opts) = ('c' => 0, 'h' => 0);
    my ($src, $cname, $hname, $tmp, $intname);

    getopts('ch', \%opts);
    if (!($opts{'c'} or $opts{'h'}) or $#ARGV != 0) {
	&usage();
    }

    $cname = $ARGV[0];
    $cname =~ s,^.*/([^/]+)$,$1,;
    $hname = $cname;
    $cname =~ s/\.m$/.c/;
    $hname =~ s/\.m$/.h/;

    $src = new IO::File "<$ARGV[0]";
    defined($src)
	or die "$0: $ARGV[0]: $!\n";

    $tmp = IO::File->new_tmpfile();
    defined($tmp)
	or die "$0: creating temporary file: $!\n";

    write_header($tmp, $ARGV[0]);
    if ($opts{'c'}) {
	print $tmp ("#include <sys/param.h>\n",
		    "#include <sys/queue.h>\n",
		    "#include <sys/bus_private.h>\n");
    }

line:
    while (<$src>) {
	chomp;

	if (/^\#\s*(if)|(else)|(elif)|(endif)|(include)/) {
	    if ($opts{'c'}) {
		print $tmp ($_, "\n");
	    }
	    next line;
	}

	s/\#.*$//;		# strip comments
	next line if (/^\s*$/);

	if (/^\s*INTERFACE\s+(\w+)\s*$/) {
	    $intname = $1;
	    if ($opts{'c'}) {
		print $tmp "#include \"$hname\"\n\n";
	    } else {
		print $tmp ("#ifndef _", $intname, "_if_h_\n",
			    "#define _", $intname, "_if_h_\n\n");
	    }
	    next line;
	}
	if (/^\s*METHOD\s+([a-zA-Z_0-9 	*]+)\s+(\w+)\s*\{/) {
	    my ($ret, $name) = ($1, $2);
	    my (@args, $mname, $umname);

	    # Get the function arguments.
	    @args = ();
arg:
	    while (<$src>) {
		chomp;
		s/\#.*$//;	# delete comments
		last arg if (/^\s*\}\s*;/);
		s/^\s+//;	# strip leading whitespace
		s/\s+$//;	# strip trailing whitespace
		s/;$//;		# strip trailing semicolon
		s/\s+/ /g;	# squish internal whitespace to a single space
		push(@args, $_);
	    }

	    $mname = $intname . '_' . $name;
	    $umname = uc $mname;

	    # Print out the method declaration
	    if ($opts{'h'}) {
		print $tmp ("extern struct device_op_desc ", $mname,
			    "_desc;\n",
			    $ret, ' ', $umname, "(",
			    join(", ", @args),
			    ");\n");
	    }

	    if ($opts{'c'}) {
		# Print the method desc
		print $tmp ("struct device_op_desc ", $mname, "_desc = {\n",
			    "\t0,\n",
			    "\t\"$mname\"\n",
			    "};\n\n");

		# Print out the method typedef
		print $tmp ("typedef ", $ret, ' ', $mname, "_t (",
			    join(", ", @args), ");\n");

		# Print out the method itself
		print $tmp ($ret, ' ', $umname, " (",
			    join(", ", @args), ")\n",
			    "{\n",
			    "\t", $mname, "_t *m = (", $mname, "_t *)",
			    "DEVOPMETH(dev, ", $mname, ");\n");
		if ($ret eq 'void') {
		    print $tmp "\tm(";
		} else {
		    print $tmp "\treturn m(";
		}
		print $tmp join(", ", map {&argname($_)} @args);
		print $tmp ");\n}\n\n";
	    }
	    next line;
	}
	# should diagnose unrecognized input here
    }

    if ($opts{'h'}) {
	print $tmp ("\n#endif /* _", $intname, "_if_h_ */\n");
    }

    compare_and_update($tmp, $opts{'c'} ? $cname : $hname);
    exit 0;
}

&main;

sub argname {
    my ($arg) = @_;
    my (@words) = split(/\s+/, $arg);
    my ($name) = pop @words;
    
    $name =~ s/^\*+//;
    return $name;
}

sub compare_and_update {
    my ($oldfh, $newname) = @_;
    my ($data1, $data2, $newfh);

    defined($oldfh->seek(0, 0))
	or die "$0: seek: $!\n";
    $oldfh->input_record_separator(undef);
    $data1 = <$oldfh>;

    $newfh = new IO::File "<$newname";
    if (defined($newfh)) {
	$newfh->input_record_separator(undef);
	$data2 = <$newfh>;
	undef $newfh;
    }
    if (defined($data2) && $data1 eq $data2) {
	printf STDERR "$0: $newname: unchanged\n";
	return 0;
    }

    $newfh = new IO::File ">$newname";
    die "$0: $newname: $!\n"
	unless(defined $newfh);
    print $newfh $data1;
    return 0;
}

sub write_header {
    my ($out, $in) = @_;

    print $out <<EOH;
/*
 * This file is produced automatically.
 * Do not modify anything in here by hand.
 *
 * Created from $in with makedevops.pl
 */

EOH
}

sub usage {
    print STDERR "$0: usage:\n\t$0 -c infile\n\t$0 -h infile\n";
    exit 1;
}
