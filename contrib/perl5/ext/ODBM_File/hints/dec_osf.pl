# The -hidden option causes compilation to fail on Digital Unix.
#   Andy Dougherty  <doughera@lafcol.lafayette.edu>
#   Sat Jan 13 16:29:52 EST 1996
$self->{LDDLFLAGS} = $Config{lddlflags};
$self->{LDDLFLAGS} =~ s/-hidden//;
#  As long as we're hinting, note the known location of the dbm routines.
#   Spider Boardman  <spider@Orb.Nashua.NH.US>
#   Fri Feb 21 14:50:31 EST 1997
$self->{LIBS} = ['-ldbm'];
