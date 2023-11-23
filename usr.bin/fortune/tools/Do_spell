#!/bin/sh -

F=_spell.$$
echo $1
spell < $1 > $F
sort $F $1.sp.ok | uniq -u | column
rm -f $F
