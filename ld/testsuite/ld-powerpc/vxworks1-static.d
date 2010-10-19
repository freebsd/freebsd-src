#name: VxWorks executable test 1 (static)
#source: vxworks1.s -mregnames
#ld: tmpdir/libvxworks1.so -Tvxworks1.ld
#error: Dynamic sections created in non-dynamic link
