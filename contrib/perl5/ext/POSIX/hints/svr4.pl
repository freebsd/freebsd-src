# NCR MP-RAS.  Thanks to Doug Hendricks for this info.
# Configure sets osname=svr4.0, osvers=3.0, archname='3441-svr4.0'
# This system needs to explicitly link against -lmw to pull in some
# symbols such as _mwoflocheckl and possibly others.
#  A. Dougherty  Thu Dec  7 11:55:28 EST 2000
if ($Config{'archname'} =~ /3441-svr4/) {
    $self->{LIBS} = ['-lm -posix -lcposix -lmw'];
}
# Not sure what OS this one is.
elsif ($Config{archname} =~ /RM\d\d\d-svr4/) {
    $self->{LIBS} = ['-lm -lc -lposix -lcposix'];
}
