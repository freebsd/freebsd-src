# Some versions of SCO contain a broken -ldbm library that is missing
# dbmclose.  Some of those might have a fixed library installed as
# -ldbm.nfs.
$self->{LIBS} = ['-ldbm.nfs', '-ldbm'];
