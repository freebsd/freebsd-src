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
(\$startperl = <<'/../') =~ s/\\s*\\z//;
$Config{startperl}
/../
(\$perlpath = <<'/../') =~ s/\\s*\\z//;
$Config{perlpath}
/../
!GROK!THIS!

# In the following, perl variables are not expanded during extraction.

print OUT <<'!NO!SUBS!';

# $RCSfile: s2p.SH,v $$Revision: 4.1 $$Date: 92/08/07 18:29:23 $
#
# $Log:	s2p.SH,v $

=head1 NAME

s2p - Sed to Perl translator

=head1 SYNOPSIS

B<s2p [options] filename>

=head1 DESCRIPTION

I<s2p> takes a sed script specified on the command line (or from
standard input) and produces a comparable I<perl> script on the
standard output.

=head2 Options

Options include:

=over 5

=item B<-DE<lt>numberE<gt>>

sets debugging flags.

=item B<-n>

specifies that this sed script was always invoked with a B<sed -n>.
Otherwise a switch parser is prepended to the front of the script.

=item B<-p>

specifies that this sed script was never invoked with a B<sed -n>.
Otherwise a switch parser is prepended to the front of the script.

=back

=head2 Considerations

The perl script produced looks very sed-ish, and there may very well
be better ways to express what you want to do in perl.  For instance,
s2p does not make any use of the split operator, but you might want
to.

The perl script you end up with may be either faster or slower than
the original sed script.  If you're only interested in speed you'll
just have to try it both ways.  Of course, if you want to do something
sed doesn't do, you have no choice.  It's often possible to speed up
the perl script by various methods, such as deleting all references to
$\ and chop.

=head1 ENVIRONMENT

s2p uses no environment variables.

=head1 AUTHOR

Larry Wall E<lt>F<larry@wall.org>E<gt>

=head1 FILES

=head1 SEE ALSO

 perl	The perl compiler/interpreter

 a2p	awk to perl translator

=head1 DIAGNOSTICS

=head1 BUGS

=cut

$indent = 4;
$shiftwidth = 4;
$l = '{'; $r = '}';

while ($ARGV[0] =~ /^-/) {
    $_ = shift;
  last if /^--/;
    if (/^-D/) {
	$debug++;
	open(BODY,'>-');
	next;
    }
    if (/^-n/) {
	$assumen++;
	next;
    }
    if (/^-p/) {
	$assumep++;
	next;
    }
    die "I don't recognize this switch: $_\n";
}

unless ($debug) {
    open(BODY,"+>/tmp/sperl$$") ||
      &Die("Can't open temp file: $!\n");
}

if (!$assumen && !$assumep) {
    print BODY &q(<<'EOT');
:	while ($ARGV[0] =~ /^-/) {
:	    $_ = shift;
:	  last if /^--/;
:	    if (/^-n/) {
:		$nflag++;
:		next;
:	    }
:	    die "I don't recognize this switch: $_\\n";
:	}
:	
EOT
}

print BODY &q(<<'EOT');
:	#ifdef PRINTIT
:	#ifdef ASSUMEP
:	$printit++;
:	#else
:	$printit++ unless $nflag;
:	#endif
:	#endif
:	<><>
:	$\ = "\n";		# automatically add newline on print
:	<><>
:	#ifdef TOPLABEL
:	LINE:
:	while (chop($_ = <>)) {
:	#else
:	LINE:
:	while (<>) {
:	    chop;
:	#endif
EOT

LINE:
while (<>) {

    # Wipe out surrounding whitespace.

    s/[ \t]*(.*)\n$/$1/;

    # Perhaps it's a label/comment.

    if (/^:/) {
	s/^:[ \t]*//;
	$label = &make_label($_);
	if ($. == 1) {
	    $toplabel = $label;
	    if (/^(top|(re)?start|redo|begin(ning)|again|input)$/i) {
		$_ = <>;
		redo LINE; # Never referenced, so delete it if not a comment.
	    }
	}
	$_ = "$label:";
	if ($lastlinewaslabel++) {
	    $indent += 4;
	    print BODY &tab, ";\n";
	    $indent -= 4;
	}
	if ($indent >= 2) {
	    $indent -= 2;
	    $indmod = 2;
	}
	next;
    } else {
	$lastlinewaslabel = '';
    }

    # Look for one or two address clauses

    $addr1 = '';
    $addr2 = '';
    if (s/^([0-9]+)//) {
	$addr1 = "$1";
	$addr1 = "\$. == $addr1" unless /^,/;
    }
    elsif (s/^\$//) {
	$addr1 = 'eof()';
    }
    elsif (s|^/||) {
	$addr1 = &fetchpat('/');
    }
    if (s/^,//) {
	if (s/^([0-9]+)//) {
	    $addr2 = "$1";
	} elsif (s/^\$//) {
	    $addr2 = "eof()";
	} elsif (s|^/||) {
	    $addr2 = &fetchpat('/');
	} else {
	    &Die("Invalid second address at line $.\n");
	}
	if ($addr2 =~ /^\d+$/) {
	    $addr1 .= "..$addr2";
	}
	else {
	    $addr1 .= "...$addr2";
	}
    }

    # Now we check for metacommands {, }, and ! and worry
    # about indentation.

    s/^[ \t]+//;
    # a { to keep vi happy
    if ($_ eq '}') {
	$indent -= 4;
	next;
    }
    if (s/^!//) {
	$if = 'unless';
	$else = "$r else $l\n";
    } else {
	$if = 'if';
	$else = '';
    }
    if (s/^{//) {	# a } to keep vi happy
	$indmod = 4;
	$redo = $_;
	$_ = '';
	$rmaybe = '';
    } else {
	$rmaybe = "\n$r";
	if ($addr2 || $addr1) {
	    $space = ' ' x $shiftwidth;
	} else {
	    $space = '';
	}
	$_ = &transmogrify();
    }

    # See if we can optimize to modifier form.

    if ($addr1) {
	if ($_ !~ /[\n{}]/ && $rmaybe && !$change &&
	  $_ !~ / if / && $_ !~ / unless /) {
	    s/;$/ $if $addr1;/;
	    $_ = substr($_,$shiftwidth,1000);
	} else {
	    $_ = "$if ($addr1) $l\n$change$_$rmaybe";
	}
	$change = '';
	next LINE;
    }
} continue {
    @lines = split(/\n/,$_);
    for (@lines) {
	unless (s/^ *<<--//) {
	    print BODY &tab;
	}
	print BODY $_, "\n";
    }
    $indent += $indmod;
    $indmod = 0;
    if ($redo) {
	$_ = $redo;
	$redo = '';
	redo LINE;
    }
}
if ($lastlinewaslabel++) {
    $indent += 4;
    print BODY &tab, ";\n";
    $indent -= 4;
}

if ($appendseen || $tseen || !$assumen) {
    $printit++ if $dseen || (!$assumen && !$assumep);
    print BODY &q(<<'EOT');
:	#ifdef SAWNEXT
:	}
:	continue {
:	#endif
:	#ifdef PRINTIT
:	#ifdef DSEEN
:	#ifdef ASSUMEP
:	    print if $printit++;
:	#else
:	    if ($printit)
:		{ print; }
:	    else
:		{ $printit++ unless $nflag; }
:	#endif
:	#else
:	    print if $printit;
:	#endif
:	#else
:	    print;
:	#endif
:	#ifdef TSEEN
:	    $tflag = 0;
:	#endif
:	#ifdef APPENDSEEN
:	    if ($atext) { chop $atext; print $atext; $atext = ''; }
:	#endif
EOT
}

print BODY &q(<<'EOT');
:	}
EOT

unless ($debug) {

    print &q(<<"EOT");
:	$startperl
:	eval 'exec $perlpath -S \$0 \${1+"\$@"}'
:		if \$running_under_some_shell;
:	
EOT
    print"$opens\n" if $opens;
    seek(BODY, 0, 0) || die "Can't rewind temp file: $!\n";
    while (<BODY>) {
	/^[ \t]*$/ && next;
	/^#ifdef (\w+)/ && ((${lc $1} || &skip), next);
	/^#else/ && (&skip, next);
	/^#endif/ && next;
	s/^<><>//;
	print;
    }
}

&Cleanup;
exit;

sub Cleanup {
    unlink "/tmp/sperl$$";
}
sub Die {
    &Cleanup;
    die $_[0];
}
sub tab {
    "\t" x ($indent / 8) . ' ' x ($indent % 8);
}
sub make_filehandle {
    local($_) = $_[0];
    local($fname) = $_;
    if (!$seen{$fname}) {
	$_ = "FH_" . $_ if /^\d/;
	s/[^a-zA-Z0-9]/_/g;
	s/^_*//;
	$_ = "\U$_";
	if ($fhseen{$_}) {
	    for ($tmp = "a"; $fhseen{"$_$tmp"}; $a++) {}
	    $_ .= $tmp;
	}
	$fhseen{$_} = 1;
	$opens .= &q(<<"EOT");
:	open($_, '>$fname') || die "Can't create $fname: \$!";
EOT
	$seen{$fname} = $_;
    }
    $seen{$fname};
}

sub make_label {
    local($label) = @_;
    $label =~ s/[^a-zA-Z0-9]/_/g;
    if ($label =~ /^[0-9_]/) { $label = 'L' . $label; }
    $label = substr($label,0,8);

    # Could be a reserved word, so capitalize it.
    substr($label,0,1) =~ y/a-z/A-Z/
      if $label =~ /^[a-z]/;

    $label;
}

sub transmogrify {
    {	# case
	if (/^d/) {
	    $dseen++;
	    chop($_ = &q(<<'EOT'));
:	<<--#ifdef PRINTIT
:	$printit = 0;
:	<<--#endif
:	next LINE;
EOT
	    $sawnext++;
	    next;
	}

	if (/^n/) {
	    chop($_ = &q(<<'EOT'));
:	<<--#ifdef PRINTIT
:	<<--#ifdef DSEEN
:	<<--#ifdef ASSUMEP
:	print if $printit++;
:	<<--#else
:	if ($printit)
:	    { print; }
:	else
:	    { $printit++ unless $nflag; }
:	<<--#endif
:	<<--#else
:	print if $printit;
:	<<--#endif
:	<<--#else
:	print;
:	<<--#endif
:	<<--#ifdef APPENDSEEN
:	if ($atext) {chop $atext; print $atext; $atext = '';}
:	<<--#endif
:	$_ = <>;
:	chop;
:	<<--#ifdef TSEEN
:	$tflag = 0;
:	<<--#endif
EOT
	    next;
	}

	if (/^a/) {
	    $appendseen++;
	    $command = $space . "\$atext .= <<'End_Of_Text';\n<<--";
	    $lastline = 0;
	    while (<>) {
		s/^[ \t]*//;
		s/^[\\]//;
		unless (s|\\$||) { $lastline = 1;}
		s/^([ \t]*\n)/<><>$1/;
		$command .= $_;
		$command .= '<<--';
		last if $lastline;
	    }
	    $_ = $command . "End_Of_Text";
	    last;
	}

	if (/^[ic]/) {
	    if (/^c/) { $change = 1; }
	    $addr1 = 1 if $addr1 eq '';
	    $addr1 = '$iter = (' . $addr1 . ')';
	    $command = $space .
	      "    if (\$iter == 1) { print <<'End_Of_Text'; }\n<<--";
	    $lastline = 0;
	    while (<>) {
		s/^[ \t]*//;
		s/^[\\]//;
		unless (s/\\$//) { $lastline = 1;}
		s/'/\\'/g;
		s/^([ \t]*\n)/<><>$1/;
		$command .= $_;
		$command .= '<<--';
		last if $lastline;
	    }
	    $_ = $command . "End_Of_Text";
	    if ($change) {
		$dseen++;
		$change = "$_\n";
		chop($_ = &q(<<"EOT"));
:	<<--#ifdef PRINTIT
:	$space\$printit = 0;
:	<<--#endif
:	${space}next LINE;
EOT
		$sawnext++;
	    }
	    last;
	}

	if (/^s/) {
	    $delim = substr($_,1,1);
	    $len = length($_);
	    $repl = $end = 0;
	    $inbracket = 0;
	    for ($i = 2; $i < $len; $i++) {
		$c = substr($_,$i,1);
		if ($c eq $delim) {
		    if ($inbracket) {
			substr($_, $i, 0) = '\\';
			$i++;
			$len++;
		    }
		    else {
			if ($repl) {
			    $end = $i;
			    last;
			} else {
			    $repl = $i;
			}
		    }
		}
		elsif ($c eq '\\') {
		    $i++;
		    if ($i >= $len) {
			$_ .= 'n';
			$_ .= <>;
			$len = length($_);
			$_ = substr($_,0,--$len);
		    }
		    elsif (substr($_,$i,1) =~ /^[n]$/) {
			;
		    }
		    elsif (!$repl &&
		      substr($_,$i,1) =~ /^[(){}\w]$/) {
			$i--;
			$len--;
			substr($_, $i, 1) = '';
		    }
		    elsif (!$repl &&
		      substr($_,$i,1) =~ /^[<>]$/) {
			substr($_,$i,1) = 'b';
		    }
		    elsif ($repl && substr($_,$i,1) =~ /^\d$/) {
			substr($_,$i-1,1) = '$';
		    }
		}
		elsif ($c eq '@') {
		    substr($_, $i, 0) = '\\';
		    $i++;
		    $len++;
		}
		elsif ($c eq '&' && $repl) {
		    substr($_, $i, 0) = '$';
		    $i++;
		    $len++;
		}
		elsif ($c eq '$' && $repl) {
		    substr($_, $i, 0) = '\\';
		    $i++;
		    $len++;
		}
		elsif ($c eq '[' && !$repl) {
		    $i++ if substr($_,$i,1) eq '^';
		    $i++ if substr($_,$i,1) eq ']';
		    $inbracket = 1;
		}
		elsif ($c eq ']') {
		    $inbracket = 0;
		}
		elsif ($c eq "\t") {
		    substr($_, $i, 1) = '\\t';
		    $i++;
		    $len++;
		}
		elsif (!$repl && index("()+",$c) >= 0) {
		    substr($_, $i, 0) = '\\';
		    $i++;
		    $len++;
		}
	    }
	    &Die("Malformed substitution at line $.\n")
	      unless $end;
	    $pat = substr($_, 0, $repl + 1);
	    $repl = substr($_, $repl+1, $end-$repl-1);
	    $end = substr($_, $end + 1, 1000);
	    &simplify($pat);
	    $subst = "$pat$repl$delim";
	    $cmd = '';
	    while ($end) {
		if ($end =~ s/^g//) {
		    $subst .= 'g';
		    next;
		}
		if ($end =~ s/^p//) {
		    $cmd .= ' && (print)';
		    next;
		}
		if ($end =~ s/^w[ \t]*//) {
		    $fh = &make_filehandle($end);
		    $cmd .= " && (print $fh \$_)";
		    $end = '';
		    next;
		}
		&Die("Unrecognized substitution command".
		  "($end) at line $.\n");
	    }
	    chop ($_ = &q(<<"EOT"));
:	<<--#ifdef TSEEN
:	$subst && \$tflag++$cmd;
:	<<--#else
:	$subst$cmd;
:	<<--#endif
EOT
	    next;
	}

	if (/^p/) {
	    $_ = 'print;';
	    next;
	}

	if (/^w/) {
	    s/^w[ \t]*//;
	    $fh = &make_filehandle($_);
	    $_ = "print $fh \$_;";
	    next;
	}

	if (/^r/) {
	    $appendseen++;
	    s/^r[ \t]*//;
	    $file = $_;
	    $_ = "\$atext .= `cat $file 2>/dev/null`;";
	    next;
	}

	if (/^P/) {
	    $_ = 'print $1 if /^(.*)/;';
	    next;
	}

	if (/^D/) {
	    chop($_ = &q(<<'EOT'));
:	s/^.*\n?//;
:	redo LINE if $_;
:	next LINE;
EOT
	    $sawnext++;
	    next;
	}

	if (/^N/) {
	    chop($_ = &q(<<'EOT'));
:	$_ .= "\n";
:	$len1 = length;
:	$_ .= <>;
:	chop if $len1 < length;
:	<<--#ifdef TSEEN
:	$tflag = 0;
:	<<--#endif
EOT
	    next;
	}

	if (/^h/) {
	    $_ = '$hold = $_;';
	    next;
	}

	if (/^H/) {
	    $_ = '$hold .= "\n", $hold .= $_;';
	    next;
	}

	if (/^g/) {
	    $_ = '$_ = $hold;';
	    next;
	}

	if (/^G/) {
	    $_ = '$_ .= "\n", $_ .= $hold;';
	    next;
	}

	if (/^x/) {
	    $_ = '($_, $hold) = ($hold, $_);';
	    next;
	}

	if (/^b$/) {
	    $_ = 'next LINE;';
	    $sawnext++;
	    next;
	}

	if (/^b/) {
	    s/^b[ \t]*//;
	    $lab = &make_label($_);
	    if ($lab eq $toplabel) {
		$_ = 'redo LINE;';
	    } else {
		$_ = "goto $lab;";
	    }
	    next;
	}

	if (/^t$/) {
	    $_ = 'next LINE if $tflag;';
	    $sawnext++;
	    $tseen++;
	    next;
	}

	if (/^t/) {
	    s/^t[ \t]*//;
	    $lab = &make_label($_);
	    $_ = q/if ($tflag) {$tflag = 0; /;
	    if ($lab eq $toplabel) {
		$_ .= 'redo LINE;}';
	    } else {
		$_ .= "goto $lab;}";
	    }
	    $tseen++;
	    next;
	}

	if (/^y/) {
	    s/abcdefghijklmnopqrstuvwxyz/a-z/g;
	    s/ABCDEFGHIJKLMNOPQRSTUVWXYZ/A-Z/g;
	    s/abcdef/a-f/g;
	    s/ABCDEF/A-F/g;
	    s/0123456789/0-9/g;
	    s/01234567/0-7/g;
	    $_ .= ';';
	}

	if (/^=/) {
	    $_ = 'print $.;';
	    next;
	}

	if (/^q/) {
	    chop($_ = &q(<<'EOT'));
:	close(ARGV);
:	@ARGV = ();
:	next LINE;
EOT
	    $sawnext++;
	    next;
	}
    } continue {
	if ($space) {
	    s/^/$space/;
	    s/(\n)(.)/$1$space$2/g;
	}
	last;
    }
    $_;
}

sub fetchpat {
    local($outer) = @_;
    local($addr) = $outer;
    local($inbracket);
    local($prefix,$delim,$ch);

    # Process pattern one potential delimiter at a time.

    DELIM: while (s#^([^\]+(|)[\\/]*)([]+(|)[\\/])##) {
	$prefix = $1;
	$delim = $2;
	if ($delim eq '\\') {
	    s/(.)//;
	    $ch = $1;
	    $delim = '' if $ch =~ /^[(){}A-Za-mo-z]$/;
	    $ch = 'b' if $ch =~ /^[<>]$/;
	    $delim .= $ch;
	}
	elsif ($delim eq '[') {
	    $inbracket = 1;
	    s/^\^// && ($delim .= '^');
	    s/^]// && ($delim .= ']');
	}
	elsif ($delim eq ']') {
	    $inbracket = 0;
	}
	elsif ($inbracket || $delim ne $outer) {
	    $delim = '\\' . $delim;
	}
	$addr .= $prefix;
	$addr .= $delim;
	if ($delim eq $outer && !$inbracket) {
	    last DELIM;
	}
    }
    $addr =~ s/\t/\\t/g;
    $addr =~ s/\@/\\@/g;
    &simplify($addr);
    $addr;
}

sub q {
    local($string) = @_;
    local($*) = 1;
    $string =~ s/^:\t?//g;
    $string;
}

sub simplify {
    $_[0] =~ s/_a-za-z0-9/\\w/ig;
    $_[0] =~ s/a-z_a-z0-9/\\w/ig;
    $_[0] =~ s/a-za-z_0-9/\\w/ig;
    $_[0] =~ s/a-za-z0-9_/\\w/ig;
    $_[0] =~ s/_0-9a-za-z/\\w/ig;
    $_[0] =~ s/0-9_a-za-z/\\w/ig;
    $_[0] =~ s/0-9a-z_a-z/\\w/ig;
    $_[0] =~ s/0-9a-za-z_/\\w/ig;
    $_[0] =~ s/\[\\w\]/\\w/g;
    $_[0] =~ s/\[^\\w\]/\\W/g;
    $_[0] =~ s/\[0-9\]/\\d/g;
    $_[0] =~ s/\[^0-9\]/\\D/g;
    $_[0] =~ s/\\d\\d\*/\\d+/g;
    $_[0] =~ s/\\D\\D\*/\\D+/g;
    $_[0] =~ s/\\w\\w\*/\\w+/g;
    $_[0] =~ s/\\t\\t\*/\\t+/g;
    $_[0] =~ s/(\[.[^]]*\])\1\*/$1+/g;
    $_[0] =~ s/([\w\s!@#%^&-=,:;'"])\1\*/$1+/g;
}

sub skip {
    local($level) = 0;

    while(<BODY>) {
	/^#ifdef/ && $level++;
	/^#else/  && !$level && return;
	/^#endif/ && !$level-- && return;
    }

    die "Unterminated `#ifdef' conditional\n";
}
!NO!SUBS!

close OUT or die "Can't close $file: $!";
chmod 0755, $file or die "Can't reset permissions for $file: $!\n";
exec("$Config{'eunicefix'} $file") if $Config{'eunicefix'} ne ':';
chdir $origdir;
