#  Try to work around "bad free" messages.  See note in ODBM_File.xs.
#   Andy Dougherty  <doughera@lafcol.lafayette.edu>
#   Sun Sep  8 12:57:52 EDT 1996
$self->{CCFLAGS} = $Config{ccflags} . ' -DDBM_BUG_DUPLICATE_FREE' ;
