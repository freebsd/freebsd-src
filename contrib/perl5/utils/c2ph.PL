#!/usr/local/bin/perl

use Config;
use File::Basename qw(&basename &dirname);
use Cwd;

# List explicitly here the variables you want Configure to
# generate.  Metaconfig only looks for shell variables, so you
# have to mention them as if they were shell variables, not
# %Config entries.  Thus you write
#  $startperl
# to ensure Configure will look for $Config{startperl}.

# This forces PL files to create target in same directory as PL file.
# This is so that make depend always knows where to find PL derivatives.
$origdir = cwd;
chdir dirname($0);
$file = basename($0, '.PL');
$file .= '.com' if $^O eq 'VMS';

open OUT,">$file" or die "Can't create $file: $!";

print "Extracting $file (with variable substitutions)\n";

# In this section, perl variables will be expanded during extraction.
# You can use $Config{...} to use Configure variables.

print OUT <<"!GROK!THIS!";
$Config{startperl}
    eval 'exec $Config{perlpath} -S \$0 \${1+"\$@"}'
	if \$running_under_some_shell;
!GROK!THIS!

# In the following, perl variables are not expanded during extraction.

print OUT <<'!NO!SUBS!';
#
#
#   c2ph (aka pstruct)
#   Tom Christiansen, <tchrist@convex.com>
#
#   As pstruct, dump C structures as generated from 'cc -g -S' stabs.
#   As c2ph, do this PLUS generate perl code for getting at the structures.
#
#   See the usage message for more.  If this isn't enough, read the code.
#

=head1 NAME

c2ph, pstruct - Dump C structures as generated from C<cc -g -S> stabs

=head1 SYNOPSIS

    c2ph [-dpnP] [var=val] [files ...]

=head2 OPTIONS

    Options:

    -w	wide; short for: type_width=45 member_width=35 offset_width=8
    -x	hex; short for:  offset_fmt=x offset_width=08 size_fmt=x size_width=04

    -n	do not generate perl code  (default when invoked as pstruct)
    -p	generate perl code         (default when invoked as c2ph)
    -v	generate perl code, with C decls as comments

    -i	do NOT recompute sizes for intrinsic datatypes
    -a	dump information on intrinsics also

    -t	trace execution
    -d	spew reams of debugging output

    -slist  give comma-separated list a structures to dump

=head1 DESCRIPTION

The following is the old c2ph.doc documentation by Tom Christiansen
<tchrist@perl.com>
Date: 25 Jul 91 08:10:21 GMT

Once upon a time, I wrote a program called pstruct.  It was a perl
program that tried to parse out C structures and display their member
offsets for you.  This was especially useful for people looking at
binary dumps or poking around the kernel.

Pstruct was not a pretty program.  Neither was it particularly robust.
The problem, you see, was that the C compiler was much better at parsing
C than I could ever hope to be.

So I got smart:  I decided to be lazy and let the C compiler parse the C,
which would spit out debugger stabs for me to read.  These were much
easier to parse.  It's still not a pretty program, but at least it's more
robust.

Pstruct takes any .c or .h files, or preferably .s ones, since that's
the format it is going to massage them into anyway, and spits out
listings like this:

 struct tty {
   int                          tty.t_locker                         000      4
   int                          tty.t_mutex_index                    004      4
   struct tty *                 tty.t_tp_virt                        008      4
   struct clist                 tty.t_rawq                           00c     20
     int                        tty.t_rawq.c_cc                      00c      4
     int                        tty.t_rawq.c_cmax                    010      4
     int                        tty.t_rawq.c_cfx                     014      4
     int                        tty.t_rawq.c_clx                     018      4
     struct tty *               tty.t_rawq.c_tp_cpu                  01c      4
     struct tty *               tty.t_rawq.c_tp_iop                  020      4
     unsigned char *            tty.t_rawq.c_buf_cpu                 024      4
     unsigned char *            tty.t_rawq.c_buf_iop                 028      4
   struct clist                 tty.t_canq                           02c     20
     int                        tty.t_canq.c_cc                      02c      4
     int                        tty.t_canq.c_cmax                    030      4
     int                        tty.t_canq.c_cfx                     034      4
     int                        tty.t_canq.c_clx                     038      4
     struct tty *               tty.t_canq.c_tp_cpu                  03c      4
     struct tty *               tty.t_canq.c_tp_iop                  040      4
     unsigned char *            tty.t_canq.c_buf_cpu                 044      4
     unsigned char *            tty.t_canq.c_buf_iop                 048      4
   struct clist                 tty.t_outq                           04c     20
     int                        tty.t_outq.c_cc                      04c      4
     int                        tty.t_outq.c_cmax                    050      4
     int                        tty.t_outq.c_cfx                     054      4
     int                        tty.t_outq.c_clx                     058      4
     struct tty *               tty.t_outq.c_tp_cpu                  05c      4
     struct tty *               tty.t_outq.c_tp_iop                  060      4
     unsigned char *            tty.t_outq.c_buf_cpu                 064      4
     unsigned char *            tty.t_outq.c_buf_iop                 068      4
   (*int)()                     tty.t_oproc_cpu                      06c      4
   (*int)()                     tty.t_oproc_iop                      070      4
   (*int)()                     tty.t_stopproc_cpu                   074      4
   (*int)()                     tty.t_stopproc_iop                   078      4
   struct thread *              tty.t_rsel                           07c      4

etc.


Actually, this was generated by a particular set of options.  You can control
the formatting of each column, whether you prefer wide or fat, hex or decimal,
leading zeroes or whatever.

All you need to be able to use this is a C compiler than generates
BSD/GCC-style stabs.  The B<-g> option on native BSD compilers and GCC
should get this for you.

To learn more, just type a bogus option, like B<-\?>, and a long usage message
will be provided.  There are a fair number of possibilities.

If you're only a C programmer, than this is the end of the message for you.
You can quit right now, and if you care to, save off the source and run it
when you feel like it.  Or not.



But if you're a perl programmer, then for you I have something much more
wondrous than just a structure offset printer.

You see, if you call pstruct by its other incybernation, c2ph, you have a code
generator that translates C code into perl code!  Well, structure and union
declarations at least, but that's quite a bit.

Prior to this point, anyone programming in perl who wanted to interact
with C programs, like the kernel, was forced to guess the layouts of
the C strutures, and then hardwire these into his program.  Of course,
when you took your wonderfully crafted program to a system where the
sgtty structure was laid out differently, you program broke.  Which is
a shame.

We've had Larry's h2ph translator, which helped, but that only works on
cpp symbols, not real C, which was also very much needed.  What I offer
you is a symbolic way of getting at all the C structures.  I've couched
them in terms of packages and functions.  Consider the following program:

    #!/usr/local/bin/perl

    require 'syscall.ph';
    require 'sys/time.ph';
    require 'sys/resource.ph';

    $ru = "\0" x &rusage'sizeof();

    syscall(&SYS_getrusage, &RUSAGE_SELF, $ru)      && die "getrusage: $!";

    @ru = unpack($t = &rusage'typedef(), $ru);

    $utime =  $ru[ &rusage'ru_utime + &timeval'tv_sec  ]
	   + ($ru[ &rusage'ru_utime + &timeval'tv_usec ]) / 1e6;

    $stime =  $ru[ &rusage'ru_stime + &timeval'tv_sec  ]
	   + ($ru[ &rusage'ru_stime + &timeval'tv_usec ]) / 1e6;

    printf "you have used %8.3fs+%8.3fu seconds.\n", $utime, $stime;


As you see, the name of the package is the name of the structure.  Regular
fields are just their own names.  Plus the following accessor functions are
provided for your convenience:

    struct	This takes no arguments, and is merely the number of first-level
		elements in the structure.  You would use this for indexing
		into arrays of structures, perhaps like this


		    $usec = $u[ &user'u_utimer
				+ (&ITIMER_VIRTUAL * &itimerval'struct)
				+ &itimerval'it_value
				+ &timeval'tv_usec
			      ];

    sizeof   	Returns the bytes in the structure, or the member if
	     	you pass it an argument, such as

			&rusage'sizeof(&rusage'ru_utime)

    typedef  	This is the perl format definition for passing to pack and
	     	unpack.  If you ask for the typedef of a nothing, you get
	     	the whole structure, otherwise you get that of the member
	     	you ask for.  Padding is taken care of, as is the magic to
	     	guarantee that a union is unpacked into all its aliases.
	     	Bitfields are not quite yet supported however.

    offsetof	This function is the byte offset into the array of that
		member.  You may wish to use this for indexing directly
		into the packed structure with vec() if you're too lazy
		to unpack it.

    typeof	Not to be confused with the typedef accessor function, this
		one returns the C type of that field.  This would allow
		you to print out a nice structured pretty print of some
		structure without knoning anything about it beforehand.
		No args to this one is a noop.  Someday I'll post such
		a thing to dump out your u structure for you.


The way I see this being used is like basically this:

	% h2ph <some_include_file.h  >  /usr/lib/perl/tmp.ph
	% c2ph  some_include_file.h  >> /usr/lib/perl/tmp.ph
	% install

It's a little tricker with c2ph because you have to get the includes right.
I can't know this for your system, but it's not usually too terribly difficult.

The code isn't pretty as I mentioned  -- I never thought it would be a 1000-
line program when I started, or I might not have begun. :-)  But I would have
been less cavalier in how the parts of the program communicated with each
other, etc.  It might also have helped if I didn't have to divine the makeup
of the stabs on the fly, and then account for micro differences between my
compiler and gcc.

Anyway, here it is.  Should run on perl v4 or greater.  Maybe less.


 --tom

=cut

$RCSID = '$Id: c2ph,v 1.7 95/10/28 10:41:47 tchrist Exp Locker: tchrist $';


######################################################################

# some handy data definitions.   many of these can be reset later.

$bitorder = 'b';  # ascending; set to B for descending bit fields

%intrinsics =
%template = (
    'char', 			'c',
    'unsigned char', 		'C',
    'short',			's',
    'short int',		's',
    'unsigned short',		'S',
    'unsigned short int',	'S',
    'short unsigned int',	'S',
    'int',			'i',
    'unsigned int',		'I',
    'long',			'l',
    'long int',			'l',
    'unsigned long',		'L',
    'unsigned long',		'L',
    'long unsigned int',	'L',
    'unsigned long int',	'L',
    'long long',		'q',
    'long long int',		'q',
    'unsigned long long',	'Q',
    'unsigned long long int',	'Q',
    'float',			'f',
    'double',			'd',
    'pointer',			'p',
    'null',			'x',
    'neganull',			'X',
    'bit',			$bitorder,
);

&buildscrunchlist;
delete $intrinsics{'neganull'};
delete $intrinsics{'bit'};
delete $intrinsics{'null'};

# use -s to recompute sizes
%sizeof = (
    'char', 			'1',
    'unsigned char', 		'1',
    'short',			'2',
    'short int',		'2',
    'unsigned short',		'2',
    'unsigned short int',	'2',
    'short unsigned int',	'2',
    'int',			'4',
    'unsigned int',		'4',
    'long',			'4',
    'long int',			'4',
    'unsigned long',		'4',
    'unsigned long int',	'4',
    'long unsigned int',	'4',
    'long long',		'8',
    'long long int',		'8',
    'unsigned long long',	'8',
    'unsigned long long int',	'8',
    'float',			'4',
    'double',			'8',
    'pointer',			'4',
);

($type_width, $member_width, $offset_width, $size_width) = (20, 20, 6, 5);

($offset_fmt, $size_fmt) = ('d', 'd');

$indent = 2;

$CC = 'cc';
$CFLAGS = '-g -S';
$DEFINES = '';

$perl++ if $0 =~ m#/?c2ph$#;

require 'getopts.pl';

eval '$'.$1.'$2;' while $ARGV[0] =~ /^([A-Za-z_]+=)(.*)/ && shift;

&Getopts('aixdpvtnws:') || &usage(0);

$opt_d && $debug++;
$opt_t && $trace++;
$opt_p && $perl++;
$opt_v && $verbose++;
$opt_n && ($perl = 0);

if ($opt_w) {
    ($type_width, $member_width, $offset_width) = (45, 35, 8);
}
if ($opt_x) {
    ($offset_fmt, $offset_width, $size_fmt, $size_width) = ( 'x', '08', 'x', 04 );
}

eval '$'.$1.'$2;' while $ARGV[0] =~ /^([A-Za-z_]+=)(.*)/ && shift;

sub PLUMBER {
    select(STDERR);
    print "oops, apperent pager foulup\n";
    $isatty++;
    &usage(1);
}

sub usage {
    local($oops) = @_;
    unless (-t STDOUT) {
	select(STDERR);
    } elsif (!$oops) {
	$isatty++;
	$| = 1;
	print "hit <RETURN> for further explanation: ";
	<STDIN>;
	open (PIPE, "|". ($ENV{PAGER} || 'more'));
	$SIG{PIPE} = PLUMBER;
	select(PIPE);
    }

    print "usage: $0 [-dpnP] [var=val] [files ...]\n";

    exit unless $isatty;

    print <<EOF;

Options:

-w	wide; short for: type_width=45 member_width=35 offset_width=8
-x	hex; short for:  offset_fmt=x offset_width=08 size_fmt=x size_width=04

-n  	do not generate perl code  (default when invoked as pstruct)
-p  	generate perl code         (default when invoked as c2ph)
-v	generate perl code, with C decls as comments

-i	do NOT recompute sizes for intrinsic datatypes
-a	dump information on intrinsics also

-t 	trace execution
-d	spew reams of debugging output

-slist  give comma-separated list a structures to dump


Var Name        Default Value    Meaning

EOF

    &defvar('CC', 'which_compiler to call');
    &defvar('CFLAGS', 'how to generate *.s files with stabs');
    &defvar('DEFINES','any extra cflags or cpp defines, like -I, -D, -U');

    print "\n";

    &defvar('type_width', 'width of type field   (column 1)');
    &defvar('member_width', 'width of member field (column 2)');
    &defvar('offset_width', 'width of offset field (column 3)');
    &defvar('size_width', 'width of size field   (column 4)');

    print "\n";

    &defvar('offset_fmt', 'sprintf format type for offset');
    &defvar('size_fmt', 'sprintf format type for size');

    print "\n";

    &defvar('indent', 'how far to indent each nesting level');

   print <<'EOF';

    If any *.[ch] files are given, these will be catted together into
    a temporary *.c file and sent through:
	    $CC $CFLAGS $DEFINES
    and the resulting *.s groped for stab information.  If no files are
    supplied, then stdin is read directly with the assumption that it
    contains stab information.  All other liens will be ignored.  At
    most one *.s file should be supplied.

EOF
    close PIPE;
    exit 1;
}

sub defvar {
    local($var, $msg) = @_;
    printf "%-16s%-15s  %s\n", $var, eval "\$$var", $msg;
}

$recurse = 1;

if (@ARGV) {
    if (grep(!/\.[csh]$/,@ARGV)) {
	warn "Only *.[csh] files expected!\n";
	&usage;
    }
    elsif (grep(/\.s$/,@ARGV)) {
	if (@ARGV > 1) {
	    warn "Only one *.s file allowed!\n";
	    &usage;
	}
    }
    elsif (@ARGV == 1 && $ARGV[0] =~ /\.c$/) {
	local($dir, $file) = $ARGV[0] =~ m#(.*/)?(.*)$#;
	$chdir = "cd $dir; " if $dir;
	&system("$chdir$CC $CFLAGS $DEFINES $file") && exit 1;
	$ARGV[0] =~ s/\.c$/.s/;
    }
    else {
	$TMP = "/tmp/c2ph.$$.c";
	&system("cat @ARGV > $TMP") && exit 1;
	&system("cd /tmp; $CC $CFLAGS $DEFINES $TMP") && exit 1;
	unlink $TMP;
	$TMP =~ s/\.c$/.s/;
	@ARGV = ($TMP);
    }
}

if ($opt_s) {
    for (split(/[\s,]+/, $opt_s)) {
	$interested{$_}++;
    }
}


$| = 1 if $debug;

main: {

    if ($trace) {
	if (-t && !@ARGV) {
	    print STDERR "reading from your keyboard: ";
	} else {
	    print STDERR "reading from " . (@ARGV ? "@ARGV" : "<STDIN>").": ";
	}
    }

STAB: while (<>) {
	if ($trace && !($. % 10)) {
	    $lineno = $..'';
	    print STDERR $lineno, "\b" x length($lineno);
	}
	next unless /^\s*\.stabs\s+/;
	$line = $_;
	s/^\s*\.stabs\s+//;
	if (s/\\\\"[d,]+$//) {
	    $saveline .= $line;
	    $savebar  = $_;
	    next STAB;
	}
	if ($saveline) {
	    s/^"//;
	    $_ = $savebar . $_;
	    $line = $saveline;
	}
	&stab;
	$savebar = $saveline = undef;
    }
    print STDERR "$.\n" if $trace;
    unlink $TMP if $TMP;

    &compute_intrinsics if $perl && !$opt_i;

    print STDERR "resolving types\n" if $trace;

    &resolve_types;
    &adjust_start_addrs;

    $sum = 2 + $type_width + $member_width;
    $pmask1 = "%-${type_width}s %-${member_width}s";
    $pmask2 = "%-${sum}s %${offset_width}${offset_fmt}%s %${size_width}${size_fmt}%s";



    if ($perl) {
	# resolve template -- should be in stab define order, but even this isn't enough.
	print STDERR "\nbuilding type templates: " if $trace;
	for $i (reverse 0..$#type) {
	    next unless defined($name = $type[$i]);
	    next unless defined $struct{$name};
	    ($iname = $name) =~ s/\..*//;
	    $build_recursed = 0;
	    &build_template($name) unless defined $template{&psou($name)} ||
					$opt_s && !$interested{$iname};
	}
	print STDERR "\n\n" if $trace;
    }

    print STDERR "dumping structs: " if $trace;

    local($iam);



    foreach $name (sort keys %struct) {
	($iname = $name) =~ s/\..*//;
	next if $opt_s && !$interested{$iname};
	print STDERR "$name " if $trace;

	undef @sizeof;
	undef @typedef;
	undef @offsetof;
	undef @indices;
	undef @typeof;
	undef @fieldnames;

	$mname = &munge($name);

	$fname = &psou($name);

	print "# " if $perl && $verbose;
	$pcode = '';
	print "$fname {\n" if !$perl || $verbose;
	$template{$fname} = &scrunch($template{$fname}) if $perl;
	&pstruct($name,$name,0);
	print "# " if $perl && $verbose;
	print "}\n" if !$perl || $verbose;
	print "\n" if $perl && $verbose;

	if ($perl) {
	    print "$pcode";

	    printf("\nsub %-32s { %4d; }\n\n", "${mname}'struct", $countof{$name});

	    print <<EOF;
sub ${mname}'typedef {
    local(\$${mname}'index) = shift;
    defined \$${mname}'index
	? \$${mname}'typedef[\$${mname}'index]
	: \$${mname}'typedef;
}
EOF

	    print <<EOF;
sub ${mname}'sizeof {
    local(\$${mname}'index) = shift;
    defined \$${mname}'index
	? \$${mname}'sizeof[\$${mname}'index]
	: \$${mname}'sizeof;
}
EOF

	    print <<EOF;
sub ${mname}'offsetof {
    local(\$${mname}'index) = shift;
    defined \$${mname}index
	? \$${mname}'offsetof[\$${mname}'index]
	: \$${mname}'sizeof;
}
EOF

	    print <<EOF;
sub ${mname}'typeof {
    local(\$${mname}'index) = shift;
    defined \$${mname}index
	? \$${mname}'typeof[\$${mname}'index]
	: '$name';
}
EOF

	    print <<EOF;
sub ${mname}'fieldnames {
    \@${mname}'fieldnames;
}
EOF

	$iam = ($isastruct{$name} && 's') || ($isaunion{$name} && 'u');

	    print <<EOF;
sub ${mname}'isastruct {
    '$iam';
}
EOF

	    print "\$${mname}'typedef = '" . &scrunch($template{$fname})
		. "';\n";

	    print "\$${mname}'sizeof = $sizeof{$name};\n\n";


	    print "\@${mname}'indices = (", &squishseq(@indices), ");\n";

	    print "\n";

	    print "\@${mname}'typedef[\@${mname}'indices] = (",
			join("\n\t", '', @typedef), "\n    );\n\n";
	    print "\@${mname}'sizeof[\@${mname}'indices] = (",
			join("\n\t", '', @sizeof), "\n    );\n\n";
	    print "\@${mname}'offsetof[\@${mname}'indices] = (",
			join("\n\t", '', @offsetof), "\n    );\n\n";
	    print "\@${mname}'typeof[\@${mname}'indices] = (",
			join("\n\t", '', @typeof), "\n    );\n\n";
	    print "\@${mname}'fieldnames[\@${mname}'indices] = (",
			join("\n\t", '', @fieldnames), "\n    );\n\n";

	    $template_printed{$fname}++;
	    $size_printed{$fname}++;
	}
	print "\n";
    }

    print STDERR "\n" if $trace;

    unless ($perl && $opt_a) {
	print "\n1;\n" if $perl;
	exit;
    }



    foreach $name (sort bysizevalue keys %intrinsics) {
	next if $size_printed{$name};
	print '$',&munge($name),"'sizeof = ", $sizeof{$name}, ";\n";
    }

    print "\n";

    sub bysizevalue { $sizeof{$a} <=> $sizeof{$b}; }


    foreach $name (sort keys %intrinsics) {
	print '$',&munge($name),"'typedef = '", $template{$name}, "';\n";
    }

    print "\n1;\n" if $perl;

    exit;
}

########################################################################################


sub stab {
    next unless $continued || /:[\$\w]+(\(\d+,\d+\))?=[\*\$\w]+/;  # (\d+,\d+) is for sun
    s/"// 						|| next;
    s/",([x\d]+),([x\d]+),([x\d]+),.*// 		|| next;

    next if /^\s*$/;

    $size = $3 if $3;
    $_ = $continued . $_ if length($continued);
    if (s/\\\\$//) {
      # if last 2 chars of string are '\\' then stab is continued
      # in next stab entry
      chop;
      $continued = $_;
      next;
    }
    $continued = '';


    $line = $_;

    if (($name, $pdecl) = /^([\$ \w]+):[tT]((\d+)(=[rufs*](\d+))+)$/) {
	print "$name is a typedef for some funky pointers: $pdecl\n" if $debug;
	&pdecl($pdecl);
	next;
    }



    if (/(([ \w]+):t(\d+|\(\d+,\d+\)))=r?(\d+|\(\d+,\d+\))(;\d+;\d+;)?/) {
	local($ident) = $2;
	push(@intrinsics, $ident);
	$typeno = &typeno($3);
	$type[$typeno] = $ident;
	print STDERR "intrinsic $ident in new type $typeno\n" if $debug;
	next;
    }

    if (($name, $typeordef, $typeno, $extra, $struct, $_)
	= /^([\$ \w]+):([ustT])(\d+|\(\d+,\d+\))(=[rufs*](\d+))?(.*)$/)
    {
	$typeno = &typeno($typeno);  # sun foolery
    }
    elsif (/^[\$\w]+:/) {
	next; # variable
    }
    else {
	warn "can't grok stab: <$_> in: $line " if $_;
	next;
    }

    #warn "got size $size for $name\n";
    $sizeof{$name} = $size if $size;

    s/;[-\d]*;[-\d]*;$//;  # we don't care about ranges

    $typenos{$name} = $typeno;

    unless (defined $type[$typeno]) {
	&panic("type 0??") unless $typeno;
	$type[$typeno] = $name unless defined $type[$typeno];
	printf "new type $typeno is $name" if $debug;
	if ($extra =~ /\*/ && defined $type[$struct]) {
	    print ", a typedef for a pointer to " , $type[$struct] if $debug;
	}
    } else {
	printf "%s is type %d", $name, $typeno if $debug;
	print ", a typedef for " , $type[$typeno] if $debug;
    }
    print "\n" if $debug;
    #next unless $extra =~ /[su*]/;

    #$type[$struct] = $name;

    if ($extra =~ /[us*]/) {
	&sou($name, $extra);
	$_ = &sdecl($name, $_, 0);
    }
    elsif (/^=ar/) {
	print "it's a bare array typedef -- that's pretty sick\n" if $debug;
	$_ = "$typeno$_";
	$scripts = '';
	$_ = &adecl($_,1);

    }
    elsif (s/((\w+):t(\d+|\(\d+,\d+\)))?=r?(;\d+;\d+;)?//) {  # the ?'s are for gcc
	push(@intrinsics, $2);
	$typeno = &typeno($3);
	$type[$typeno] = $2;
	print STDERR "intrinsic $2 in new type $typeno\n" if $debug;
    }
    elsif (s/^=e//) { # blessed be thy compiler; mine won't do this
	&edecl;
    }
    else {
	warn "Funny remainder for $name on line $_ left in $line " if $_;
    }
}

sub typeno {  # sun thinks types are (0,27) instead of just 27
    local($_) = @_;
    s/\(\d+,(\d+)\)/$1/;
    $_;
}

sub pstruct {
    local($what,$prefix,$base) = @_;
    local($field, $fieldname, $typeno, $count, $offset, $entry);
    local($fieldtype);
    local($type, $tname);
    local($mytype, $mycount, $entry2);
    local($struct_count) = 0;
    local($pad, $revpad, $length, $prepad, $lastoffset, $lastlength, $fmt);
    local($bits,$bytes);
    local($template);


    local($mname) = &munge($name);

    sub munge {
	local($_) = @_;
	s/[\s\$\.]/_/g;
	$_;
    }

    local($sname) = &psou($what);

    $nesting++;

    for $field (split(/;/, $struct{$what})) {
	$pad = $prepad = 0;
	$entry = '';
	($fieldname, $typeno, $count, $offset, $length) = split(/,/, $field);

	$type = $type[$typeno];

	$type =~ /([^[]*)(\[.*\])?/;
	$mytype = $1;
	$count .= $2;
	$fieldtype = &psou($mytype);

	local($fname) = &psou($name);

	if ($build_templates) {

	    $pad = ($offset - ($lastoffset + $lastlength))/8
		if defined $lastoffset;

	    if (! $finished_template{$sname}) {
		if ($isaunion{$what}) {
		    $template{$sname} .= 'X' x $revpad . ' '    if $revpad;
		} else {
		    $template{$sname} .= 'x' x $pad    . ' '    if $pad;
		}
	    }

	    $template = &fetch_template($type);
	    &repeat_template($template,$count);

	    if (! $finished_template{$sname}) {
		$template{$sname} .= $template;
	    }

	    $revpad = $length/8 if $isaunion{$what};

	    ($lastoffset, $lastlength) = ($offset, $length);

	} else {
	    print '# ' if $perl && $verbose;
	    $entry = sprintf($pmask1,
			' ' x ($nesting * $indent) . $fieldtype,
			"$prefix.$fieldname" . $count);

	    $entry =~ s/(\*+)( )/$2$1/;

	    printf $pmask2,
		    $entry,
		    ($base+$offset)/8,
		    ($bits = ($base+$offset)%8) ? ".$bits" : "  ",
		    $length/8,
		    ($bits = $length % 8) ? ".$bits": ""
			if !$perl || $verbose;

	    if ($perl) {
		$template = &fetch_template($type);
		&repeat_template($template,$count);
	    }

	    if ($perl && $nesting == 1) {

		push(@sizeof, int($length/8) .",\t# $fieldname");
		push(@offsetof, int($offset/8) .",\t# $fieldname");
		local($little) = &scrunch($template);
		push(@typedef, "'$little', \t# $fieldname");
		$type =~ s/(struct|union) //;
		push(@typeof, "'$mytype" . ($count ? $count : '') .
		    "',\t# $fieldname");
		push(@fieldnames, "'$fieldname',");
	    }

	    print '  ', ' ' x $indent x $nesting, $template
				if $perl && $verbose;

	    print "\n" if !$perl || $verbose;

	}
	if ($perl) {
	    local($mycount) = defined $struct{$mytype} ? $countof{$mytype} : 1;
	    $mycount *= &scripts2count($count) if $count;
	    if ($nesting==1 && !$build_templates) {
		$pcode .= sprintf("sub %-32s { %4d; }\n",
			"${mname}'${fieldname}", $struct_count);
		push(@indices, $struct_count);
	    }
	    $struct_count += $mycount;
	}


	&pstruct($type, "$prefix.$fieldname", $base+$offset)
		if $recurse && defined $struct{$type};
    }

    $countof{$what} = $struct_count unless defined $countof{$whati};

    $template{$sname} .= '$' if $build_templates;
    $finished_template{$sname}++;

    if ($build_templates && !defined $sizeof{$name}) {
	local($fmt) = &scrunch($template{$sname});
	print STDERR "no size for $name, punting with $fmt..." if $debug;
	eval '$sizeof{$name} = length(pack($fmt, ()))';
	if ($@) {
	    chop $@;
	    warn "couldn't get size for \$name: $@";
	} else {
	    print STDERR $sizeof{$name}, "\n" if $debUg;
	}
    }

    --$nesting;
}


sub psize {
    local($me) = @_;
    local($amstruct) = $struct{$me} ?  'struct ' : '';

    print '$sizeof{\'', $amstruct, $me, '\'} = ';
    printf "%d;\n", $sizeof{$me};
}

sub pdecl {
    local($pdecl) = @_;
    local(@pdecls);
    local($tname);

    warn "pdecl: $pdecl\n" if $debug;

    $pdecl =~ s/\(\d+,(\d+)\)/$1/g;
    $pdecl =~ s/\*//g;
    @pdecls = split(/=/, $pdecl);
    $typeno = $pdecls[0];
    $tname = pop @pdecls;

    if ($tname =~ s/^f//) { $tname = "$tname&"; }
    #else { $tname = "$tname*"; }

    for (reverse @pdecls) {
	$tname  .= s/^f// ? "&" : "*";
	#$tname =~ s/^f(.*)/$1&/;
	print "type[$_] is $tname\n" if $debug;
	$type[$_] = $tname unless defined $type[$_];
    }
}



sub adecl {
    ($arraytype, $unknown, $lower, $upper) = ();
    #local($typeno);
    # global $typeno, @type
    local($_, $typedef) = @_;

    while (s/^((\d+|\(\d+,\d+\))=)?ar(\d+|\(\d+,\d+\));//) {
	($arraytype, $unknown) = ($2, $3);
	$arraytype = &typeno($arraytype);
	$unknown = &typeno($unknown);
	if (s/^(\d+);(\d+);//) {
	    ($lower, $upper) = ($1, $2);
	    $scripts .= '[' .  ($upper+1) . ']';
	} else {
	    warn "can't find array bounds: $_";
	}
    }
    if (s/^([(,)\d*f=]*),(\d+),(\d+);//) {
	($start, $length) = ($2, $3);
	$whatis = $1;
	if ($whatis =~ /^(\d+|\(\d+,\d+\))=/) {
	    $typeno = &typeno($1);
	    &pdecl($whatis);
	} else {
	    $typeno = &typeno($whatis);
	}
    } elsif (s/^(\d+)(=[*suf]\d*)//) {
	local($whatis) = $2;

	if ($whatis =~ /[f*]/) {
	    &pdecl($whatis);
	} elsif ($whatis =~ /[su]/) {  #
	    print "$prefix.$fieldname is an array$scripts anon structs; disgusting\n"
		if $debug;
	    #$type[$typeno] = $name unless defined $type[$typeno];
	    ##printf "new type $typeno is $name" if $debug;
	    $typeno = $1;
	    $type[$typeno] = "$prefix.$fieldname";
	    local($name) = $type[$typeno];
	    &sou($name, $whatis);
	    $_ = &sdecl($name, $_, $start+$offset);
	    1;
	    $start = $start{$name};
	    $offset = $sizeof{$name};
	    $length = $offset;
	} else {
	    warn "what's this? $whatis in $line ";
	}
    } elsif (/^\d+$/) {
	$typeno = $_;
    } else {
	warn "bad array stab: $_ in $line ";
	next STAB;
    }
    #local($wasdef) = defined($type[$typeno]) && $debug;
    #if ($typedef) {
	#print "redefining $type[$typeno] to " if $wasdef;
	#$type[$typeno] = "$whatis$scripts"; # unless defined $type[$typeno];
	#print "$type[$typeno]\n" if $wasdef;
    #} else {
	#$type[$arraytype] = $type[$typeno] unless defined $type[$arraytype];
    #}
    $type[$arraytype] = "$type[$typeno]$scripts" if defined $type[$typeno];
    print "type[$arraytype] is $type[$arraytype]\n" if $debug;
    print "$prefix.$fieldname is an array of $type[$arraytype]\n" if $debug;
    $_;
}



sub sdecl {
    local($prefix, $_, $offset) = @_;

    local($fieldname, $scripts, $type, $arraytype, $unknown,
    $whatis, $pdecl, $upper,$lower, $start,$length) = ();
    local($typeno,$sou);


SFIELD:
    while (/^([^;]+);/) {
	$scripts = '';
	warn "sdecl $_\n" if $debug;
	if (s/^([\$\w]+)://) {
	    $fieldname = $1;
	} elsif (s/(\d+)=([us])(\d+|\(\d+,\d+\))//) { #
	    $typeno = &typeno($1);
	    $type[$typeno] = "$prefix.$fieldname";
	    local($name) = "$prefix.$fieldname";
	    &sou($name,$2);
	    $_ = &sdecl("$prefix.$fieldname", $_, $start+$offset);
	    $start = $start{$name};
	    $offset += $sizeof{$name};
	    #print "done with anon, start is $start, offset is $offset\n";
	    #next SFIELD;
	} else  {
	    warn "weird field $_ of $line" if $debug;
	    next STAB;
	    #$fieldname = &gensym;
	    #$_ = &sdecl("$prefix.$fieldname", $_, $start+$offset);
	}

	if (/^(\d+|\(\d+,\d+\))=ar/) {
	    $_ = &adecl($_);
	}
	elsif (s/^(\d+|\(\d+,\d+\))?,(\d+),(\d+);//) {
          ($start, $length) =  ($2, $3);
          &panic("no length?") unless $length;
          $typeno = &typeno($1) if $1;
        }
        elsif (s/^(\d+)=xs\w+:,(\d+),(\d+);//) {
	    ($start, $length) =  ($2, $3);
	    &panic("no length?") unless $length;
	    $typeno = &typeno($1) if $1;
	}
	elsif (s/^((\d+|\(\d+,\d+\))(=[*f](\d+|\(\d+,\d+\)))+),(\d+),(\d+);//) {
	    ($pdecl, $start, $length) =  ($1,$5,$6);
	    &pdecl($pdecl);
	}
	elsif (s/(\d+)=([us])(\d+|\(\d+,\d+\))//) { # the dratted anon struct
	    ($typeno, $sou) = ($1, $2);
	    $typeno = &typeno($typeno);
	    if (defined($type[$typeno])) {
		warn "now how did we get type $1 in $fieldname of $line?";
	    } else {
		print "anon type $typeno is $prefix.$fieldname\n" if $debug;
		$type[$typeno] = "$prefix.$fieldname" unless defined $type[$typeno];
	    };
	    local($name) = "$prefix.$fieldname";
	    &sou($name,$sou);
	    print "anon ".($isastruct{$name}) ? "struct":"union"." for $prefix.$fieldname\n" if $debug;
	    $type[$typeno] = "$prefix.$fieldname";
	    $_ = &sdecl("$prefix.$fieldname", $_, $start+$offset);
	    $start = $start{$name};
	    $length = $sizeof{$name};
	}
	else {
	    warn "can't grok stab for $name ($_) in line $line ";
	    next STAB;
	}

	&panic("no length for $prefix.$fieldname") unless $length;
	$struct{$name} .= join(',', $fieldname, $typeno, $scripts, $start, $length) . ';';
    }
    if (s/;\d*,(\d+),(\d+);//) {
	local($start, $size) = ($1, $2);
	$sizeof{$prefix} = $size;
	print "start of $prefix is $start, size of $sizeof{$prefix}\n" if $debug;
	$start{$prefix} = $start;
    }
    $_;
}

sub edecl {
    s/;$//;
    $enum{$name} = $_;
    $_ = '';
}

sub resolve_types {
    local($sou);
    for $i (0 .. $#type) {
	next unless defined $type[$i];
	$_ = $type[$i];
	unless (/\d/) {
	    print "type[$i] $type[$i]\n" if $debug;
	    next;
	}
	print "type[$i] $_ ==> " if $debug;
	s/^(\d+)(\**)\&\*(\**)/"$2($3".&type($1) . ')()'/e;
	s/^(\d+)\&/&type($1)/e;
	s/^(\d+)/&type($1)/e;
	s/(\*+)([^*]+)(\*+)/$1$3$2/;
	s/\((\*+)(\w+)(\*+)\)/$3($1$2)/;
	s/^(\d+)([\*\[].*)/&type($1).$2/e;
	#s/(\d+)(\*|(\[[\[\]\d\*]+]\])+)/&type($1).$2/ge;
	$type[$i] = $_;
	print "$_\n" if $debug;
    }
}
sub type { &psou($type[$_[0]] || "<UNDEFINED>"); }

sub adjust_start_addrs {
    for (sort keys %start) {
	($basename = $_) =~ s/\.[^.]+$//;
	$start{$_} += $start{$basename};
	print "start: $_ @ $start{$_}\n" if $debug;
    }
}

sub sou {
    local($what, $_) = @_;
    /u/ && $isaunion{$what}++;
    /s/ && $isastruct{$what}++;
}

sub psou {
    local($what) = @_;
    local($prefix) = '';
    if ($isaunion{$what})  {
	$prefix = 'union ';
    } elsif ($isastruct{$what})  {
	$prefix = 'struct ';
    }
    $prefix . $what;
}

sub scrunch {
    local($_) = @_;

    return '' if $_ eq '';

    study;

    s/\$//g;
    s/  / /g;
    1 while s/(\w) \1/$1$1/g;

    # i wanna say this, but perl resists my efforts:
    #	   s/(\w)(\1+)/$2 . length($1)/ge;

    &quick_scrunch;

    s/ $//;

    $_;
}

sub buildscrunchlist {
    $scrunch_code = "sub quick_scrunch {\n";
    for (values %intrinsics) {
        $scrunch_code .= "\ts/(${_}{2,})/'$_' . length(\$1)/ge;\n";
    }
    $scrunch_code .= "}\n";
    print "$scrunch_code" if $debug;
    eval $scrunch_code;
    &panic("can't eval scrunch_code $@ \nscrunch_code") if $@;
}

sub fetch_template {
    local($mytype) = @_;
    local($fmt);
    local($count) = 1;

    &panic("why do you care?") unless $perl;

    if ($mytype =~ s/(\[\d+\])+$//) {
	$count .= $1;
    }

    if ($mytype =~ /\*/) {
	$fmt = $template{'pointer'};
    }
    elsif (defined $template{$mytype}) {
	$fmt = $template{$mytype};
    }
    elsif (defined $struct{$mytype}) {
	if (!defined $template{&psou($mytype)}) {
	    &build_template($mytype) unless $mytype eq $name;
	}
	elsif ($template{&psou($mytype)} !~ /\$$/) {
	    #warn "incomplete template for $mytype\n";
	}
	$fmt = $template{&psou($mytype)} || '?';
    }
    else {
	warn "unknown fmt for $mytype\n";
	$fmt = '?';
    }

    $fmt x $count . ' ';
}

sub compute_intrinsics {
    local($TMP) = "/tmp/c2ph-i.$$.c";
    open (TMP, ">$TMP") || die "can't open $TMP: $!";
    select(TMP);

    print STDERR "computing intrinsic sizes: " if $trace;

    undef %intrinsics;

    print <<'EOF';
main() {
    char *mask = "%d %s\n";
EOF

    for $type (@intrinsics) {
	next if !$type || $type eq 'void' || $type =~ /complex/; # sun stuff
	print <<"EOF";
    printf(mask,sizeof($type), "$type");
EOF
    }

    print <<'EOF';
    printf(mask,sizeof(char *), "pointer");
    exit(0);
}
EOF
    close TMP;

    select(STDOUT);
    open(PIPE, "cd /tmp && $CC $TMP && /tmp/a.out|");
    while (<PIPE>) {
	chop;
	split(' ',$_,2);;
	print "intrinsic $_[1] is size $_[0]\n" if $debug;
	$sizeof{$_[1]} = $_[0];
	$intrinsics{$_[1]} = $template{$_[0]};
    }
    close(PIPE) || die "couldn't read intrinsics!";
    unlink($TMP, '/tmp/a.out');
    print STDERR "done\n" if $trace;
}

sub scripts2count {
    local($_) = @_;

    s/^\[//;
    s/\]$//;
    s/\]\[/*/g;
    $_ = eval;
    &panic("$_: $@") if $@;
    $_;
}

sub system {
    print STDERR "@_\n" if $trace;
    system @_;
}

sub build_template {
    local($name) = @_;

    &panic("already got a template for $name") if defined $template{$name};

    local($build_templates) = 1;

    local($lparen) = '(' x $build_recursed;
    local($rparen) = ')' x $build_recursed;

    print STDERR "$lparen$name$rparen " if $trace;
    $build_recursed++;
    &pstruct($name,$name,0);
    print STDERR "TEMPLATE for $name is ", $template{&psou($name)}, "\n" if $debug;
    --$build_recursed;
}


sub panic {

    select(STDERR);

    print "\npanic: @_\n";

    exit 1 if $] <= 4.003;  # caller broken

    local($i,$_);
    local($p,$f,$l,$s,$h,$a,@a,@sub);
    for ($i = 0; ($p,$f,$l,$s,$h,$w) = caller($i); $i++) {
	@a = @DB'args;
	for (@a) {
	    if (/^StB\000/ && length($_) == length($_main{'_main'})) {
		$_ = sprintf("%s",$_);
	    }
	    else {
		s/'/\\'/g;
		s/([^\0]*)/'$1'/ unless /^-?[\d.]+$/;
		s/([\200-\377])/sprintf("M-%c",ord($1)&0177)/eg;
		s/([\0-\37\177])/sprintf("^%c",ord($1)^64)/eg;
	    }
	}
	$w = $w ? '@ = ' : '$ = ';
	$a = $h ? '(' . join(', ', @a) . ')' : '';
	push(@sub, "$w&$s$a from file $f line $l\n");
	last if $signal;
    }
    for ($i=0; $i <= $#sub; $i++) {
	last if $signal;
	print $sub[$i];
    }
    exit 1;
}

sub squishseq {
    local($num);
    local($last) = -1e8;
    local($string);
    local($seq) = '..';

    while (defined($num = shift)) {
        if ($num == ($last + 1)) {
            $string .= $seq unless $inseq++;
            $last = $num;
            next;
        } elsif ($inseq) {
            $string .= $last unless $last == -1e8;
        }

        $string .= ',' if defined $string;
        $string .= $num;
        $last = $num;
        $inseq = 0;
    }
    $string .= $last if $inseq && $last != -e18;
    $string;
}

sub repeat_template {
    #  local($template, $scripts) = @_;  have to change caller's values

    if ( $_[1] ) {
	local($ncount) = &scripts2count($_[1]);
	if ($_[0] =~ /^\s*c\s*$/i) {
	    $_[0] = "A$ncount ";
	    $_[1] = '';
	} else {
	    $_[0] = $template x $ncount;
	}
    }
}
!NO!SUBS!

close OUT or die "Can't close $file: $!";
chmod 0755, $file or die "Can't reset permissions for $file: $!\n";
unlink 'pstruct';
print "Linking c2ph to pstruct.\n";
if (defined $Config{d_link}) {
  link 'c2ph', 'pstruct';
} else {
  unshift @INC, '../lib';
  require File::Copy;
  File::Copy::syscopy('c2ph', 'pstruct');
}
exec("$Config{'eunicefix'} $file") if $Config{'eunicefix'} ne ':';
chdir $origdir;
