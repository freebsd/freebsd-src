use ExtUtils::MakeMaker;
WriteMakefile(
    NAME		=> 'File::Glob',
    VERSION_FROM	=> 'Glob.pm',
    MAN3PODS		=> {},     # Pods will be built by installman.
    OBJECT		=> 'bsd_glob$(OBJ_EXT) Glob$(OBJ_EXT)',

## uncomment for glob debugging (will cause make test to fail)
#   DEFINE		=> '-DGLOB_DEBUG',
#   OPTIMIZE		=> '-g',
);
use Config;
sub MY::cflags {
  package MY;
  my $inherited = shift->SUPER::cflags(@_);
  if ($Config::Config{archname} =~ /^aix/ and 
      $Config::Config{use64bitall} eq 'define') {
    $inherited =~ s/\s-O\d?//m;
  }
  $inherited;
}
