# as: -meabi=4
# readelf: -A
# This test is only valid on ELF based ports.
#not-target: *-*-*coff *-*-pe *-*-wince *-*-*aout* *-*-netbsd *-*-riscix*
Attribute Section: aeabi
File Attributes
  Tag_CPU_name: "ARM1136JF-S"
  Tag_CPU_arch: v6
  Tag_ARM_ISA_use: Yes
  Tag_ABI_VFP_args: VFP registers
  Tag_compatibility: flag = 3, vendor = GNU
  Tag_unknown_128: 1234 \(0x4d2\)
  Tag_unknown_129: "bar"
