package DB;

# modified Perl debugger, to be run from Emacs in perldb-mode
# Ray Lischner (uunet!mntgfx!lisch) as of 5 Nov 1990

$header = '$Header: /pub/FreeBSD/FreeBSD-CVS/src/gnu/usr.bin/perl/emacs/perldb.pl,v 1.1.1.1 1994/09/10 06:27:50 gclarkii Exp $';
#
# This file is automatically included if you do perl -d.
# It's probably not useful to include this yourself.
#
# Perl supplies the values for @line and %sub.  It effectively inserts
# a do DB'DB(<linenum>); in front of every place that can
# have a breakpoint.  It also inserts a do 'perldb.pl' before the first line.
#
# $Log: perldb.pl,v $
# Revision 1.1.1.1  1994/09/10  06:27:50  gclarkii
# Initial import of Perl 4.046 bmaked
#
# Revision 1.1.1.1  1993/08/23  21:29:46  nate
# PERL!
#
# Revision 4.0  91/03/20  01:18:58  lwall
# 4.0 baseline.
# 
# Revision 3.0.1.6  91/01/11  18:08:58  lwall
# patch42: @_ couldn't be accessed from debugger
# 
# Revision 3.0.1.5  90/11/10  01:40:26  lwall
# patch38: the debugger wouldn't stop correctly or do action routines
# 
# Revision 3.0.1.4  90/10/15  17:40:38  lwall
# patch29: added caller
# patch29: the debugger now understands packages and evals
# patch29: scripts now run at almost full speed under the debugger
# patch29: more variables are settable from debugger
# 
# Revision 3.0.1.3  90/08/09  04:00:58  lwall
# patch19: debugger now allows continuation lines
# patch19: debugger can now dump lists of variables
# patch19: debugger can now add aliases easily from prompt
# 
# Revision 3.0.1.2  90/03/12  16:39:39  lwall
# patch13: perl -d didn't format stack traces of *foo right
# patch13: perl -d wiped out scalar return values of subroutines
# 
# Revision 3.0.1.1  89/10/26  23:14:02  lwall
# patch1: RCS expanded an unintended $Header in lib/perldb.pl
# 
# Revision 3.0  89/10/18  15:19:46  lwall
# 3.0 baseline
# 
# Revision 2.0  88/06/05  00:09:45  root
# Baseline version 2.0.
# 
#

open(IN, "</dev/tty") || open(IN,  "<&STDIN");	# so we don't dingle stdin
open(OUT,">/dev/tty") || open(OUT, ">&STDOUT");	# so we don't dongle stdout
select(OUT);
$| = 1;				# for DB'OUT
select(STDOUT);
$| = 1;				# for real STDOUT
$sub = '';

# Is Perl being run from Emacs?
$emacs = $main'ARGV[$[] eq '-emacs';
shift(@main'ARGV) if $emacs;

$header =~ s/.Header: ([^,]+),v(\s+\S+\s+\S+).*$/$1$2/;
print OUT "\nLoading DB routines from $header\n\nEnter h for help.\n\n";

sub DB {
    &save;
    ($package, $filename, $line) = caller;
    $usercontext = '($@, $!, $[, $,, $/, $\) = @saved;' .
	"package $package;";		# this won't let them modify, alas
    local(*dbline) = "_<$filename";
    $max = $#dbline;
    if (($stop,$action) = split(/\0/,$dbline{$line})) {
	if ($stop eq '1') {
	    $signal |= 1;
	}
	else {
	    $evalarg = "\$DB'signal |= do {$stop;}"; &eval;
	    $dbline{$line} =~ s/;9($|\0)/$1/;
	}
    }
    if ($single || $trace || $signal) {
	if ($emacs) {
	    print OUT "\032\032$filename:$line:0\n";
	} else {
	    print OUT "$package'" unless $sub =~ /'/;
	    print OUT "$sub($filename:$line):\t",$dbline[$line];
	    for ($i = $line + 1; $i <= $max && $dbline[$i] == 0; ++$i) {
		last if $dbline[$i] =~ /^\s*(}|#|\n)/;
		print OUT "$sub($filename:$i):\t",$dbline[$i];
	    }
	}
    }
    $evalarg = $action, &eval if $action;
    if ($single || $signal) {
	$evalarg = $pre, &eval if $pre;
	print OUT $#stack . " levels deep in subroutine calls!\n"
	    if $single & 4;
	$start = $line;
	while ((print OUT "  DB<", $#hist+1, "> "), $cmd=&gets) {
	    $single = 0;
	    $signal = 0;
	    $cmd eq '' && exit 0;
	    chop($cmd);
	    $cmd =~ s/\\$// && do {
		print OUT "  cont: ";
		$cmd .= &gets;
		redo;
	    };
	    $cmd =~ /^q$/ && exit 0;
	    $cmd =~ /^$/ && ($cmd = $laststep);
	    push(@hist,$cmd) if length($cmd) > 1;
	    ($i) = split(/\s+/,$cmd);
	    eval "\$cmd =~ $alias{$i}", print OUT $@ if $alias{$i};
	    $cmd =~ /^h$/ && do {
		print OUT "
T		Stack trace.
s		Single step.
n		Next, steps over subroutine calls.
r		Return from current subroutine.
c [line]	Continue; optionally inserts a one-time-only breakpoint 
		at the specified line.
<CR>		Repeat last n or s.
l min+incr	List incr+1 lines starting at min.
l min-max	List lines.
l line		List line;
l		List next window.
-		List previous window.
w line		List window around line.
l subname	List subroutine.
f filename	Switch to filename.
/pattern/	Search forwards for pattern; final / is optional.
?pattern?	Search backwards for pattern.
L		List breakpoints and actions.
S		List subroutine names.
t		Toggle trace mode.
b [line] [condition]
		Set breakpoint; line defaults to the current execution line; 
		condition breaks if it evaluates to true, defaults to \'1\'.
b subname [condition]
		Set breakpoint at first line of subroutine.
d [line]	Delete breakpoint.
D		Delete all breakpoints.
a [line] command
		Set an action to be done before the line is executed.
		Sequence is: check for breakpoint, print line if necessary,
		do action, prompt user if breakpoint or step, evaluate line.
A		Delete all actions.
V [pkg [vars]]	List some (default all) variables in package (default current).
X [vars]	Same as \"V currentpackage [vars]\".
< command	Define command before prompt.
| command	Define command after prompt.
! number	Redo command (default previous command).
! -number	Redo number\'th to last command.
H -number	Display last number commands (default all).
q or ^D		Quit.
p expr		Same as \"print DB'OUT expr\" in current package.
= [alias value]	Define a command alias, or list current aliases.
command		Execute as a perl statement in current package.

";
		next; };
	    $cmd =~ /^t$/ && do {
		$trace = !$trace;
		print OUT "Trace = ".($trace?"on":"off")."\n";
		next; };
	    $cmd =~ /^S$/ && do {
		foreach $subname (sort(keys %sub)) {
		    print OUT $subname,"\n";
		}
		next; };
	    $cmd =~ s/^X\b/V $package/;
	    $cmd =~ /^V$/ && do {
		$cmd = 'V $package'; };
	    $cmd =~ /^V\s*(\S+)\s*(.*)/ && do {
		$packname = $1;
		@vars = split(' ',$2);
		do 'dumpvar.pl' unless defined &main'dumpvar;
		if (defined &main'dumpvar) {
		    &main'dumpvar($packname,@vars);
		}
		else {
		    print DB'OUT "dumpvar.pl not available.\n";
		}
		next; };
	    $cmd =~ /^f\s*(.*)/ && do {
		$file = $1;
		if (!$file) {
		    print OUT "The old f command is now the r command.\n";
		    print OUT "The new f command switches filenames.\n";
		    next;
		}
		if (!defined $_main{'_<' . $file}) {
		    if (($try) = grep(m#^_<.*$file#, keys %_main)) {
			$file = substr($try,2);
			print "\n$file:\n";
		    }
		}
		if (!defined $_main{'_<' . $file}) {
		    print OUT "There's no code here anything matching $file.\n";
		    next;
		}
		elsif ($file ne $filename) {
		    *dbline = "_<$file";
		    $max = $#dbline;
		    $filename = $file;
		    $start = 1;
		    $cmd = "l";
		} };
	    $cmd =~ /^l\s*(['A-Za-z_]['\w]*)/ && do {
		$subname = $1;
		$subname = "main'" . $subname unless $subname =~ /'/;
		$subname = "main" . $subname if substr($subname,0,1) eq "'";
		($file,$subrange) = split(/:/,$sub{$subname});
		if ($file ne $filename) {
		    *dbline = "_<$file";
		    $max = $#dbline;
		    $filename = $file;
		}
		if ($subrange) {
		    if (eval($subrange) < -$window) {
			$subrange =~ s/-.*/+/;
		    }
		    $cmd = "l $subrange";
		} else {
		    print OUT "Subroutine $1 not found.\n";
		    next;
		} };
	    $cmd =~ /^w\s*(\d*)$/ && do {
		$incr = $window - 1;
		$start = $1 if $1;
		$start -= $preview;
		$cmd = 'l ' . $start . '-' . ($start + $incr); };
	    $cmd =~ /^-$/ && do {
		$incr = $window - 1;
		$cmd = 'l ' . ($start-$window*2) . '+'; };
	    $cmd =~ /^l$/ && do {
		$incr = $window - 1;
		$cmd = 'l ' . $start . '-' . ($start + $incr); };
	    $cmd =~ /^l\s*(\d*)\+(\d*)$/ && do {
		$start = $1 if $1;
		$incr = $2;
		$incr = $window - 1 unless $incr;
		$cmd = 'l ' . $start . '-' . ($start + $incr); };
	    $cmd =~ /^l\s*(([\d\$\.]+)([-,]([\d\$\.]+))?)?/ && do {
		$end = (!$2) ? $max : ($4 ? $4 : $2);
		$end = $max if $end > $max;
		$i = $2;
		$i = $line if $i eq '.';
		$i = 1 if $i < 1;
		if ($emacs) {
		    print OUT "\032\032$filename:$i:0\n";
		    $i = $end;
		} else {
		    for (; $i <= $end; $i++) {
			print OUT "$i:\t", $dbline[$i];
			last if $signal;
		    }
		}
		$start = $i;	# remember in case they want more
		$start = $max if $start > $max;
		next; };
	    $cmd =~ /^D$/ && do {
		print OUT "Deleting all breakpoints...\n";
		for ($i = 1; $i <= $max ; $i++) {
		    if (defined $dbline{$i}) {
			$dbline{$i} =~ s/^[^\0]+//;
			if ($dbline{$i} =~ s/^\0?$//) {
			    delete $dbline{$i};
			}
		    }
		}
		next; };
	    $cmd =~ /^L$/ && do {
		for ($i = 1; $i <= $max; $i++) {
		    if (defined $dbline{$i}) {
			print OUT "$i:\t", $dbline[$i];
			($stop,$action) = split(/\0/, $dbline{$i});
			print OUT "  break if (", $stop, ")\n" 
			    if $stop;
			print OUT "  action:  ", $action, "\n" 
			    if $action;
			last if $signal;
		    }
		}
		next; };
	    $cmd =~ /^b\s*(['A-Za-z_]['\w]*)\s*(.*)/ && do {
		$subname = $1;
		$cond = $2 || '1';
		$subname = "$package'" . $subname unless $subname =~ /'/;
		$subname = "main" . $subname if substr($subname,0,1) eq "'";
		($filename,$i) = split(/[:-]/, $sub{$subname});
		if ($i) {
		    *dbline = "_<$filename";
		    ++$i while $dbline[$i] == 0 && $i < $#dbline;
		    $dbline{$i} =~ s/^[^\0]*/$cond/;
		} else {
		    print OUT "Subroutine $subname not found.\n";
		}
		next; };
	    $cmd =~ /^b\s*(\d*)\s*(.*)/ && do {
		$i = ($1?$1:$line);
		$cond = $2 || '1';
		if ($dbline[$i] == 0) {
		    print OUT "Line $i not breakable.\n";
		} else {
		    $dbline{$i} =~ s/^[^\0]*/$cond/;
		}
		next; };
	    $cmd =~ /^d\s*(\d+)?/ && do {
		$i = ($1?$1:$line);
		$dbline{$i} =~ s/^[^\0]*//;
		delete $dbline{$i} if $dbline{$i} eq '';
		next; };
	    $cmd =~ /^A$/ && do {
		for ($i = 1; $i <= $max ; $i++) {
		    if (defined $dbline{$i}) {
			$dbline{$i} =~ s/\0[^\0]*//;
			delete $dbline{$i} if $dbline{$i} eq '';
		    }
		}
		next; };
	    $cmd =~ /^<\s*(.*)/ && do {
		$pre = do action($1);
		next; };
	    $cmd =~ /^>\s*(.*)/ && do {
		$post = do action($1);
		next; };
	    $cmd =~ /^a\s*(\d+)(\s+(.*))?/ && do {
		$i = $1;
		if ($dbline[$i] == 0) {
		    print OUT "Line $i may not have an action.\n";
		} else {
		    $dbline{$i} =~ s/\0[^\0]*//;
		    $dbline{$i} .= "\0" . do action($3);
		}
		next; };
	    $cmd =~ /^n$/ && do {
		$single = 2;
		$laststep = $cmd;
		last; };
	    $cmd =~ /^s$/ && do {
		$single = 1;
		$laststep = $cmd;
		last; };
	    $cmd =~ /^c\s*(\d*)\s*$/ && do {
		$i = $1;
		if ($i) {
		    if ($dbline[$i] == 0) {
		        print OUT "Line $i not breakable.\n";
			next;
		    }
		    $dbline{$i} =~ s/(\0|$)/;9$1/;	# add one-time-only b.p.
		}
		for ($i=0; $i <= $#stack; ) {
		    $stack[$i++] &= ~1;
		}
		last; };
	    $cmd =~ /^r$/ && do {
		$stack[$#stack] |= 2;
		last; };
	    $cmd =~ /^T$/ && do {
		local($p,$f,$l,$s,$h,$a,@a,@sub);
		for ($i = 1; ($p,$f,$l,$s,$h,$w) = caller($i); $i++) {
		    @a = @args;
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
		    print OUT $sub[$i];
		}
	        next; };
	    $cmd =~ /^\/(.*)$/ && do {
		$inpat = $1;
		$inpat =~ s:([^\\])/$:$1:;
		if ($inpat ne "") {
		    eval '$inpat =~ m'."\n$inpat\n";	
		    if ($@ ne "") {
		    	print OUT "$@";
		    	next;
		    }
		    $pat = $inpat;
		}
		$end = $start;
		eval '
		for (;;) {
		    ++$start;
		    $start = 1 if ($start > $max);
		    last if ($start == $end);
		    if ($dbline[$start] =~ m'."\n$pat\n".'i) {
			if ($emacs) {
			    print OUT "\032\032$filename:$start:0\n";
			} else {
			    print OUT "$start:\t", $dbline[$start], "\n";
			}
			last;
		    }
		} ';
		print OUT "/$pat/: not found\n" if ($start == $end);
		next; };
	    $cmd =~ /^\?(.*)$/ && do {
		$inpat = $1;
		$inpat =~ s:([^\\])\?$:$1:;
		if ($inpat ne "") {
		    eval '$inpat =~ m'."\n$inpat\n";	
		    if ($@ ne "") {
		    	print OUT "$@";
		    	next;
		    }
		    $pat = $inpat;
		}
		$end = $start;
		eval '
		for (;;) {
		    --$start;
		    $start = $max if ($start <= 0);
		    last if ($start == $end);
		    if ($dbline[$start] =~ m'."\n$pat\n".'i) {
			if ($emacs) {
			    print OUT "\032\032$filename:$start:0\n";
			} else {
			    print OUT "$start:\t", $dbline[$start], "\n";
			}
			last;
		    }
		} ';
		print OUT "?$pat?: not found\n" if ($start == $end);
		next; };
	    $cmd =~ /^!+\s*(-)?(\d+)?$/ && do {
		pop(@hist) if length($cmd) > 1;
		$i = ($1?($#hist-($2?$2:1)):($2?$2:$#hist));
		$cmd = $hist[$i] . "\n";
		print OUT $cmd;
		redo; };
	    $cmd =~ /^!(.+)$/ && do {
		$pat = "^$1";
		pop(@hist) if length($cmd) > 1;
		for ($i = $#hist; $i; --$i) {
		    last if $hist[$i] =~ $pat;
		}
		if (!$i) {
		    print OUT "No such command!\n\n";
		    next;
		}
		$cmd = $hist[$i] . "\n";
		print OUT $cmd;
		redo; };
	    $cmd =~ /^H\s*(-(\d+))?/ && do {
		$end = $2?($#hist-$2):0;
		$hist = 0 if $hist < 0;
		for ($i=$#hist; $i>$end; $i--) {
		    print OUT "$i: ",$hist[$i],"\n"
			unless $hist[$i] =~ /^.?$/;
		};
		next; };
	    $cmd =~ s/^p( .*)?$/print DB'OUT$1/;
	    $cmd =~ /^=/ && do {
		if (local($k,$v) = ($cmd =~ /^=\s*(\S+)\s+(.*)/)) {
		    $alias{$k}="s~$k~$v~";
		    print OUT "$k = $v\n";
		} elsif ($cmd =~ /^=\s*$/) {
		    foreach $k (sort keys(%alias)) {
			if (($v = $alias{$k}) =~ s~^s\~$k\~(.*)\~$~$1~) {
			    print OUT "$k = $v\n";
			} else {
			    print OUT "$k\t$alias{$k}\n";
			};
		    };
		};
		next; };
	    $evalarg = $cmd; &eval;
	    print OUT "\n";
	}
	if ($post) {
	    $evalarg = $post; &eval;
	}
    }
    ($@, $!, $[, $,, $/, $\) = @saved;
}

sub save {
    @saved = ($@, $!, $[, $,, $/, $\);
    $[ = 0; $, = ""; $/ = "\n"; $\ = "";
}

# The following takes its argument via $evalarg to preserve current @_

sub eval {
    eval "$usercontext $evalarg; &DB'save";
    print OUT $@;
}

sub action {
    local($action) = @_;
    while ($action =~ s/\\$//) {
	print OUT "+ ";
	$action .= &gets;
    }
    $action;
}

sub gets {
    local($.);
    <IN>;
}

sub catch {
    $signal = 1;
}

sub sub {
    push(@stack, $single);
    $single &= 1;
    $single |= 4 if $#stack == $deep;
    if (wantarray) {
	@i = &$sub;
	$single |= pop(@stack);
	@i;
    }
    else {
	$i = &$sub;
	$single |= pop(@stack);
	$i;
    }
}

$single = 1;			# so it stops on first executable statement
@hist = ('?');
$SIG{'INT'} = "DB'catch";
$deep = 100;		# warning if stack gets this deep
$window = 10;
$preview = 3;

@stack = (0);
@ARGS = @ARGV;
for (@args) {
    s/'/\\'/g;
    s/(.*)/'$1'/ unless /^-?[\d.]+$/;
}

if (-f '.perldb') {
    do './.perldb';
}
elsif (-f "$ENV{'LOGDIR'}/.perldb") {
    do "$ENV{'LOGDIR'}/.perldb";
}
elsif (-f "$ENV{'HOME'}/.perldb") {
    do "$ENV{'HOME'}/.perldb";
}

1;
