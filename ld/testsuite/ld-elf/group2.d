#source: ../../../binutils/testsuite/binutils-all/group.s
#ld: -r
#readelf: -Sg --wide

#...
  \[[ 0-9]+\] foo_group[ \t]+GROUP[ \t]+.*
#...
  \[[ 0-9]+\] \.text.*[ \t]+PROGBITS[ \t0-9a-f]+AXG.*
#...
  \[[ 0-9]+\] \.data.*[ \t]+PROGBITS[ \t0-9a-f]+WAG.*
#...
COMDAT group section \[[ 0-9]+\] `foo_group' \[foo_group\] contains 2 sections:
   \[Index\]    Name
   \[[ 0-9]+\]   .text.*
   \[[ 0-9]+\]   .data.*
#pass
