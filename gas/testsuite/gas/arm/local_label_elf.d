#nm: -n
#name: ARM local label relocs to section symbol relocs (ELF)
# This test is only valid on ELF targets.
# There are COFF and Windows CE versions of this test.
#skip: *-*-*coff *-*-pe *-wince-* *-*-wince *-*-*aout* *-*-netbsd *-*-riscix*

# Check if relocations against local symbols are converted to 
# relocations against section symbols.

