# Keep the copyright. Also ensures we never have a completely empty commit.
/\tCOPYING$/p

# A few architectures have dts files at non standard paths. Massage those into
# a standard arch/ARCH/boot/dts first.

# symlink: arch/microblaze/boot/dts/system.dts -> ../../platform/generic/system.dts
\,\tarch/microblaze/boot/dts/system.dts$,d
s,\tarch/microblaze/platform/generic/\(system.dts\)$,\tarch/microblaze/boot/dts/\1,g

# arch/mips/lantiq/dts/easy50712.dts
# arch/mips/lantiq/dts/danube.dtsi
# arch/mips/netlogic/dts/xlp_evp.dts
# arch/mips/ralink/dts/rt3050.dtsi
# arch/mips/ralink/dts/rt3052_eval.dts
s,\tarch/mips/\([^/]*\)/dts/\(.*\.dts.\?\)$,\tarch/mips/boot/dts/\2,g

# arch/mips/cavium-octeon/octeon_68xx.dts
# arch/mips/cavium-octeon/octeon_3xxx.dts
# arch/mips/mti-sead3/sead3.dts
s,\tarch/mips/\([^/]*\)/\([^/]*\.dts.\?\)$,\tarch/mips/boot/dts/\2,g

# arch/x86/platform/ce4100/falconfalls.dts
s,\tarch/x86/platform/ce4100/falconfalls.dts,\tarch/x86/boot/dts/falconfalls.dts,g

# test cases
s,\tdrivers/of/testcase-data/,\ttestcase-data/,gp

# Now rewrite generic DTS paths
s,\tarch/\([^/]*\)/boot/dts/\(.*\.dts.\?\)$,\tsrc/\1/\2,gp
s,\tarch/\([^/]*\)/boot/dts/\(.*\.h\)$,\tsrc/\1/\2,gp

# Also rewrite the DTS include paths for dtc+cpp support
s,\tarch/\([^/]*\)/include/dts/,\tsrc/\1/include/,gp
s,\tinclude/dt-bindings/,\tinclude/dt-bindings/,gp

# Rewrite the bindings subdirectory
s,\tDocumentation/devicetree/bindings/,\tBindings/,gp
