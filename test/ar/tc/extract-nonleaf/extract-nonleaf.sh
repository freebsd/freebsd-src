# $Id$
inittest extract-nonleaf tc/extract-nonleaf
extshar ${TESTDIR}
extshar ${RLTDIR}
runcmd "${AR} xv invalid.a" work true
rundiff true
