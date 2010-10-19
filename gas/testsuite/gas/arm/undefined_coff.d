#name: Undefined local label error
# COFF and aout based ports use a different naming convention for local labels.
#not-skip: *-*-*coff *-*-pe *-*-wince *-*-*aout* *-*-netbsd *-*-riscix*
#error-output: undefined_coff.l
