#ifdef __ASSEMBLY__
#define ASM_CONST_LL(x)	(x)
#else
#define ASM_CONST_LL(x)	(x##ULL)
#endif

#define TEST_ADDR_1H	ASM_CONST_LL(0xdeadbeef)
#define TEST_ADDR_1L	ASM_CONST_LL(0x00000000)
#define TEST_ADDR_1	((TEST_ADDR_1H << 32) | TEST_ADDR_1L)
#define TEST_SIZE_1H	ASM_CONST_LL(0x00000000)
#define TEST_SIZE_1L	ASM_CONST_LL(0x00100000)
#define TEST_SIZE_1	((TEST_SIZE_1H << 32) | TEST_SIZE_1L)
#define TEST_ADDR_2H	ASM_CONST_LL(0)
#define TEST_ADDR_2L	ASM_CONST_LL(123456789)
#define TEST_ADDR_2	((TEST_ADDR_2H << 32) | TEST_ADDR_2L)
#define TEST_SIZE_2H	ASM_CONST_LL(0)
#define TEST_SIZE_2L	ASM_CONST_LL(010000)
#define TEST_SIZE_2	((TEST_SIZE_2H << 32) | TEST_SIZE_2L)

#define TEST_VALUE_1	0xdeadbeef
#define TEST_VALUE_2	123456789

#define TEST_VALUE64_1H	ASM_CONST_LL(0xdeadbeef)
#define TEST_VALUE64_1L	ASM_CONST_LL(0x01abcdef)
#define TEST_VALUE64_1	((TEST_VALUE64_1H << 32) | TEST_VALUE64_1L)

#define PHANDLE_1	0x2000
#define PHANDLE_2	0x2001

#define TEST_STRING_1	"hello world"
#define TEST_STRING_2	"nastystring: \a\b\t\n\v\f\r\\\""
#define TEST_STRING_3	"\xde\xad\xbe\xef"

#define TEST_STRING_4_PARTIAL	"foobar"
#define TEST_STRING_4_RESULT	"testfoobar"

#define TEST_CHAR1	'\r'
#define TEST_CHAR2	'b'
#define TEST_CHAR3	'\0'
#define TEST_CHAR4	'\''
#define TEST_CHAR5	'\xff'

#ifndef __ASSEMBLY__
extern struct fdt_header test_tree1;
extern struct fdt_header truncated_property;
extern struct fdt_header bad_node_char;
extern struct fdt_header bad_node_format;
extern struct fdt_header bad_prop_char;
extern struct fdt_header ovf_size_strings;
#endif /* ! __ASSEMBLY */
