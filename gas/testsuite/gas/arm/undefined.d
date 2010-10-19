#name: Undefined local label error
# COFF and aout based ports use a different naming convention for local labels.
#skip: *-*-*coff *-*-pe *-*-wince *-*-*aout* *-*-netbsd *-*-riscix*
#error-output: undefined.l
