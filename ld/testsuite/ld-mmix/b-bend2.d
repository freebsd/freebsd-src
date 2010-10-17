#source: b-twoinsn.s
#source: b-post1.s
#source: b-goodmain.s
#source: b-bend.s
#ld: --oformat binary
#objcopy_linked_file:
#error: invalid mmo file: lop_end not last item in file

# We use the b-bend.s file just to make the correct lop_end in
# b-goodmain.s not the last one.
