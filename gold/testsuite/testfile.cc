// testfile.cc -- Dummy ELF objects for testing purposes.

#include "gold.h"

#include "target.h"
#include "target-select.h"

#include "test.h"
#include "testfile.h"

namespace gold_testsuite
{

using namespace gold;

// A Target used for testing purposes.

class Target_test : public Sized_target<32, false>
{
 public:
  Target_test()
    : Sized_target<32, false>(&test_target_info)
  { }

  void
  scan_relocs(const General_options&, Symbol_table*, Layout*,
	      Sized_relobj<32, false>*, unsigned int, unsigned int,
	      const unsigned char*, size_t, size_t, const unsigned char*,
	      Symbol**)
  { ERROR("call to Target_test::scan_relocs"); }

  void
  relocate_section(const Relocate_info<32, false>*, unsigned int,
		   const unsigned char*, size_t, unsigned char*,
		   elfcpp::Elf_types<32>::Elf_Addr, off_t)
  { ERROR("call to Target_test::relocate_section"); }

  static const Target::Target_info test_target_info;
};

const Target::Target_info Target_test::test_target_info =
{
  32,					// size
  false,				// is_big_endian
  static_cast<elfcpp::EM>(0xffff),	// machine_code
  false,				// has_make_symbol
  false,				// has_resolve
  "/dummy",				// dynamic_linker
  0x08000000,				// text_segment_address
  0x1000,				// abi_pagesize
  0x1000				// common_pagesize
};

// The single test target.

Target_test target_test;

// A pointer to the test target.  This is used in CHECKs.

Target* target_test_pointer = &target_test;

// Select the test target.

class Target_selector_test : public Target_selector
{
 public:
  Target_selector_test()
    : Target_selector(0xffff, 32, false)
  { }

  Target*
  recognize(int, int, int)
  { return &target_test; }
};

// Register the test target selector.

Target_selector_test target_selector_test;

// A simple ELF object with one empty section, named ".test" and one
// globally visible symbol named "test".

const unsigned char test_file_1[] =
{
  // Ehdr
  // EI_MAG[0-3]
  0x7f, 'E', 'L', 'F',
  // EI_CLASS: 32 bit.
  1,
  // EI_DATA: little endian
  1,
  // EI_VERSION
  1,
  // EI_OSABI
  0,
  // EI_ABIVERSION
  0,
  // EI_PAD
  0, 0, 0, 0, 0, 0, 0,
  // e_type: ET_REL
  1, 0,
  // e_machine: a magic value used for testing.
  0xff, 0xff,
  // e_version
  1, 0, 0, 0,
  // e_entry
  0, 0, 0, 0,
  // e_phoff
  0, 0, 0, 0,
  // e_shoff: starts right after file header
  52, 0, 0, 0,
  // e_flags
  0, 0, 0, 0,
  // e_ehsize
  52, 0,
  // e_phentsize
  32, 0,
  // e_phnum
  0, 0,
  // e_shentsize
  40, 0,
  // e_shnum: dummy, .test, .symtab, .strtab, .shstrtab
  5, 0,
  // e_shstrndx
  4, 0,

  // Offset 52
  // Shdr 0: dummy entry
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0,

  // Offset 92
  // Shdr 1: .test
  // sh_name: after initial null
  1, 0, 0, 0,
  // sh_type: SHT_PROGBITS
  1, 0, 0, 0,
  // sh_flags: SHF_ALLOC
  2, 0, 0, 0,
  // sh_addr
  0, 0, 0, 0,
  // sh_offset: after file header + 5 section headers
  252, 0, 0, 0,
  // sh_size
  0, 0, 0, 0,
  // sh_link
  0, 0, 0, 0,
  // sh_info
  0, 0, 0, 0,
  // sh_addralign
  1, 0, 0, 0,
  // sh_entsize
  0, 0, 0, 0,

  // Offset 132
  // Shdr 2: .symtab
  // sh_name: 1 null byte + ".test\0"
  7, 0, 0, 0,
  // sh_type: SHT_SYMTAB
  2, 0, 0, 0,
  // sh_flags
  0, 0, 0, 0,
  // sh_addr
  0, 0, 0, 0,
  // sh_offset: after file header + 5 section headers + empty section
  252, 0, 0, 0,
  // sh_size: two symbols: dummy symbol + test symbol
  32, 0, 0, 0,
  // sh_link: to .strtab
  3, 0, 0, 0,
  // sh_info: one local symbol, the dummy symbol
  1, 0, 0, 0,
  // sh_addralign
  4, 0, 0, 0,
  // sh_entsize: size of symbol
  16, 0, 0, 0,

  // Offset 172
  // Shdr 3: .strtab
  // sh_name: 1 null byte + ".test\0" + ".symtab\0"
  15, 0, 0, 0,
  // sh_type: SHT_STRTAB
  3, 0, 0, 0,
  // sh_flags
  0, 0, 0, 0,
  // sh_addr
  0, 0, 0, 0,
  // sh_offset: after .symtab section.  284 == 0x11c
  0x1c, 0x1, 0, 0,
  // sh_size: 1 null byte + "test\0"
  6, 0, 0, 0,
  // sh_link
  0, 0, 0, 0,
  // sh_info
  0, 0, 0, 0,
  // sh_addralign
  1, 0, 0, 0,
  // sh_entsize
  0, 0, 0, 0,

  // Offset 212
  // Shdr 4: .shstrtab
  // sh_name: 1 null byte + ".test\0" + ".symtab\0" + ".strtab\0"
  23, 0, 0, 0,
  // sh_type: SHT_STRTAB
  3, 0, 0, 0,
  // sh_flags
  0, 0, 0, 0,
  // sh_addr
  0, 0, 0, 0,
  // sh_offset: after .strtab section.  290 == 0x122
  0x22, 0x1, 0, 0,
  // sh_size: all section names
  33, 0, 0, 0,
  // sh_link
  0, 0, 0, 0,
  // sh_info
  0, 0, 0, 0,
  // sh_addralign
  1, 0, 0, 0,
  // sh_entsize
  0, 0, 0, 0,

  // Offset 252
  // Contents of .symtab section
  // Symbol 0
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,

  // Offset 268
  // Symbol 1
  // st_name
  1, 0, 0, 0,
  // st_value
  0, 0, 0, 0,
  // st_size
  0, 0, 0, 0,
  // st_info: STT_NOTYPE, STB_GLOBAL
  0x10,
  // st_other
  0,
  // st_shndx: In .test
  1, 0,

  // Offset 284
  // Contents of .strtab section
  '\0',
  't', 'e', 's', 't', '\0',

  // Offset 290
  // Contents of .shstrtab section
  '\0',
  '.', 't', 'e', 's', 't', '\0',
  '.', 's', 'y', 'm', 't', 'a', 'b', '\0',
  '.', 's', 't', 'r', 't', 'a', 'b', '\0',
  '.', 's', 'h', 's', 't', 'r', 't', 'a', 'b', '\0'
};

const unsigned int test_file_1_size = sizeof test_file_1;

} // End namespace gold_testsuite.
