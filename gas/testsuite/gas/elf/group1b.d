#readelf: -g
#name: group section with multiple sections of same name
#source: group1.s

#...
COMDAT group section \[    1\] `.foo_group' \[.foo_group\] contains 1 sections:
[ 	]+\[Index\][ 	]+Name
[ 	]+\[.*\][ 	]+.text
#pass
