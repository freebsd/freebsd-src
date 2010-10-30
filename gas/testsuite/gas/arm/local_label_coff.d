#nm: -n
#name: ARM local label relocs to section symbol relocs (COFF)
# This test is only valid on COFF based targets, except Windows CE.
# There are ELF and Windows CE versions of this test.
#not-skip: *-unknown-pe *-epoc-pe *-*-*coff

# Check if relocations against local symbols are converted to 
# relocations against section symbols.
0+0 b .bss
0+0 d .data
0+0 t .text
