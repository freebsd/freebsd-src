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
(my \$perlpath = <<'/../') =~ s/\\s*\\z//;
$Config{perlpath}
/../
!GROK!THIS!

# In the following, perl variables are not expanded during extraction.

print OUT <<'!NO!SUBS!';
use strict;
use vars qw/$statdone/;
use File::Spec::Functions 'curdir';
my $startperl = "#! $perlpath -w";

#
# Modified September 26, 1993 to provide proper handling of years after 1999
#   Tom Link <tml+@pitt.edu>
#   University of Pittsburgh
#
# Modified April 7, 1998 with nasty hacks to implement the troublesome -follow
#  Billy Constantine <wdconsta@cs.adelaide.edu.au> <billy@smug.adelaide.edu.au>
#  University of Adelaide, Adelaide, South Australia
#
# Modified 1999-06-10, 1999-07-07 to migrate to cleaner perl5 usage
#   Ken Pizzini <ken@halcyon.com>
#
# Modified 2000-01-28 to use the 'follow' option of File::Find

my @roots = ();
while ($ARGV[0] =~ /^[^-!(]/) {
    push(@roots, shift);
}
@roots = (curdir()) unless @roots;
for (@roots) { $_ = &quote($_) }
my $roots = join(', ', @roots);

my $find = "find";
my $indent_depth = 1;
my $stat = 'lstat';
my $decl = '';
my $flushall = '';
my $initfile = '';
my $initnewer = '';
my $out = '';
my %init = ();
my ($follow_in_effect,$Skip_And) = (0,0);

while (@ARGV) {
    $_ = shift;
    s/^-// || /^[()!]/ || die "Unrecognized switch: $_\n";
    if ($_ eq '(') {
        $out .= &tab . "(\n";
        $indent_depth++;
        next;
    } elsif ($_ eq ')') {
        --$indent_depth;
        $out .= &tab . ")";
    } elsif ($_ eq 'follow') {
        $follow_in_effect= 1;
        $stat = 'stat';
        $Skip_And= 1;
    } elsif ($_ eq '!') {
        $out .= &tab . "!";
        next;
    } elsif ($_ eq 'name') {
        $out .= &tab . '/' . &fileglob_to_re(shift) . "/s";
    } elsif ($_ eq 'perm') {
        my $onum = shift;
        $onum =~ /^-?[0-7]+$/
            || die "Malformed -perm argument: $onum\n";
        $out .= &tab;
        if ($onum =~ s/^-//) {
            $onum = sprintf("0%o", oct($onum) & 07777);
            $out .= "((\$mode & $onum) == $onum)";
        } else {
            $onum =~ s/^0*/0/;
            $out .= "((\$mode & 0777) == $onum)";
        }
    } elsif ($_ eq 'type') {
        (my $filetest = shift) =~ tr/s/S/;
        $out .= &tab . "-$filetest _";
    } elsif ($_ eq 'print') {
        $out .= &tab . 'print("$name\n")';
    } elsif ($_ eq 'print0') {
        $out .= &tab . 'print("$name\0")';
    } elsif ($_ eq 'fstype') {
        my $type = shift;
        $out .= &tab;
        if ($type eq 'nfs') {
            $out .= '($dev < 0)';
        } else {
            $out .= '($dev >= 0)'; #XXX
        }
    } elsif ($_ eq 'user') {
        my $uname = shift;
        $out .= &tab . "(\$uid == \$uid{'$uname'})";
        $init{user} = 1;
    } elsif ($_ eq 'group') {
        my $gname = shift;
        $out .= &tab . "(\$gid == \$gid{'$gname'})";
        $init{group} = 1;
    } elsif ($_ eq 'nouser') {
        $out .= &tab . '!exists $uid{$uid}';
        $init{user} = 1;
    } elsif ($_ eq 'nogroup') {
        $out .= &tab . '!exists $gid{$gid}';
        $init{group} = 1;
    } elsif ($_ eq 'links') {
        $out .= &tab . &n('$nlink', shift);
    } elsif ($_ eq 'inum') {
        $out .= &tab . &n('$ino', shift);
    } elsif ($_ eq 'size') {
        $_ = shift;
        my $n = 'int(((-s _) + 511) / 512)';
        if (s/c\z//) {
            $n = 'int(-s _)';
        } elsif (s/k\z//) {
            $n = 'int(((-s _) + 1023) / 1024)';
        }
        $out .= &tab . &n($n, $_);
    } elsif ($_ eq 'atime') {
        $out .= &tab . &n('int(-A _)', shift);
    } elsif ($_ eq 'mtime') {
        $out .= &tab . &n('int(-M _)', shift);
    } elsif ($_ eq 'ctime') {
        $out .= &tab . &n('int(-C _)', shift);
    } elsif ($_ eq 'exec') {
        my @cmd = ();
        while (@ARGV && $ARGV[0] ne ';')
            { push(@cmd, shift) }
        shift;
        $out .= &tab;
        if ($cmd[0] =~m#^(?:(?:/usr)?/bin/)?rm$#
                && $cmd[$#cmd] eq '{}'
                && (@cmd == 2 || (@cmd == 3 && $cmd[1] eq '-f'))) {
            if (@cmd == 2) {
                $out .= '(unlink($_) || warn "$name: $!\n")';
            } elsif (!@ARGV) {
                $out .= 'unlink($_)';
            } else {
                $out .= '(unlink($_) || 1)';
            }
        } else {
            for (@cmd)
                { s/'/\\'/g }
            { local $" = "','"; $out .= "&doexec(0, '@cmd')"; }
            $init{doexec} = 1;
        }
    } elsif ($_ eq 'ok') {
        my @cmd = ();
        while (@ARGV && $ARGV[0] ne ';')
            { push(@cmd, shift) }
        shift;
        $out .= &tab;
        for (@cmd)
            { s/'/\\'/g }
        { local $" = "','"; $out .= "&doexec(0, '@cmd')"; }
        $init{doexec} = 1;
    } elsif ($_ eq 'prune') {
        $out .= &tab . '($File::Find::prune = 1)';
    } elsif ($_ eq 'xdev') {
        $out .= &tab . '!($File::Find::prune |= ($dev != $File::Find::topdev))'
;
    } elsif ($_ eq 'newer') {
        my $file = shift;
        my $newername = 'AGE_OF' . $file;
        $newername =~ s/\W/_/g;
        $newername = '$' . $newername;
        $out .= &tab . "(-M _ < $newername)";
        $initnewer .= "my $newername = -M " . &quote($file) . ";\n";
    } elsif ($_ eq 'eval') {
        my $prog = shift;
        $prog =~ s/'/\\'/g;
        $out .= &tab . "eval {$prog}";
    } elsif ($_ eq 'depth') {
        $find = 'finddepth';
        next;
    } elsif ($_ eq 'ls') {
        $out .= &tab . "&ls";
        $init{ls} = 1;
    } elsif ($_ eq 'tar') {
        die "-tar must have a filename argument\n" unless @ARGV;
        my $file = shift;
        my $fh = 'FH' . $file;
        $fh =~ s/\W/_/g;
        $out .= &tab . "&tar(*$fh, \$name)";
        $flushall .= "&tflushall;\n";
        $initfile .= "open($fh, " . &quote('> ' . $file) .
                     qq{) || die "Can't open $fh: \$!\\n";\n};
        $init{tar} = 1;
    } elsif (/^(n?)cpio\z/) {
        die "-$_ must have a filename argument\n" unless @ARGV;
        my $file = shift;
        my $fh = 'FH' . $file;
        $fh =~ s/\W/_/g;
        $out .= &tab . "&cpio(*$fh, \$name, '$1')";
        $find = 'finddepth';
        $flushall .= "&cflushall;\n";
        $initfile .= "open($fh, " . &quote('> ' . $file) .
                     qq{) || die "Can't open $fh: \$!\\n";\n};
        $init{cpio} = 1;
    } else {
        die "Unrecognized switch: -$_\n";
    }

    if (@ARGV) {
        if ($ARGV[0] eq '-o') {
            { local($statdone) = 1; $out .= "\n" . &tab . "||\n"; }
            $statdone = 0 if $indent_depth == 1 && exists $init{delayedstat};
            $init{saw_or} = 1;
            shift;
        } else {
            $out .= " &&" unless $Skip_And || $ARGV[0] eq ')';
            $out .= "\n";
            shift if $ARGV[0] eq '-a';
        }
    }
}


print <<"END";
$startperl
    eval 'exec $perlpath -S \$0 \${1+"\$@"}'
        if 0; #\$running_under_some_shell

use strict;
use File::Find ();

# Set the variable \$File::Find::dont_use_nlink if you're using AFS,
# since AFS cheats.

# for the convenience of &wanted calls, including -eval statements:
use vars qw/*name *dir *prune/;
*name   = *File::Find::name;
*dir    = *File::Find::dir;
*prune  = *File::Find::prune;

END


if (exists $init{ls}) {
    print <<'END';
my @rwx = qw(--- --x -w- -wx r-- r-x rw- rwx);
my @moname = qw(Jan Feb Mar Apr May Jun Jul Aug Sep Oct Nov Dec);

END
}

if (exists $init{user} || exists $init{ls} || exists $init{tar}) {
    print "my (%uid, %user);\n";
    print "while (my (\$name, \$pw, \$uid) = getpwent) {\n";
    print '    $uid{$name} = $uid{$uid} = $uid;', "\n"
        if exists $init{user};
    print '    $user{$uid} = $name unless exists $user{$uid};', "\n"
        if exists $init{ls} || exists $init{tar};
    print "}\n\n";
}

if (exists $init{group} || exists $init{ls} || exists $init{tar}) {
    print "my (%gid, %group);\n";
    print "while (my (\$name, \$pw, \$gid) = getgrent) {\n";
    print '    $gid{$name} = $gid{$gid} = $gid;', "\n"
        if exists $init{group};
    print '    $group{$gid} = $name unless exists $group{$gid};', "\n"
        if exists $init{ls} || exists $init{tar};
    print "}\n\n";
}

print $initnewer, "\n" if $initnewer ne '';
print $initfile, "\n" if $initfile ne '';
$flushall .= "exit;\n";
if (exists $init{declarestat}) {
    $out = <<'END' . $out;
    my ($dev,$ino,$mode,$nlink,$uid,$gid);

END
}

if ( $follow_in_effect ) {
$out =~ s/lstat\(\$_\)/lstat(_)/;
print <<"END";
$decl
# Traverse desired filesystems
File::Find::$find( {wanted => \\&wanted, follow => 1}, $roots);
$flushall

sub wanted {
$out;
}

END
} else {
print <<"END";
$decl
# Traverse desired filesystems
File::Find::$find({wanted => \\&wanted}, $roots);
$flushall

sub wanted {
$out;
}

END
}

if (exists $init{doexec}) {
    print <<'END';

use Cwd ();
my $cwd = Cwd::cwd();

sub doexec {
    my $ok = shift;
    for my $word (@_)
        { $word =~ s#{}#$name#g }
    if ($ok) {
        my $old = select(STDOUT);
        $| = 1;
        print "@_";
        select($old);
        return 0 unless <STDIN> =~ /^y/;
    }
    chdir $cwd; #sigh
    system @_;
    chdir $File::Find::dir;
    return !$?;
}

END
}

if (exists $init{ls}) {
    print <<'INTRO', <<"SUB", <<'END';

sub sizemm {
    my $rdev = shift;
    sprintf("%3d, %3d", ($rdev >> 8) & 0xff, $rdev & 0xff);
}

sub ls {
    my ($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,
INTRO
        \$atime,\$mtime,\$ctime,\$blksize,\$blocks) = $stat(_);
SUB
    my $pname = $name;

    $blocks
        or $blocks = int(($size + 1023) / 1024);

    my $perms = $rwx[$mode & 7];
    $mode >>= 3;
    $perms = $rwx[$mode & 7] . $perms;
    $mode >>= 3;
    $perms = $rwx[$mode & 7] . $perms;
    substr($perms, 2, 1) =~ tr/-x/Ss/ if -u _;
    substr($perms, 5, 1) =~ tr/-x/Ss/ if -g _;
    substr($perms, 8, 1) =~ tr/-x/Tt/ if -k _;
    if    (-f _) { $perms = '-' . $perms; }
    elsif (-d _) { $perms = 'd' . $perms; }
    elsif (-l _) { $perms = 'l' . $perms; $pname .= ' -> ' . readlink($_); }
    elsif (-c _) { $perms = 'c' . $perms; $size = sizemm($rdev); }
    elsif (-b _) { $perms = 'b' . $perms; $size = sizemm($rdev); }
    elsif (-p _) { $perms = 'p' . $perms; }
    elsif (-S _) { $perms = 's' . $perms; }
    else         { $perms = '?' . $perms; }

    my $user = $user{$uid} || $uid;
    my $group = $group{$gid} || $gid;

    my ($sec,$min,$hour,$mday,$mon,$timeyear) = localtime($mtime);
    if (-M _ > 365.25 / 2) {
        $timeyear += 1900;
    } else {
        $timeyear = sprintf("%02d:%02d", $hour, $min);
    }

    printf "%5lu %4ld %-10s %3d %-8s %-8s %8s %s %2d %5s %s\n",
            $ino,
                 $blocks,
                      $perms,
                            $nlink,
                                $user,
                                     $group,
                                          $size,
                                              $moname[$mon],
                                                 $mday,
                                                     $timeyear,
                                                         $pname;
    1;
}

END
}


if (exists $init{cpio} || exists $init{tar}) {
print <<'END';

my %blocks = ();

sub flush {
    my ($fh, $varref, $blksz) = @_;

    while (length($$varref) >= $blksz) {
        no strict qw/refs/;
        syswrite($fh, $$varref, $blksz);
        substr($$varref, 0, $blksz) = '';
        ++$blocks{$fh};
    }
}

END
}


if (exists $init{cpio}) {
    print <<'INTRO', <<"SUB", <<'END';

my %cpout = ();
my %nc = ();

sub cpio {
    my ($fh, $fname, $nc) = @_;
    my $text = '';
    my ($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,
        $atime,$mtime,$ctime,$blksize,$blocks);
    local (*IN);

    if ( ! defined $fname ) {
        $fname = 'TRAILER!!!';
        ($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,
          $atime,$mtime,$ctime,$blksize,$blocks) = (0) x 13;
    } else {
        ($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,
INTRO
          \$atime,\$mtime,\$ctime,\$blksize,\$blocks) = $stat(_);
SUB
        if (-f _) {
            open(IN, "./$_\0") || do {
                warn "Couldn't open $fname: $!\n";
                return;
            }
        } else {
            $text = readlink($_);
            $size = 0 unless defined $text;
        }
    }

    $fname =~ s#^\./##;
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
            length($fname)+1,
            $size,
            $fname);
    } else {
        $cpout{$fh} .= "\0" if length($cpout{$fh}) & 1;
        $cpout{$fh} .= pack("SSSSSSSSLSLa*",
            070707, $dev, $ino, $mode, $uid, $gid, $nlink, $rdev, $mtime,
            length($fname)+1, $size,
            $fname . (length($fname) & 1 ? "\0" : "\0\0"));
    }

    if ($text ne '') {
        $cpout{$fh} .= $text;
    } elsif ($size) {
        my $l;
        flush($fh, \$cpout{$fh}, 5120)
            while ($l = length($cpout{$fh})) >= 5120;
        while (sysread(IN, $cpout{$fh}, 5120 - $l, $l)) {
            flush($fh, \$cpout{$fh}, 5120);
            $l = length($cpout{$fh});
        }
        close IN;
    }
}

sub cflushall {
    for my $fh (keys %cpout) {
        &cpio($fh, undef, $nc{$fh});
        $cpout{$fh} .= "0" x (5120 - length($cpout{$fh}));
        flush($fh, \$cpout{$fh}, 5120);
        print $blocks{$fh} * 10, " blocks\n";
    }
}

END
}

if (exists $init{tar}) {
    print <<'INTRO', <<"SUB", <<'END';

my %tarout = ();
my %linkseen = ();

sub tar {
    my ($fh, $fname) = @_;
    my $prefix = '';
    my $typeflag = '0';
    my $linkname;
    my ($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,
INTRO
        \$atime,\$mtime,\$ctime,\$blksize,\$blocks) = $stat(_);
SUB
    local (*IN);

    if ($nlink > 1) {
        if ($linkname = $linkseen{$fh, $dev, $ino}) {
            if (length($linkname) > 100) {
                warn "$0: omitting file with linkname ",
                     "too long for tar output: $linkname\n";
                return;
            }
            $typeflag = '1';
            $size = 0;
        } else {
            $linkseen{$fh, $dev, $ino} = $fname;
        }
    }
    if ($typeflag eq '0') {
        if (-f _) {
            open(IN, "./$_\0") || do {
                warn "Couldn't open $fname: $!\n";
                return;
            }
        } else {
            $linkname = readlink($_);
            if (defined $linkname) { $typeflag = '2' }
            elsif (-c _) { $typeflag = '3' }
            elsif (-b _) { $typeflag = '4' }
            elsif (-d _) { $typeflag = '5' }
            elsif (-p _) { $typeflag = '6' }
        }
    }

    if (length($fname) > 100) {
        ($prefix, $fname) = ($fname =~ m#\A(.*?)/(.{,100})\Z(?!\n)#);
        if (!defined($fname) || length($prefix) > 155) {
            warn "$0: omitting file with name too long for tar output: ",
                 $fname, "\n";
            return;
        }
    }

    $size = 0 if $typeflag ne '0';
    my $header = pack("a100a8a8a8a12a12a8a1a100a6a2a32a32a8a8a155",
                        $fname,
                        sprintf("%7o ", $mode &    0777),
                        sprintf("%7o ", $uid  & 0777777),
                        sprintf("%7o ", $gid  & 0777777),
                        sprintf("%11o ", $size),
                        sprintf("%11o ", $mtime),
                        ' 'x8,
                        $typeflag,
                        defined $linkname ? $linkname : '',
                        "ustar\0",
                        "00",
                        $user{$uid},
                        $group{$gid},
                        ($rdev >> 8) & 0xff,
                        $rdev & 0xff,
                        $prefix,
                     );
    substr($header, 148, 8) = sprintf("%7o ", unpack("%16C*", $header));
    my $l = length($header) % 512;
    $tarout{$fh} .= $header;
    $tarout{$fh} .= "\0" x (512 - $l) if $l;

    if ($size) {
        flush($fh, \$tarout{$fh}, 10240)
            while ($l = length($tarout{$fh})) >= 10240;
        while (sysread(IN, $tarout{$fh}, 10240 - $l, $l)) {
            my $slop = length($tarout{$fh}) % 512;
            $tarout{$fh} .= "\0" x (512 - $slop) if $slop;
            flush($fh, \$tarout{$fh}, 10240);
            $l = length($tarout{$fh});
        }
        close IN;
    }
}

sub tflushall {
    my $len;
    for my $fh (keys %tarout) {
        $len = 10240 - length($tarout{$fh});
        $len += 10240 if $len < 1024;
        $tarout{$fh} .= "\0" x $len;
        flush($fh, \$tarout{$fh}, 10240);
    }
}

END
}

exit;

############################################################################

sub tab {
    my $tabstring;

    $tabstring = "\t" x ($indent_depth/2) . ' ' x ($indent_depth%2 * 4);
    if (!$statdone) {
        if ($_ =~ /^(?:name|print|prune|exec|ok|\(|\))/) {
            $init{delayedstat} = 1;
        } else {
            my $statcall = '(($dev,$ino,$mode,$nlink,$uid,$gid) = '
                         . $stat . '($_))';
            if (exists $init{saw_or}) {
                $tabstring .= "(\$nlink || $statcall) &&\n" . $tabstring;
            } else {
                $tabstring .= "$statcall &&\n" . $tabstring;
            }
            $statdone = 1;
            $init{declarestat} = 1;
        }
    }
    $tabstring =~ s/^\s+/ / if $out =~ /!$/;
    $tabstring;
}

sub fileglob_to_re {
    my $x = shift;
    $x =~ s#([./^\$()])#\\$1#g;
    $x =~ s#([?*])#.$1#g;
    "^$x\\z";
}

sub n {
    my ($pre, $n) = @_;
    $n =~ s/^-/< / || $n =~ s/^\+/> / || $n =~ s/^/== /;
    $n =~ s/ 0*(\d)/ $1/;
    "($pre $n)";
}

sub quote {
    my $string = shift;
    $string =~ s/\\/\\\\/g;
    $string =~ s/'/\\'/g;
    "'$string'";
}

__END__

=head1 NAME

find2perl - translate find command lines to Perl code

=head1 SYNOPSIS

	find2perl [paths] [predicates] | perl

=head1 DESCRIPTION

find2perl is a little translator to convert find command lines to
equivalent Perl code.  The resulting code is typically faster than
running find itself.

"paths" are a set of paths where find2perl will start its searches and
"predicates" are taken from the following list.

=over 4

=item C<! PREDICATE>

Negate the sense of the following predicate.  The C<!> must be passed as
a distinct argument, so it may need to be surrounded by whitespace and/or
quoted from interpretation by the shell using a backslash (just as with
using C<find(1)>).

=item C<( PREDICATES )>

Group the given PREDICATES.  The parentheses must be passed as distinct
arguments, so they may need to be surrounded by whitespace and/or
quoted from interpretation by the shell using a backslash (just as with
using C<find(1)>).

=item C<PREDICATE1 PREDICATE2>

True if _both_ PREDICATE1 and PREDICATE2 are true; PREDICATE2 is not
evaluated if PREDICATE1 is false.

=item C<PREDICATE1 -o PREDICATE2>

True if either one of PREDICATE1 or PREDICATE2 is true; PREDICATE2 is
not evaluated if PREDICATE1 is true.

=item C<-follow>

Follow (dereference) symlinks.  The checking of file attributes depends
on the position of the C<-follow> option. If it precedes the file
check option, an C<stat> is done which means the file check applies to the
file the symbolic link is pointing to. If C<-follow> option follows the
file check option, this now applies to the symbolic link itself, i.e.
an C<lstat> is done.

=item C<-depth>

Change directory traversal algorithm from breadth-first to depth-first.

=item C<-prune>

Do not descend into the directory currently matched.

=item C<-xdev>

Do not traverse mount points (prunes search at mount-point directories).

=item C<-name GLOB>

File name matches specified GLOB wildcard pattern.  GLOB may need to be
quoted to avoid interpretation by the shell (just as with using
C<find(1)>).

=item C<-perm PERM>

Low-order 9 bits of permission match octal value PERM.

=item C<-perm -PERM>

The bits specified in PERM are all set in file's permissions.

=item C<-type X>

The file's type matches perl's C<-X> operator.

=item C<-fstype TYPE>

Filesystem of current path is of type TYPE (only NFS/non-NFS distinction
is implemented).

=item C<-user USER>

True if USER is owner of file.

=item C<-group GROUP>

True if file's group is GROUP.

=item C<-nouser>

True if file's owner is not in password database.

=item C<-nogroup>

True if file's group is not in group database.

=item C<-inum INUM>

True file's inode number is INUM.

=item C<-links N>

True if (hard) link count of file matches N (see below).

=item C<-size N>

True if file's size matches N (see below) N is normally counted in
512-byte blocks, but a suffix of "c" specifies that size should be
counted in characters (bytes) and a suffix of "k" specifes that
size should be counted in 1024-byte blocks.

=item C<-atime N>

True if last-access time of file matches N (measured in days) (see
below).

=item C<-ctime N>

True if last-changed time of file's inode matches N (measured in days,
see below).

=item C<-mtime N>

True if last-modified time of file matches N (measured in days, see below).

=item C<-newer FILE>

True if last-modified time of file matches N.

=item C<-print>

Print out path of file (always true).

=item C<-print0>

Like -print, but terminates with \0 instead of \n.

=item C<-exec OPTIONS ;>

exec() the arguments in OPTIONS in a subprocess; any occurence of {} in
OPTIONS will first be substituted with the path of the current
file.  Note that the command "rm" has been special-cased to use perl's
unlink() function instead (as an optimization).  The C<;> must be passed as
a distinct argument, so it may need to be surrounded by whitespace and/or
quoted from interpretation by the shell using a backslash (just as with
using C<find(1)>).

=item C<-ok OPTIONS ;>

Like -exec, but first prompts user; if user's response does not begin
with a y, skip the exec.  The C<;> must be passed as
a distinct argument, so it may need to be surrounded by whitespace and/or
quoted from interpretation by the shell using a backslash (just as with
using C<find(1)>).

=item C<-eval EXPR>

Has the perl script eval() the EXPR.  

=item C<-ls>

Simulates C<-exec ls -dils {} ;>

=item C<-tar FILE>

Adds current output to tar-format FILE.

=item C<-cpio FILE>

Adds current output to old-style cpio-format FILE.

=item C<-ncpio FILE>

Adds current output to "new"-style cpio-format FILE.

=back

Predicates which take a numeric argument N can come in three forms:

   * N is prefixed with a +: match values greater than N
   * N is prefixed with a -: match values less than N
   * N is not prefixed with either + or -: match only values equal to N

=head1 SEE ALSO

find

=cut
!NO!SUBS!

close OUT or die "Can't close $file: $!";
chmod 0755, $file or die "Can't reset permissions for $file: $!\n";
exec("$Config{'eunicefix'} $file") if $Config{'eunicefix'} ne ':';
chdir $origdir;
