#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

$| = 1;

my @pass = (0,1);
my $tests = 11;
printf "1..%d\n", $tests * scalar(@pass);

use File::Copy;

for my $pass (@pass) {

  require File::Copy;

  my $loopconst = $pass*$tests;

  # First we create a file
  open(F, ">file-$$") or die;
  binmode F; # for DOSISH platforms, because test 3 copies to stdout
  printf F "ok %d\n", 3 + $loopconst;
  close F;

  copy "file-$$", "copy-$$";

  open(F, "copy-$$") or die;
  $foo = <F>;
  close(F);

  print "not " if -s "file-$$" != -s "copy-$$";
  printf "ok %d\n", 1 + $loopconst;

  print "not " unless $foo eq sprintf "ok %d\n", 3+$loopconst;
  printf "ok %d\n", 2+$loopconst;

  binmode STDOUT unless $^O eq 'VMS'; # Copy::copy works in binary mode
  copy "copy-$$", \*STDOUT;
  unlink "copy-$$" or die "unlink: $!";

  open(F,"file-$$");
  copy(*F, "copy-$$");
  open(R, "copy-$$") or die "open copy-$$: $!"; $foo = <R>; close(R);
  print "not " unless $foo eq sprintf "ok %d\n", 3+$loopconst;
  printf "ok %d\n", 4+$loopconst;
  unlink "copy-$$" or die "unlink: $!";
  open(F,"file-$$");
  copy(\*F, "copy-$$");
  close(F) or die "close: $!";
  open(R, "copy-$$") or die; $foo = <R>; close(R) or die "close: $!";
  print "not " unless $foo eq sprintf "ok %d\n", 3+$loopconst;
  printf "ok %d\n", 5+$loopconst;
  unlink "copy-$$" or die "unlink: $!";

  require IO::File;
  $fh = IO::File->new(">copy-$$") or die "Cannot open copy-$$:$!";
  binmode $fh or die;
  copy("file-$$",$fh);
  $fh->close or die "close: $!";
  open(R, "copy-$$") or die; $foo = <R>; close(R);
  print "# foo=`$foo'\nnot " unless $foo eq sprintf "ok %d\n", 3+$loopconst;
  printf "ok %d\n", 6+$loopconst;
  unlink "copy-$$" or die "unlink: $!";
  require FileHandle;
  my $fh = FileHandle->new(">copy-$$") or die "Cannot open copy-$$:$!";
  binmode $fh or die;
  copy("file-$$",$fh);
  $fh->close;
  open(R, "copy-$$") or die; $foo = <R>; close(R);
  print "not " unless $foo eq sprintf "ok %d\n", 3+$loopconst;
  printf "ok %d\n", 7+$loopconst;
  unlink "file-$$" or die "unlink: $!";

  print "# moved missing file.\nnot " if move("file-$$", "copy-$$");
  print "# target disappeared.\nnot " if not -e "copy-$$";
  printf "ok %d\n", 8+$loopconst;

  move "copy-$$", "file-$$" or print "# move did not succeed.\n";
  print "# not moved: $!\nnot " unless -e "file-$$" and not -e "copy-$$";
  open(R, "file-$$") or die; $foo = <R>; close(R);
  print "# foo=`$foo'\nnot " unless $foo eq sprintf "ok %d\n", 3+$loopconst;
  printf "ok %d\n", 9+$loopconst;

  copy "file-$$", "lib";
  open(R, "lib/file-$$") or die; $foo = <R>; close(R);
  print "not " unless $foo eq sprintf "ok %d\n", 3+$loopconst;
  printf "ok %d\n", 10+$loopconst;
  unlink "lib/file-$$" or die "unlink: $!";

  move "file-$$", "lib";
  open(R, "lib/file-$$") or die "open lib/file-$$: $!"; $foo = <R>; close(R);
  print "not " unless $foo eq sprintf("ok %d\n", 3+$loopconst)
      and not -e "file-$$";;
  printf "ok %d\n", 11+$loopconst;
  unlink "lib/file-$$" or die "unlink: $!";

  # warn sprintf "INC->".$INC{"File/Copy.pm"};
  delete $INC{"File/Copy.pm"};

}


END {
    1 while unlink "file-$$";
    1 while unlink "lib/file-$$";
}
