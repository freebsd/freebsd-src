# $Id$
inittest add-nonexistent tc/add-nonexistent
runcmd "${AR} rc archive.a nonexistent" work true
rundiff true
