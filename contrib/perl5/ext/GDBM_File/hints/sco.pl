# SCO OSR5 needs to link with libc.so again to have C<fsync> defined
$self->{LIBS} = ['-lgdbm -lc'];
