#! /bin/sh

cat <<!
/* File created from flex.skel via mkskel.sh */

#include "flexdef.h"

char *skel[] = {
!

sed 's/\\/&&/g' $* | sed 's/"/\\"/g' | sed 's/.*/  "&",/'

cat <<!
  0
};
!
