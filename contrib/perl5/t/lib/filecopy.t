#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

print "1..11\n";

$| = 1;

use File::Copy;

# First we create a file
open(F, ">file-$$") or die;
binmode F; # for DOSISH platforms, because test 3 copies to stdout
print F "ok 3\n";
close F;

copy "file-$$", "copy-$$";

open(F, "copy-$$") or die;
$foo = <F>;
close(F);

print "not " if -s "file-$$" != -s "copy-$$";
print "ok 1\n";

print "not " unless $foo eq "ok 3\n";
print "ok 2\n";

binmode STDOUT unless $^O eq 'VMS';			# Copy::copy works in binary mode
copy "copy-$$", \*STDOUT;
unlink "copy-$$" or die "unlink: $!";

open(F,"file-$$");
copy(*F, "copy-$$");
open(R, "copy-$$") or die "open copy-$$: $!"; $foo = <R>; close(R);
print "not " unless $foo eq "ok 3\n";
print "ok 4\n";
unlink "copy-$$" or die "unlink: $!";
open(F,"file-$$");
copy(\*F, "copy-$$");
close(F) or die "close: $!";
open(R, "copy-$$") or die; $foo = <R>; close(R) or die "close: $!";
print "not " unless $foo eq "ok 3\n";
print "ok 5\n";
unlink "copy-$$" or die "unlink: $!";

require IO::File;
$fh = IO::File->new(">copy-$$") or die "Cannot open copy-$$:$!";
binmode $fh or die;
copy("file-$$",$fh);
$fh->close or die "close: $!";
open(R, "copy-$$") or die; $foo = <R>; close(R);
print "# foo=`$foo'\nnot " unless $foo eq "ok 3\n";
print "ok 6\n";
unlink "copy-$$" or die "unlink: $!";
require FileHandle;
my $fh = FileHandle->new(">copy-$$") or die "Cannot open copy-$$:$!";
binmode $fh or die;
copy("file-$$",$fh);
$fh->close;
open(R, "copy-$$") or die; $foo = <R>; close(R);
print "not " unless $foo eq "ok 3\n";
print "ok 7\n";
unlink "file-$$" or die "unlink: $!";

print "# moved missing file.\nnot " if move("file-$$", "copy-$$");
print "# target disappeared.\nnot " if not -e "copy-$$";
print "ok 8\n";

move "copy-$$", "file-$$" or print "# move did not succeed.\n";
print "# not moved: $!\nnot " unless -e "file-$$" and not -e "copy-$$";
open(R, "file-$$") or die; $foo = <R>; close(R);
print "# foo=`$foo'\nnot " unless $foo eq "ok 3\n";
print "ok 9\n";

copy "file-$$", "lib";
open(R, "lib/file-$$") or die; $foo = <R>; close(R);
print "not " unless $foo eq "ok 3\n";
print "ok 10\n";
unlink "lib/file-$$" or die "unlink: $!";

move "file-$$", "lib";
open(R, "lib/file-$$") or die "open lib/file-$$: $!"; $foo = <R>; close(R);
print "not " unless $foo eq "ok 3\n" and not -e "file-$$";;
print "ok 11\n";
unlink "lib/file-$$" or die "unlink: $!";

