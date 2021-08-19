#! /bin/sh
# idea and sed lines taken straight from flex

cat <<!EOF
/* File created via mkskel.sh */

extern const char *crunched_skel[];
const char *crunched_skel[] = {
!EOF

sed 's/\\/&&/g' $* | sed 's/"/\\"/g' | sed 's/.*/  "&",/'

cat <<!EOF
  0
};
!EOF
