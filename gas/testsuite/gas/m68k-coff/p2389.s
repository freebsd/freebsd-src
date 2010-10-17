#  I reached a point where the file looks
# clean and complies with gas syntax, but it core dumps gas. Here's a
# little gdb info:
#
# Program terminated with signal 11, Segmentation fault.
# #0  0x6323c in memcpy ()
# (gdb) bt
# #0  0x6323c in memcpy ()
# #1  0xf2b0 in fill_section (abfd=0xeaee8, filehdr=0x8a7f4, 
#     file_cursor=0xf7fff654) at obj-format.c:534
# #2  0x112a8 in write_object_file () at obj-format.c:1786
# #3  0x13ef8 in main (argc=5, argv=0xf7fff7bc) at ../../p3/gas/as.c:310
# (gdb) 
#
# gas did manage to create the .o file at this point.

	.bss

_ASIC_INT_TBL:   .space 32,0	| keep interrupt routines here
