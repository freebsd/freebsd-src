#source: dso-1.s
#source: gotrel1.s --pic
#as: --no-underscore --em=criself
#ld: -m crislinux tmpdir/libdso-4.so
#objdump: -T

# The DSO used has an undef reference to the symbol "dsofn", which is
# supposed to cause the program to automatically export it as a dynamic
# symbol; no --export-dynamic is supposed to be needed.

#...
[0-9a-f]+ g    DF .text	00000000 dsofn
#pass
