# Several OSs come with an old version of the DB library which fails
# on a few of the db-recno.t tests.  This file is sourced by the hints
# files for those OSs.

cat <<EOF >&4

Unless you've upgraded your DB library manually you will see failures in
db-recno tests 51, 53 and 55.  The behavior these tests are checking is
broken in the DB library which is included with the OS.  You can ignore
the errors if you're never going to use the broken functionality (recno
databases with a modified bval), otherwise you'll have to upgrade your
DB library or OS.

EOF
