# SCO ODT 3.2v4.2 has a -ldbm library that is missing dbmclose.  
# This system should have a complete library installed as -ldbm.nfs which
# should be used instead (Probably need the networking product add-on)
$self->{LIBS} = ['-lndbm',-e "/usr/lib/libdbm.nfs.a"?'-ldbm.nfs':'-ldbm'];
