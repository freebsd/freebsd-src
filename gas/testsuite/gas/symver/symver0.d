#nm: -n
#name: symver symver0
#
# The #... and #pass are there to match extra symbols inserted by
# some toolchains, eg arm-elf toolchain will add $d.

[ 	]+U foo@version1
#...
0+0000000 D foo1
0+00000.. d L_foo1
