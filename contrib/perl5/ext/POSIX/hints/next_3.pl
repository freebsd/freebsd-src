# NeXT *does* have setpgid when we use the -posix flag, but
# doesn't when we don't.  The main perl sources are compiled
# without -posix, so the hints/next_3.sh hint file tells Configure
# that  d_setpgid=undef.
$self->{CCFLAGS} = $Config{ccflags} . ' -posix -DHAS_SETPGID' ;
