# $Id$
inittest extract-nonexistent tc/extract-nonexistent
extshar ${TESTDIR}
extshar ${RLTDIR}
runcmd "${AR} xv valid.a nonexistent" work true
rundiff true
