#!/bin/sh

me=$(git config user.name || echo WHO-AM-I)
now=$(date +%Y)

! git grep -l "Copyright .* $me" |
grep -v 'tests/.*[.]exp[a-z]*' |
( while read f
  do git log --format="%ci $f" -1 -- $f
  done;
  git log --format="%ci COPYING" -1
) |
grep ^$now |
while read d t z f
do grep -H -m 1 "Copyright .* $me" $f
done |
grep -v $now

grep ^[.]Dd unifdef.1 | grep -v "$(date +'%B [0-9]*, %Y')"

exit 0
