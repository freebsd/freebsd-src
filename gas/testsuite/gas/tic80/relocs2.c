extern char x_char;
extern short x_short;
static int x_int;
extern long x_long;
extern float x_float;
extern double x_double;
extern char *x_char_p;

static char s_char;
static short s_short;
static int s_int;
static long s_long;
static float s_float;
static double s_double;
static char *s_char_p;

char g_char;
short g_short;
int g_int;
long g_long;
float g_float;
double g_double;
char *g_char_p;

main ()
{
  x_char = s_char;
  g_char = x_char;
  x_short = s_short;
  g_short = x_short;
  x_int = s_int;
  g_int = x_int;
  x_long = s_long;
  g_long = x_long;
  x_float = s_float;
  g_float = x_float;
  x_double = s_double;
  g_double = x_double;
  x_char_p = s_char_p;
  g_char_p = x_char_p;
}
