# -lucb has been reported to be fatal for perl5 on Solaris.
# Thus we deliberately don't include it here.
$self->{LIBS} = ['-ldbm'];
