#ifdef __ASSEMBLY__
#define ASM_CONST_LL(x)	(x)
#else
#define ASM_CONST_LL(x)	(x##ULL)
#endif

#define TEST_ADDR_1	ASM_CONST_LL(0xdeadbeef00000000)
#define TEST_SIZE_1	ASM_CONST_LL(0x100000)
#define TEST_ADDR_2	ASM_CONST_LL(123456789)
#define TEST_SIZE_2	ASM_CONST_LL(010000)

#define TEST_VALUE_1	0xdeadbeef
#define TEST_VALUE_2	123456789

#define TEST_VALUE64_1	ASM_CONST_LL(0xdeadbeef01abcdef)

#define PHANDLE_1	0x2000
#define PHANDLE_2	0x2001

#define TEST_STRING_1	"hello world"
#define TEST_STRING_2	"nastystring: \a\b\t\n\v\f\r\\\""
#define TEST_STRING_3	"\xde\xad\xbe\xef"

#define TEST_CHAR1	'\r'
#define TEST_CHAR2	'b'
#define TEST_CHAR3	'\0'
#define TEST_CHAR4	'\''
#define TEST_CHAR5	'\xff'

#ifndef __ASSEMBLY__
extern struct fdt_header _test_tree1;
extern struct fdt_header _truncated_property;
extern struct fdt_header _bad_node_char;
extern struct fdt_header _bad_node_format;
extern struct fdt_header _bad_prop_char;
#endif /* ! __ASSEMBLY */
