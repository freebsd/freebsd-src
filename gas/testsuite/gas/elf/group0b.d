#readelf: -g
#name: group section
#source: group0.s

#...
COMDAT group section \[    1\] `.foo_group' \[.foo_group\] contains 2 sections:
[ 	]+\[Index\][ 	]+Name
[ 	]+\[.*\][ 	]+.foo
[ 	]+\[.*\][ 	]+.bar
#pass
