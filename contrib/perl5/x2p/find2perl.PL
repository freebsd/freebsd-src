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
\$startperl = "$Config{startperl}";
\$perlpath = "$Config{perlpath}";
!GROK!THIS!

# In the following, perl variables are not expanded during extraction.

print OUT <<'!NO!SUBS!';

# 
# Modified September 26, 1993 to provide proper handling of years after 1999
#   Tom Link <tml+@pitt.edu>
#   University of Pittsburgh
# 
# Modified April 7, 1998 with nasty hacks to implement the troublesome -follow
#  Billy Constantine <wdconsta@cs.adelaide.edu.au> <billy@smug.adelaide.edu.au>
#  University of Adelaide, Adelaide, South Australia
# 

while ($ARGV[0] =~ /^[^-!(]/) {
    push(@roots, shift);
}
@roots = ('.') unless @roots;
for (@roots) { $_ = &quote($_); }
$roots = join(',', @roots);

$indent = 1;
$stat = 'lstat';
$decl = '';

while (@ARGV) {
    $_ = shift;
    s/^-// || /^[()!]/ || die "Unrecognized switch: $_\n";
    if ($_ eq '(') {
	$out .= &tab . "(\n";
	$indent++;
	next;
    }
    elsif ($_ eq ')') {
	$indent--;
	$out .= &tab . ")";
    }
    elsif ($_ eq 'follow') {
	$stat = 'stat';
	$decl = '%already_seen = ();';
	$out .= &tab . '(not $already_seen{"$dev,$ino"}) &&';
	$out .= "\n" . &tab . '(($already_seen{"$dev,$ino"} = !(-d _)) || 1)';
    }
    elsif ($_ eq '!') {
	$out .= &tab . "!";
	next;
    }
    elsif ($_ eq 'name') {
	$out .= &tab;
	$pat = &fileglob_to_re(shift);
	$out .= '/' . $pat . "/";
    }
    elsif ($_ eq 'perm') {
	$onum = shift;
	die "Malformed -perm argument: $onum\n" unless $onum =~ /^-?[0-7]+$/;
	if ($onum =~ s/^-//) {
	    $onum = '0' . sprintf("%o", oct($onum) & 017777);	# s/b 07777 ?
	    $out .= &tab . "((\$mode & $onum) == $onum)";
	}
	else {
	    $onum = '0' . $onum unless $onum =~ /^0/;
	    $out .= &tab . "((\$mode & 0777) == $onum)";
	}
    }
    elsif ($_ eq 'type') {
	($filetest = shift) =~ tr/s/S/;
	$out .= &tab . "-$filetest _";
    }
    elsif ($_ eq 'print') {
	$out .= &tab . 'print("$name\n")';
    }
    elsif ($_ eq 'print0') {
	$out .= &tab . 'print("$name\0")';
    }
    elsif ($_ eq 'fstype') {
	$out .= &tab;
	$type = shift;
	if ($type eq 'nfs')
	    { $out .= '($dev < 0)'; }
	else
	    { $out .= '($dev >= 0)'; }
    }
    elsif ($_ eq 'user') {
	$uname = shift;
	$out .= &tab . "(\$uid == \$uid{'$uname'})";
	$inituser++;
    }
    elsif ($_ eq 'group') {
	$gname = shift;
	$out .= &tab . "(\$gid == \$gid{'$gname'})";
	$initgroup++;
    }
    elsif ($_ eq 'nouser') {
	$out .= &tab . '!defined $uid{$uid}';
	$inituser++;
    }
    elsif ($_ eq 'nogroup') {
	$out .= &tab . '!defined $gid{$gid}';
	$initgroup++;
    }
    elsif ($_ eq 'links') {
	$out .= &tab . '($nlink ' . &n(shift);
    }
    elsif ($_ eq 'inum') {
	$out .= &tab . '($ino ' . &n(shift);
    }
    elsif ($_ eq 'size') {
	$out .= &tab . '(int(((-s _) + 511) / 512) ' . &n(shift);
    }
    elsif ($_ eq 'atime') {
	$out .= &tab . '(int(-A _) ' . &n(shift);
    }
    elsif ($_ eq 'mtime') {
	$out .= &tab . '(int(-M _) ' . &n(shift);
    }
    elsif ($_ eq 'ctime') {
	$out .= &tab . '(int(-C _) ' . &n(shift);
    }
    elsif ($_ eq 'exec') {
	for (@cmd = (); @ARGV && $ARGV[0] ne ';'; push(@cmd,shift)) { }
	shift;
	$_ = "@cmd";
	if (m#^(/bin/)?rm -f {}$#) {
	    if (!@ARGV) {
		$out .= &tab . 'unlink($_)';
	    }
	    else {
		$out .= &tab . '(unlink($_) || 1)';
	    }
	}
	elsif (m#^(/bin/)?rm {}$#) {
	    $out .= &tab . '(unlink($_) || warn "$name: $!\n")';
	}
	else {
	    for (@cmd) { s/'/\\'/g; }
	    $" = "','";
	    $out .= &tab . "&exec(0, '@cmd')";
	    $" = ' ';
	    $initexec++;
	}
    }
    elsif ($_ eq 'ok') {
	for (@cmd = (); @ARGV && $ARGV[0] ne ';'; push(@cmd,shift)) { }
	shift;
	for (@cmd) { s/'/\\'/g; }
	$" = "','";
	$out .= &tab . "&exec(1, '@cmd')";
	$" = ' ';
	$initexec++;
    }
    elsif ($_ eq 'prune') {
	$out .= &tab . '($prune = 1)';
    }
    elsif ($_ eq 'xdev') {
	$out .= &tab . '!($prune |= ($dev != $topdev))';
    }
    elsif ($_ eq 'newer') {
	$out .= &tab;
	$file = shift;
	$newername = 'AGE_OF' . $file;
	$newername =~ s/[^\w]/_/g;
	$newername = "\$$newername";
	$out .= "(-M _ < $newername)";
	$initnewer .= "$newername = -M " . &quote($file) . ";\n";
    }
    elsif ($_ eq 'eval') {
	$prog = &quote(shift);
	$out .= &tab . "eval $prog";
    }
    elsif ($_ eq 'depth') {
	$depth++;
	next;
    }
    elsif ($_ eq 'ls') {
	$out .= &tab . "&ls";
	$initls++;
    }
    elsif ($_ eq 'tar') {
	$out .= &tab;
	die "-tar must have a filename argument\n" unless @ARGV;
	$file = shift;
	$fh = 'FH' . $file;
	$fh =~ s/[^\w]/_/g;
	$out .= "&tar($fh)";
	$file = '>' . $file;
	$initfile .= "open($fh, " . &quote($file) .
	  qq{) || die "Can't open $fh: \$!\\n";\n};
	$inittar++;
	$flushall = "\n&tflushall;\n";
    }
    elsif (/^n?cpio$/) {
	$depth++;
	$out .= &tab;
	die "-$_ must have a filename argument\n" unless @ARGV;
	$file = shift;
	$fh = 'FH' . $file;
	$fh =~ s/[^\w]/_/g;
	$out .= "&cpio('" . substr($_,0,1) . "', $fh)";
	$file = '>' . $file;
	$initfile .= "open($fh, " . &quote($file) .
	  qq{) || die "Can't open $fh: \$!\\n";\n};
	$initcpio++;
	$flushall = "\n&flushall;\n";
    }
    else {
	die "Unrecognized switch: -$_\n";
    }
    if (@ARGV) {
	if ($ARGV[0] eq '-o') {
	    { local($statdone) = 1; $out .= "\n" . &tab . "||\n"; }
	    $statdone = 0 if $indent == 1 && $delayedstat;
	    $saw_or++;
	    shift;
	}
	else {
	    $out .= " &&" unless $ARGV[0] eq ')';
	    $out .= "\n";
	    shift if $ARGV[0] eq '-a';
	}
    }
}

print <<"END";
$startperl
    eval 'exec $perlpath -S \$0 \${1+"\$@"}'
	if \$running_under_some_shell;

END

if ($initls) {
    print <<'END';
@rwx = ('---','--x','-w-','-wx','r--','r-x','rw-','rwx');
@moname = (Jan,Feb,Mar,Apr,May,Jun,Jul,Aug,Sep,Oct,Nov,Dec);

END
}

if ($inituser || $initls) {
    print 'while (($name, $pw, $uid) = getpwent) {', "\n";
    print '    $uid{$name} = $uid{$uid} = $uid;', "\n" if $inituser;
    print '    $user{$uid} = $name unless $user{$uid};', "\n" if $initls;
    print "}\n\n";
}

if ($initgroup || $initls) {
    print 'while (($name, $pw, $gid) = getgrent) {', "\n";
    print '    $gid{$name} = $gid{$gid} = $gid;', "\n" if $initgroup;
    print '    $group{$gid} = $name unless $group{$gid};', "\n" if $initls;
    print "}\n\n";
}

print $initnewer, "\n" if $initnewer;

print $initfile, "\n" if $initfile;

$find = $depth ? "finddepth" : "find";
print <<"END";
require "$find.pl";

# Traverse desired filesystems

$decl
&$find($roots);
$flushall
exit;
sub wanted {
$out;
}

END

if ($initexec) {
    print <<'END';
sub exec {
    local($ok, @cmd) = @_;
    foreach $word (@cmd) {
	$word =~ s#{}#$name#g;
    }
    if ($ok) {
	local($old) = select(STDOUT);
	$| = 1;
	print "@cmd";
	select($old);
	return 0 unless <STDIN> =~ /^y/;
    }
    chdir $cwd;		# sigh
    system @cmd;
    chdir $dir;
    return !$?;
}

END
}

if ($initls) {
    print <<"INTERP", <<'END';
sub ls {
    (\$dev,\$ino,\$mode,\$nlink,\$uid,\$gid,\$rdev,\$sizemm,
      \$atime,\$mtime,\$ctime,\$blksize,\$blocks) = $stat\(_\);
INTERP

    $pname = $name;

    if (defined $blocks) {
	$blocks = int(($blocks + 1) / 2);
    }
    else {
	$blocks = int(($size + 1023) / 1024);
    }

    if    (-f _) { $perms = '-'; }
    elsif (-d _) { $perms = 'd'; }
    elsif (-c _) { $perms = 'c'; $sizemm = &sizemm; }
    elsif (-b _) { $perms = 'b'; $sizemm = &sizemm; }
    elsif (-p _) { $perms = 'p'; }
    elsif (-S _) { $perms = 's'; }
    else         { $perms = 'l'; $pname .= ' -> ' . readlink($_); }

    $tmpmode = $mode;
    $tmp = $rwx[$tmpmode & 7];
    $tmpmode >>= 3;
    $tmp = $rwx[$tmpmode & 7] . $tmp;
    $tmpmode >>= 3;
    $tmp = $rwx[$tmpmode & 7] . $tmp;
    substr($tmp,2,1) =~ tr/-x/Ss/ if -u _;
    substr($tmp,5,1) =~ tr/-x/Ss/ if -g _;
    substr($tmp,8,1) =~ tr/-x/Tt/ if -k _;
    $perms .= $tmp;

    $user = $user{$uid} || $uid;
    $group = $group{$gid} || $gid;

    ($sec,$min,$hour,$mday,$mon,$year) = localtime($mtime);
    $moname = $moname[$mon];
    if (-M _ > 365.25 / 2) {
	$timeyear = $year + 1900;
    }
    else {
	$timeyear = sprintf("%02d:%02d", $hour, $min);
    }

    printf "%5lu %4ld %-10s %2d %-8s %-8s %8s %s %2d %5s %s\n",
	    $ino,
		 $blocks,
		      $perms,
			    $nlink,
				$user,
				     $group,
					  $sizemm,
					      $moname,
						 $mday,
						     $timeyear,
							 $pname;
    1;
}

sub sizemm {
    sprintf("%3d, %3d", ($rdev >> 8) & 255, $rdev & 255);
}

END
}

if ($initcpio) {
print <<'START', <<"INTERP", <<'END';
sub cpio {
    local($nc,$fh) = @_;
    local($text);

    if ($name eq 'TRAILER!!!') {
	$text = '';
	$size = 0;
    }
    else {
START
	(\$dev,\$ino,\$mode,\$nlink,\$uid,\$gid,\$rdev,\$size,
	  \$atime,\$mtime,\$ctime,\$blksize,\$blocks) = $stat\(_\);
INTERP
	if (-f _) {
	    open(IN, "./$_\0") || do {
		warn "Couldn't open $name: $!\n";
		return;
	    };
	}
	else {
	    $text = readlink($_);
	    $size = 0 unless defined $text;
	}
    }

    ($nm = $name) =~ s#^\./##;
    $nc{$fh} = $nc;
    if ($nc eq 'n') {
	$cpout{$fh} .=
	  sprintf("%06o%06o%06o%06o%06o%06o%06o%06o%011lo%06o%011lo%s\0",
	    070707,
	    $dev & 0777777,
	    $ino & 0777777,
	    $mode & 0777777,
	    $uid & 0777777,
	    $gid & 0777777,
	    $nlink & 0777777,
	    $rdev & 0177777,
	    $mtime,
	    length($nm)+1,
	    $size,
	    $nm);
    }
    else {
	$cpout{$fh} .= "\0" if length($cpout{$fh}) & 1;
	$cpout{$fh} .= pack("SSSSSSSSLSLa*",
	    070707, $dev, $ino, $mode, $uid, $gid, $nlink, $rdev, $mtime,
	    length($nm)+1, $size, $nm . (length($nm) & 1 ? "\0" : "\0\0"));
    }
    if ($text ne '') {
	$cpout{$fh} .= $text;
    }
    elsif ($size) {
	&flush($fh) while ($l = length($cpout{$fh})) >= 5120;
	while (sysread(IN, $cpout{$fh}, 5120 - $l, $l)) {
	    &flush($fh);
	    $l = length($cpout{$fh});
	}
    }
    close IN;
}

sub flush {
    local($fh) = @_;

    while (length($cpout{$fh}) >= 5120) {
	syswrite($fh,$cpout{$fh},5120);
	++$blocks{$fh};
	substr($cpout{$fh}, 0, 5120) = '';
    }
}

sub flushall {
    $name = 'TRAILER!!!';
    foreach $fh (keys %cpout) {
	&cpio($nc{$fh},$fh);
	$cpout{$fh} .= "0" x (5120 - length($cpout{$fh}));
	&flush($fh);
	print $blocks{$fh} * 10, " blocks\n";
    }
}

END
}

if ($inittar) {
print <<'START', <<"INTERP", <<'END';
sub tar {
    local($fh) = @_;
    local($linkname,$header,$l,$slop);
    local($linkflag) = "\0";

START
    (\$dev,\$ino,\$mode,\$nlink,\$uid,\$gid,\$rdev,\$size,
      \$atime,\$mtime,\$ctime,\$blksize,\$blocks) = $stat\(_\);
INTERP
    $nm = $name;
    if ($nlink > 1) {
	if ($linkname = $linkseen{$fh,$dev,$ino}) {
	    $linkflag = 1;
	}
	else {
	    $linkseen{$fh,$dev,$ino} = $nm;
	}
    }
    if (-f _) {
	open(IN, "./$_\0") || do {
	    warn "Couldn't open $name: $!\n";
	    return;
	};
	$size = 0 if $linkflag ne "\0";
    }
    else {
	$linkname = readlink($_);
	$linkflag = 2 if defined $linkname;
	$nm .= '/' if -d _;
	$size = 0;
    }

    $header = pack("a100a8a8a8a12a12a8a1a100",
	$nm,
	sprintf("%6o ", $mode & 0777),
	sprintf("%6o ", $uid & 0777777),
	sprintf("%6o ", $gid & 0777777),
	sprintf("%11o ", $size),
	sprintf("%11o ", $mtime),
	"        ",
	$linkflag,
	$linkname);
    $l = length($header) % 512;
    substr($header, 148, 6) = sprintf("%6o", unpack("%16C*", $header));
    substr($header, 154, 1) = "\0";  # blech
    $tarout{$fh} .= $header;
    $tarout{$fh} .= "\0" x (512 - $l) if $l;
    if ($size) {
	&tflush($fh) while ($l = length($tarout{$fh})) >= 10240;
	while (sysread(IN, $tarout{$fh}, 10240 - $l, $l)) {
	    $slop = length($tarout{$fh}) % 512;
	    $tarout{$fh} .= "\0" x (512 - $slop) if $slop;
	    &tflush($fh);
	    $l = length($tarout{$fh});
	}
    }
    close IN;
}

sub tflush {
    local($fh) = @_;

    while (length($tarout{$fh}) >= 10240) {
	syswrite($fh,$tarout{$fh},10240);
	++$blocks{$fh};
	substr($tarout{$fh}, 0, 10240) = '';
    }
}

sub tflushall {
    local($len);

    foreach $fh (keys %tarout) {
	$len = 10240 - length($tarout{$fh});
	$len += 10240 if $len < 1024;
	$tarout{$fh} .= "\0" x $len;
	&tflush($fh);
    }
}

END
}

exit;

############################################################################

sub tab {
    local($tabstring);

    $tabstring = "\t" x ($indent / 2) . ' ' x ($indent % 2 * 4);
    if (!$statdone) {
	if ($_ =~ /^(name|print|prune|exec|ok|\(|\))/) {
	    $delayedstat++;
	}
	else {
	    if ($saw_or) {
		$tabstring .= <<"ENDOFSTAT" . $tabstring;
(\$nlink || ((\$dev,\$ino,\$mode,\$nlink,\$uid,\$gid) = $stat\(\$_\))) &&
ENDOFSTAT
	    }
	    else {
		$tabstring .= <<"ENDOFSTAT" . $tabstring;
((\$dev,\$ino,\$mode,\$nlink,\$uid,\$gid) = $stat\(\$_\)) &&
ENDOFSTAT
	    }
	    $statdone = 1;
	}
    }
    $tabstring =~ s/^\s+/ / if $out =~ /!$/;
    $tabstring;
}

sub fileglob_to_re {
    local($tmp) = @_;

    $tmp =~ s#([./^\$()])#\\$1#g;
    $tmp =~ s/([?*])/.$1/g;
    "^$tmp\$";
}

sub n {
    local($n) = @_;

    $n =~ s/^-/< / || $n =~ s/^\+/> / || $n =~ s/^/== /;
    $n =~ s/ 0*(\d)/ $1/;
    $n . ')';
}

sub quote {
    local($string) = @_;
    $string =~ s/'/\\'/;
    "'$string'";
}
!NO!SUBS!

close OUT or die "Can't close $file: $!";
chmod 0755, $file or die "Can't reset permissions for $file: $!\n";
exec("$Config{'eunicefix'} $file") if $Config{'eunicefix'} ne ':';
chdir $origdir;
