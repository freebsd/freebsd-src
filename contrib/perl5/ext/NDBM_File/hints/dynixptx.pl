# On DYNIX/ptx 4.0 (v4.1.3), ndbm is actually contained in the 
# libc library, and must be explicitly linked against -lc when compiling.
$self->{LIBS} = ['-lc'];
