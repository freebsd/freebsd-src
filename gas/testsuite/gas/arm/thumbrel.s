@ Check that PC-relative relocs against local function symbols are
@ generated correctly.
.text
.thumb
a:
.word 0
.word b - a
.word 0
.word 0
.type b, %function
b:
