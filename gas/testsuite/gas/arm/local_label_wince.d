#nm: -n
#name: ARM local label relocs to section symbol relocs (WinCE)
# This test is only valid on Windows CE.
# There are ELF and COFF versions of this test.
#not-skip: *-*-wince *-wince-*

# Check if relocations against local symbols are converted to 
# relocations against section symbols.
0+0 b .bss
0+0 d .data
0+0 t .text
