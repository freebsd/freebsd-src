#define LESSTEST_VERSION 1

typedef unsigned long wchar;
typedef unsigned char byte;
typedef unsigned char Attr;
typedef unsigned char Color;

#define NULL_COLOR          ((Color)0xff)

#define ATTR_BOLD           (1<<0)
#define ATTR_UNDERLINE      (1<<1)
#define ATTR_STANDOUT       (1<<2)
#define ATTR_BLINK          (1<<3)

#define ESC                 '\33'
#define LESS_DUMP_CHAR      '\35'
#define UNICODE_MAX_BYTES   4
#define MAX_SCREENBUF_SIZE  (16*1024)

#define RUN_OK              0
#define RUN_ERR             1

#define LTS_CHAR_ATTR       '@'
#define LTS_CHAR_FG_COLOR   '$'
#define LTS_CHAR_BG_COLOR   '!'
#define LTS_CHAR_CURSOR     '#'

#define is_ascii(ch)        ((ch) >= ' ' && (ch) < 0x7f)
#define pr_ascii(ch)        (is_ascii(ch) ? ((char)ch) : '.')

#undef countof
#define countof(a) (sizeof(a)/sizeof(*a))
