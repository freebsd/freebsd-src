#source: b-twoinsn.s
#source: b-bend.s
#source: b-post1.s
#source: b-goodmain.s
#ld: --oformat binary
#objcopy_linked_file:
#error: invalid mmo file: lop_end not last item in file

# This test depend on that the non-at-end condition is tested before
# not-correct-YZ-field and might need tweaking if the implementation
# changes.
