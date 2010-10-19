#target: cris-*-*elf*
#ld: --section-start=.text=0xc0010000
#objdump: -s -j .text

# Check that we don't get a "relocation truncated to fit", when a
# relocation would overflow if it hadn't been wrapping.  We always
# want 32-bit-wrapping on a 32-bit target for the benefit of Linux
# address-mapping macros.

.*:     file format elf32.*-cris

Contents of section \.text:
 c0010000 04200100 00200100                    .*
