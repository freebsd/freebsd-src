package ExtUtils::Manifest;

require Exporter;
use Config;
use File::Find;
use File::Copy 'copy';
use Carp;
use strict;

use vars qw($VERSION @ISA @EXPORT_OK
	    $Is_VMS $Debug $Verbose $Quiet $MANIFEST $found);

$VERSION = substr(q$Revision: 1.33 $, 10);
@ISA=('Exporter');
@EXPORT_OK = ('mkmanifest', 'manicheck', 'fullcheck', 'filecheck', 
	      'skipcheck', 'maniread', 'manicopy');

$Is_VMS = $^O eq 'VMS';
if ($Is_VMS) { require File::Basename }

$Debug = 0;
$Verbose = 1;
$Quiet = 0;
$MANIFEST = 'MANIFEST';

# Really cool fix from Ilya :)
unless (defined $Config{d_link}) {
    *ln = \&cp;
}

sub mkmanifest {
    my $manimiss = 0;
    my $read = maniread() or $manimiss++;
    $read = {} if $manimiss;
    local *M;
    rename $MANIFEST, "$MANIFEST.bak" unless $manimiss;
    open M, ">$MANIFEST" or die "Could not open $MANIFEST: $!";
    my $matches = _maniskip();
    my $found = manifind();
    my($key,$val,$file,%all);
    %all = (%$found, %$read);
    $all{$MANIFEST} = ($Is_VMS ? "$MANIFEST\t\t" : '') . 'This list of files'
        if $manimiss; # add new MANIFEST to known file list
    foreach $file (sort keys %all) {
	next if &$matches($file);
	if ($Verbose){
	    warn "Added to $MANIFEST: $file\n" unless exists $read->{$file};
	}
	my $text = $all{$file};
	($file,$text) = split(/\s+/,$text,2) if $Is_VMS && $text;
	my $tabs = (5 - (length($file)+1)/8);
	$tabs = 1 if $tabs < 1;
	$tabs = 0 unless $text;
	print M $file, "\t" x $tabs, $text, "\n";
    }
    close M;
}

sub manifind {
    local $found = {};
    find(sub {return if -d $_;
	      (my $name = $File::Find::name) =~ s|./||;
	      warn "Debug: diskfile $name\n" if $Debug;
	      $name  =~ s#(.*)\.$#\L$1# if $Is_VMS;
	      $found->{$name} = "";}, ".");
    $found;
}

sub fullcheck {
    _manicheck(3);
}

sub manicheck {
    return @{(_manicheck(1))[0]};
}

sub filecheck {
    return @{(_manicheck(2))[1]};
}

sub skipcheck {
    _manicheck(6);
}

sub _manicheck {
    my($arg) = @_;
    my $read = maniread();
    my $found = manifind();
    my $file;
    my $dosnames=(defined(&Dos::UseLFN) && Dos::UseLFN()==0);
    my(@missfile,@missentry);
    if ($arg & 1){
	foreach $file (sort keys %$read){
	    warn "Debug: manicheck checking from $MANIFEST $file\n" if $Debug;
            if ($dosnames){
                $file = lc $file;
                $file =~ s=(\.(\w|-)+)=substr ($1,0,4)=ge;
                $file =~ s=((\w|-)+)=substr ($1,0,8)=ge;
            }
	    unless ( exists $found->{$file} ) {
		warn "No such file: $file\n" unless $Quiet;
		push @missfile, $file;
	    }
	}
    }
    if ($arg & 2){
	$read ||= {};
	my $matches = _maniskip();
	my $skipwarn = $arg & 4;
	foreach $file (sort keys %$found){
	    if (&$matches($file)){
		warn "Skipping $file\n" if $skipwarn;
		next;
	    }
	    warn "Debug: manicheck checking from disk $file\n" if $Debug;
	    unless ( exists $read->{$file} ) {
		warn "Not in $MANIFEST: $file\n" unless $Quiet;
		push @missentry, $file;
	    }
	}
    }
    (\@missfile,\@missentry);
}

sub maniread {
    my ($mfile) = @_;
    $mfile ||= $MANIFEST;
    my $read = {};
    local *M;
    unless (open M, $mfile){
	warn "$mfile: $!";
	return $read;
    }
    while (<M>){
	chomp;
	next if /^#/;
	if ($Is_VMS) {
	    my($file)= /^(\S+)/;
	    next unless $file;
	    my($base,$dir) = File::Basename::fileparse($file);
	    # Resolve illegal file specifications in the same way as tar
	    $dir =~ tr/./_/;
	    my(@pieces) = split(/\./,$base);
	    if (@pieces > 2) { $base = shift(@pieces) . '.' . join('_',@pieces); }
	    my $okfile = "$dir$base";
	    warn "Debug: Illegal name $file changed to $okfile\n" if $Debug;
	    $read->{"\L$okfile"}=$_;
	}
	else { /^(\S+)\s*(.*)/ and $read->{$1}=$2; }
    }
    close M;
    $read;
}

# returns an anonymous sub that decides if an argument matches
sub _maniskip {
    my ($mfile) = @_;
    my $matches = sub {0};
    my @skip ;
    $mfile ||= "$MANIFEST.SKIP";
    local *M;
    return $matches unless -f $mfile;
    open M, $mfile or return $matches;
    while (<M>){
	chomp;
	next if /^#/;
	next if /^\s*$/;
	push @skip, $_;
    }
    close M;
    my $opts = $Is_VMS ? 'oi ' : 'o ';
    my $sub = "\$matches = "
	. "sub { my(\$arg)=\@_; return 1 if "
	. join (" || ",  (map {s!/!\\/!g; "\$arg =~ m/$_/$opts"} @skip), 0)
	. " }";
    eval $sub;
    print "Debug: $sub\n" if $Debug;
    $matches;
}

sub manicopy {
    my($read,$target,$how)=@_;
    croak "manicopy() called without target argument" unless defined $target;
    $how ||= 'cp';
    require File::Path;
    require File::Basename;
    my(%dirs,$file);
    $target = VMS::Filespec::unixify($target) if $Is_VMS;
    umask 0 unless $Is_VMS;
    File::Path::mkpath([ $target ],1,$Is_VMS ? undef : 0755);
    foreach $file (keys %$read){
	$file = VMS::Filespec::unixify($file) if $Is_VMS;
	if ($file =~ m!/!) { # Ilya, that hurts, I fear, or maybe not?
	    my $dir = File::Basename::dirname($file);
	    $dir = VMS::Filespec::unixify($dir) if $Is_VMS;
	    File::Path::mkpath(["$target/$dir"],1,$Is_VMS ? undef : 0755);
	}
	cp_if_diff($file, "$target/$file", $how);
    }
}

sub cp_if_diff {
    my($from, $to, $how)=@_;
    -f $from or carp "$0: $from not found";
    my($diff) = 0;
    local(*F,*T);
    open(F,$from) or croak "Can't read $from: $!\n";
    if (open(T,$to)) {
	while (<F>) { $diff++,last if $_ ne <T>; }
	$diff++ unless eof(T);
	close T;
    }
    else { $diff++; }
    close F;
    if ($diff) {
	if (-e $to) {
	    unlink($to) or confess "unlink $to: $!";
	}
      STRICT_SWITCH: {
	    best($from,$to), last STRICT_SWITCH if $how eq 'best';
	    cp($from,$to), last STRICT_SWITCH if $how eq 'cp';
	    ln($from,$to), last STRICT_SWITCH if $how eq 'ln';
	    croak("ExtUtils::Manifest::cp_if_diff " .
		  "called with illegal how argument [$how]. " .
		  "Legal values are 'best', 'cp', and 'ln'.");
	}
    }
}

sub cp {
    my ($srcFile, $dstFile) = @_;
    my ($perm,$access,$mod) = (stat $srcFile)[2,8,9];
    copy($srcFile,$dstFile);
    utime $access, $mod + ($Is_VMS ? 1 : 0), $dstFile;
    # chmod a+rX-w,go-w
    chmod(  0444 | ( $perm & 0111 ? 0111 : 0 ),  $dstFile );
}

sub ln {
    my ($srcFile, $dstFile) = @_;
    return &cp if $Is_VMS;
    link($srcFile, $dstFile);
    local($_) = $dstFile; # chmod a+r,go-w+X (except "X" only applies to u=x)
    my $mode= 0444 | (stat)[2] & 0700;
    if (! chmod(  $mode | ( $mode & 0100 ? 0111 : 0 ),  $_  )) {
       unlink $dstFile;
       return;
    }
    1;
}

sub best {
    my ($srcFile, $dstFile) = @_;
    if (-l $srcFile) {
	cp($srcFile, $dstFile);
    } else {
	ln($srcFile, $dstFile) or cp($srcFile, $dstFile);
    }
}

1;

__END__

=head1 NAME

ExtUtils::Manifest - utilities to write and check a MANIFEST file

=head1 SYNOPSIS

C<require ExtUtils::Manifest;>

C<ExtUtils::Manifest::mkmanifest;>

C<ExtUtils::Manifest::manicheck;>

C<ExtUtils::Manifest::filecheck;>

C<ExtUtils::Manifest::fullcheck;>

C<ExtUtils::Manifest::skipcheck;>

C<ExtUtild::Manifest::manifind();>

C<ExtUtils::Manifest::maniread($file);>

C<ExtUtils::Manifest::manicopy($read,$target,$how);>

=head1 DESCRIPTION

Mkmanifest() writes all files in and below the current directory to a
file named in the global variable $ExtUtils::Manifest::MANIFEST (which
defaults to C<MANIFEST>) in the current directory. It works similar to

    find . -print

but in doing so checks each line in an existing C<MANIFEST> file and
includes any comments that are found in the existing C<MANIFEST> file
in the new one. Anything between white space and an end of line within
a C<MANIFEST> file is considered to be a comment. Filenames and
comments are separated by one or more TAB characters in the
output. All files that match any regular expression in a file
C<MANIFEST.SKIP> (if such a file exists) are ignored.

Manicheck() checks if all the files within a C<MANIFEST> in the
current directory really do exist. It only reports discrepancies and
exits silently if MANIFEST and the tree below the current directory
are in sync.

Filecheck() finds files below the current directory that are not
mentioned in the C<MANIFEST> file. An optional file C<MANIFEST.SKIP>
will be consulted. Any file matching a regular expression in such a
file will not be reported as missing in the C<MANIFEST> file.

Fullcheck() does both a manicheck() and a filecheck().

Skipcheck() lists all the files that are skipped due to your
C<MANIFEST.SKIP> file.

Manifind() returns a hash reference. The keys of the hash are the
files found below the current directory.

Maniread($file) reads a named C<MANIFEST> file (defaults to
C<MANIFEST> in the current directory) and returns a HASH reference
with files being the keys and comments being the values of the HASH.
Blank lines and lines which start with C<#> in the C<MANIFEST> file
are discarded.

I<Manicopy($read,$target,$how)> copies the files that are the keys in
the HASH I<%$read> to the named target directory. The HASH reference
I<$read> is typically returned by the maniread() function. This
function is useful for producing a directory tree identical to the
intended distribution tree. The third parameter $how can be used to
specify a different methods of "copying". Valid values are C<cp>,
which actually copies the files, C<ln> which creates hard links, and
C<best> which mostly links the files but copies any symbolic link to
make a tree without any symbolic link. Best is the default.

=head1 MANIFEST.SKIP

The file MANIFEST.SKIP may contain regular expressions of files that
should be ignored by mkmanifest() and filecheck(). The regular
expressions should appear one on each line. Blank lines and lines
which start with C<#> are skipped.  Use C<\#> if you need a regular
expression to start with a sharp character. A typical example:

    \bRCS\b
    ^MANIFEST\.
    ^Makefile$
    ~$
    \.html$
    \.old$
    ^blib/
    ^MakeMaker-\d

=head1 EXPORT_OK

C<&mkmanifest>, C<&manicheck>, C<&filecheck>, C<&fullcheck>,
C<&maniread>, and C<&manicopy> are exportable.

=head1 GLOBAL VARIABLES

C<$ExtUtils::Manifest::MANIFEST> defaults to C<MANIFEST>. Changing it
results in both a different C<MANIFEST> and a different
C<MANIFEST.SKIP> file. This is useful if you want to maintain
different distributions for different audiences (say a user version
and a developer version including RCS).

C<$ExtUtils::Manifest::Quiet> defaults to 0. If set to a true value,
all functions act silently.

=head1 DIAGNOSTICS

All diagnostic output is sent to C<STDERR>.

=over

=item C<Not in MANIFEST:> I<file>

is reported if a file is found, that is missing in the C<MANIFEST>
file which is excluded by a regular expression in the file
C<MANIFEST.SKIP>.

=item C<No such file:> I<file>

is reported if a file mentioned in a C<MANIFEST> file does not
exist.

=item C<MANIFEST:> I<$!>

is reported if C<MANIFEST> could not be opened.

=item C<Added to MANIFEST:> I<file>

is reported by mkmanifest() if $Verbose is set and a file is added
to MANIFEST. $Verbose is set to 1 by default.

=back

=head1 SEE ALSO

L<ExtUtils::MakeMaker> which has handy targets for most of the functionality.

=head1 AUTHOR

Andreas Koenig <F<koenig@franz.ww.TU-Berlin.DE>>

=cut
