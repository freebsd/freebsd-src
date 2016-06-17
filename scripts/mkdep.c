/*
 * Originally by Linus Torvalds.
 * Smart CONFIG_* processing by Werner Almesberger, Michael Chastain.
 *
 * Usage: mkdep cflags -- file ...
 * 
 * Read source files and output makefile dependency lines for them.
 * I make simple dependency lines for #include <*.h> and #include "*.h".
 * I also find instances of CONFIG_FOO and generate dependencies
 *    like include/config/foo.h.
 *
 * 1 August 1999, Michael Elizabeth Chastain, <mec@shout.net>
 * - Keith Owens reported a bug in smart config processing.  There used
 *   to be an optimization for "#define CONFIG_FOO ... #ifdef CONFIG_FOO",
 *   so that the file would not depend on CONFIG_FOO because the file defines
 *   this symbol itself.  But this optimization is bogus!  Consider this code:
 *   "#if 0 \n #define CONFIG_FOO \n #endif ... #ifdef CONFIG_FOO".  Here
 *   the definition is inactivated, but I still used it.  It turns out this
 *   actually happens a few times in the kernel source.  The simple way to
 *   fix this problem is to remove this particular optimization.
 *
 * 2.3.99-pre1, Andrew Morton <andrewm@uow.edu.au>
 * - Changed so that 'filename.o' depends upon 'filename.[cS]'.  This is so that
 *   missing source files are noticed, rather than silently ignored.
 *
 * 2.4.2-pre3, Keith Owens <kaos@ocs.com.au>
 * - Accept cflags followed by '--' followed by filenames.  mkdep extracts -I
 *   options from cflags and looks in the specified directories as well as the
 *   defaults.   Only -I is supported, no attempt is made to handle -idirafter,
 *   -isystem, -I- etc.
 */

#include <ctype.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>



char __depname[512] = "\n\t@touch ";
#define depname (__depname+9)
int hasdep;

struct path_struct {
	int len;
	char *buffer;
};
struct path_struct *path_array;
int paths;


/* Current input file */
static const char *g_filename;

/*
 * This records all the configuration options seen.
 * In perl this would be a hash, but here it's a long string
 * of values separated by newlines.  This is simple and
 * extremely fast.
 */
char * str_config  = NULL;
int    size_config = 0;
int    len_config  = 0;

static void
do_depname(void)
{
	if (!hasdep) {
		hasdep = 1;
		printf("%s:", depname);
		if (g_filename)
			printf(" %s", g_filename);
	}
}

/*
 * Grow the configuration string to a desired length.
 * Usually the first growth is plenty.
 */
void grow_config(int len)
{
	while (len_config + len > size_config) {
		if (size_config == 0)
			size_config = 2048;
		str_config = realloc(str_config, size_config *= 2);
		if (str_config == NULL)
			{ perror("malloc config"); exit(1); }
	}
}



/*
 * Lookup a value in the configuration string.
 */
int is_defined_config(const char * name, int len)
{
	const char * pconfig;
	const char * plast = str_config + len_config - len;
	for ( pconfig = str_config + 1; pconfig < plast; pconfig++ ) {
		if (pconfig[ -1] == '\n'
		&&  pconfig[len] == '\n'
		&&  !memcmp(pconfig, name, len))
			return 1;
	}
	return 0;
}



/*
 * Add a new value to the configuration string.
 */
void define_config(const char * name, int len)
{
	grow_config(len + 1);

	memcpy(str_config+len_config, name, len);
	len_config += len;
	str_config[len_config++] = '\n';
}



/*
 * Clear the set of configuration strings.
 */
void clear_config(void)
{
	len_config = 0;
	define_config("", 0);
}



/*
 * This records all the precious .h filenames.  No need for a hash,
 * it's a long string of values enclosed in tab and newline.
 */
char * str_precious  = NULL;
int    size_precious = 0;
int    len_precious  = 0;



/*
 * Grow the precious string to a desired length.
 * Usually the first growth is plenty.
 */
void grow_precious(int len)
{
	while (len_precious + len > size_precious) {
		if (size_precious == 0)
			size_precious = 2048;
		str_precious = realloc(str_precious, size_precious *= 2);
		if (str_precious == NULL)
			{ perror("malloc"); exit(1); }
	}
}



/*
 * Add a new value to the precious string.
 */
void define_precious(const char * filename)
{
	int len = strlen(filename);
	grow_precious(len + 4);
	*(str_precious+len_precious++) = '\t';
	memcpy(str_precious+len_precious, filename, len);
	len_precious += len;
	memcpy(str_precious+len_precious, " \\\n", 3);
	len_precious += 3;
}



/*
 * Handle an #include line.
 */
void handle_include(int start, const char * name, int len)
{
	struct path_struct *path;
	int i;

	if (len == 14 && !memcmp(name, "linux/config.h", len))
		return;

	if (len >= 7 && !memcmp(name, "config/", 7))
		define_config(name+7, len-7-2);

	for (i = start, path = path_array+start; i < paths; ++i, ++path) {
		memcpy(path->buffer+path->len, name, len);
		path->buffer[path->len+len] = '\0';
		if (access(path->buffer, F_OK) == 0) {
			do_depname();
			printf(" \\\n   %s", path->buffer);
			return;
		}
	}

}



/*
 * Add a path to the list of include paths.
 */
void add_path(const char * name)
{
	struct path_struct *path;
	char resolved_path[PATH_MAX+1];
	const char *name2;

	if (strcmp(name, ".")) {
		name2 = realpath(name, resolved_path);
		if (!name2) {
			fprintf(stderr, "realpath(%s) failed, %m\n", name);
			exit(1);
		}
	}
	else {
		name2 = "";
	}

	path_array = realloc(path_array, (++paths)*sizeof(*path_array));
	if (!path_array) {
		fprintf(stderr, "cannot expand path_arry\n");
		exit(1);
	}

	path = path_array+paths-1;
	path->len = strlen(name2);
	path->buffer = malloc(path->len+1+256+1);
	if (!path->buffer) {
		fprintf(stderr, "cannot allocate path buffer\n");
		exit(1);
	}
	strcpy(path->buffer, name2);
	if (path->len && *(path->buffer+path->len-1) != '/') {
		*(path->buffer+path->len) = '/';
		*(path->buffer+(++(path->len))) = '\0';
	}
}



/*
 * Record the use of a CONFIG_* word.
 */
void use_config(const char * name, int len)
{
	char *pc;
	int i;

	pc = path_array[paths-1].buffer + path_array[paths-1].len;
	memcpy(pc, "config/", 7);
	pc += 7;

	for (i = 0; i < len; i++) {
	    char c = name[i];
	    if (isupper((int)c)) c = tolower((int)c);
	    if (c == '_')   c = '/';
	    pc[i] = c;
	}
	pc[len] = '\0';

	if (is_defined_config(pc, len))
	    return;

	define_config(pc, len);

	do_depname();
	printf(" \\\n   $(wildcard %s.h)", path_array[paths-1].buffer);
}



/*
 * Macros for stunningly fast map-based character access.
 * __buf is a register which holds the current word of the input.
 * Thus, there is one memory access per sizeof(unsigned long) characters.
 */

#if defined(__alpha__) || defined(__i386__) || defined(__ia64__)  || defined(__x86_64__) || defined(__MIPSEL__)	\
    || defined(__arm__)
#define LE_MACHINE
#endif

#ifdef LE_MACHINE
#define next_byte(x) (x >>= 8)
#define current ((unsigned char) __buf)
#else
#define next_byte(x) (x <<= 8)
#define current (__buf >> 8*(sizeof(unsigned long)-1))
#endif

#define GETNEXT { \
	next_byte(__buf); \
	if ((unsigned long) next % sizeof(unsigned long) == 0) { \
		if (next >= end) \
			break; \
		__buf = * (unsigned long *) next; \
	} \
	next++; \
}

/*
 * State machine macros.
 */
#define CASE(c,label) if (current == c) goto label
#define NOTCASE(c,label) if (current != c) goto label

/*
 * Yet another state machine speedup.
 */
#define MAX2(a,b) ((a)>(b)?(a):(b))
#define MIN2(a,b) ((a)<(b)?(a):(b))
#define MAX5(a,b,c,d,e) (MAX2(a,MAX2(b,MAX2(c,MAX2(d,e)))))
#define MIN5(a,b,c,d,e) (MIN2(a,MIN2(b,MIN2(c,MIN2(d,e)))))



/*
 * The state machine looks for (approximately) these Perl regular expressions:
 *
 *    m|\/\*.*?\*\/|
 *    m|\/\/.*|
 *    m|'.*?'|
 *    m|".*?"|
 *    m|#\s*include\s*"(.*?)"|
 *    m|#\s*include\s*<(.*?>"|
 *    m|#\s*(?define|undef)\s*CONFIG_(\w*)|
 *    m|(?!\w)CONFIG_|
 *
 * About 98% of the CPU time is spent here, and most of that is in
 * the 'start' paragraph.  Because the current characters are
 * in a register, the start loop usually eats 4 or 8 characters
 * per memory read.  The MAX5 and MIN5 tests dispose of most
 * input characters with 1 or 2 comparisons.
 */
void state_machine(const char * map, const char * end)
{
	const char * next = map;
	const char * map_dot;
	unsigned long __buf = 0;

	for (;;) {
start:
	GETNEXT
__start:
	if (current > MAX5('/','\'','"','#','C')) goto start;
	if (current < MIN5('/','\'','"','#','C')) goto start;
	CASE('/',  slash);
	CASE('\'', squote);
	CASE('"',  dquote);
	CASE('#',  pound);
	CASE('C',  cee);
	goto start;

/* // */
slash_slash:
	GETNEXT
	CASE('\n', start);
	NOTCASE('\\', slash_slash);
	GETNEXT
	goto slash_slash;

/* / */
slash:
	GETNEXT
	CASE('/',  slash_slash);
	NOTCASE('*', __start);
slash_star_dot_star:
	GETNEXT
__slash_star_dot_star:
	NOTCASE('*', slash_star_dot_star);
	GETNEXT
	NOTCASE('/', __slash_star_dot_star);
	goto start;

/* '.*?' */
squote:
	GETNEXT
	CASE('\'', start);
	NOTCASE('\\', squote);
	GETNEXT
	goto squote;

/* ".*?" */
dquote:
	GETNEXT
	CASE('"', start);
	NOTCASE('\\', dquote);
	GETNEXT
	goto dquote;

/* #\s* */
pound:
	GETNEXT
	CASE(' ',  pound);
	CASE('\t', pound);
	CASE('i',  pound_i);
	CASE('d',  pound_d);
	CASE('u',  pound_u);
	goto __start;

/* #\s*i */
pound_i:
	GETNEXT NOTCASE('n', __start);
	GETNEXT NOTCASE('c', __start);
	GETNEXT NOTCASE('l', __start);
	GETNEXT NOTCASE('u', __start);
	GETNEXT NOTCASE('d', __start);
	GETNEXT NOTCASE('e', __start);
	goto pound_include;

/* #\s*include\s* */
pound_include:
	GETNEXT
	CASE(' ',  pound_include);
	CASE('\t', pound_include);
	map_dot = next;
	CASE('"',  pound_include_dquote);
	CASE('<',  pound_include_langle);
	goto __start;

/* #\s*include\s*"(.*)" */
pound_include_dquote:
	GETNEXT
	CASE('\n', start);
	NOTCASE('"', pound_include_dquote);
	handle_include(0, map_dot, next - map_dot - 1);
	goto start;

/* #\s*include\s*<(.*)> */
pound_include_langle:
	GETNEXT
	CASE('\n', start);
	NOTCASE('>', pound_include_langle);
	handle_include(1, map_dot, next - map_dot - 1);
	goto start;

/* #\s*d */
pound_d:
	GETNEXT NOTCASE('e', __start);
	GETNEXT NOTCASE('f', __start);
	GETNEXT NOTCASE('i', __start);
	GETNEXT NOTCASE('n', __start);
	GETNEXT NOTCASE('e', __start);
	goto pound_define_undef;

/* #\s*u */
pound_u:
	GETNEXT NOTCASE('n', __start);
	GETNEXT NOTCASE('d', __start);
	GETNEXT NOTCASE('e', __start);
	GETNEXT NOTCASE('f', __start);
	goto pound_define_undef;

/*
 * #\s*(define|undef)\s*CONFIG_(\w*)
 *
 * this does not define the word, because it could be inside another
 * conditional (#if 0).  But I do parse the word so that this instance
 * does not count as a use.  -- mec
 */
pound_define_undef:
	GETNEXT
	CASE(' ',  pound_define_undef);
	CASE('\t', pound_define_undef);

	        NOTCASE('C', __start);
	GETNEXT NOTCASE('O', __start);
	GETNEXT NOTCASE('N', __start);
	GETNEXT NOTCASE('F', __start);
	GETNEXT NOTCASE('I', __start);
	GETNEXT NOTCASE('G', __start);
	GETNEXT NOTCASE('_', __start);

	map_dot = next;
pound_define_undef_CONFIG_word:
	GETNEXT
	if (isalnum(current) || current == '_')
		goto pound_define_undef_CONFIG_word;
	goto __start;

/* \<CONFIG_(\w*) */
cee:
	if (next >= map+2 && (isalnum((int)next[-2]) || next[-2] == '_'))
		goto start;
	GETNEXT NOTCASE('O', __start);
	GETNEXT NOTCASE('N', __start);
	GETNEXT NOTCASE('F', __start);
	GETNEXT NOTCASE('I', __start);
	GETNEXT NOTCASE('G', __start);
	GETNEXT NOTCASE('_', __start);

	map_dot = next;
cee_CONFIG_word:
	GETNEXT
	if (isalnum(current) || current == '_')
		goto cee_CONFIG_word;
	use_config(map_dot, next - map_dot - 1);
	goto __start;
    }
}



/*
 * Generate dependencies for one file.
 */
void do_depend(const char * filename, const char * command)
{
	int mapsize;
	int pagesizem1 = getpagesize()-1;
	int fd;
	struct stat st;
	char * map;

	fd = open(filename, O_RDONLY);
	if (fd < 0) {
		perror(filename);
		return;
	}

	fstat(fd, &st);
	if (st.st_size == 0) {
		fprintf(stderr,"%s is empty\n",filename);
		close(fd);
		return;
	}

	mapsize = st.st_size;
	mapsize = (mapsize+pagesizem1) & ~pagesizem1;
	map = mmap(NULL, mapsize, PROT_READ, MAP_PRIVATE, fd, 0);
	if ((long) map == -1) {
		perror("mkdep: mmap");
		close(fd);
		return;
	}
	if ((unsigned long) map % sizeof(unsigned long) != 0)
	{
		fprintf(stderr, "do_depend: map not aligned\n");
		exit(1);
	}

	hasdep = 0;
	clear_config();
	state_machine(map, map+st.st_size);
	if (hasdep) {
		puts(command);
		if (*command)
			define_precious(filename);
	}

	munmap(map, mapsize);
	close(fd);
}



/*
 * Generate dependencies for all files.
 */
int main(int argc, char **argv)
{
	int len;
	const char *hpath;

	hpath = getenv("HPATH");
	if (!hpath) {
		fputs("mkdep: HPATH not set in environment.  "
		      "Don't bypass the top level Makefile.\n", stderr);
		return 1;
	}

	add_path(".");		/* for #include "..." */

	while (++argv, --argc > 0) {
		if (strncmp(*argv, "-I", 2) == 0) {
			if (*((*argv)+2)) {
				add_path((*argv)+2);
			}
			else {
				++argv;
				--argc;
				add_path(*argv);
			}
		}
		else if (strcmp(*argv, "--") == 0) {
			break;
		}
	}

	add_path(hpath);	/* must be last entry, for config files */

	while (--argc > 0) {
		const char * filename = *++argv;
		const char * command  = __depname;
		g_filename = 0;
		len = strlen(filename);
		memcpy(depname, filename, len+1);
		if (len > 2 && filename[len-2] == '.') {
			if (filename[len-1] == 'c' || filename[len-1] == 'S') {
			    depname[len-1] = 'o';
			    g_filename = filename;
			    command = "";
			}
		}
		do_depend(filename, command);
	}
	if (len_precious) {
		*(str_precious+len_precious) = '\0';
		printf(".PRECIOUS:%s\n", str_precious);
	}
	return 0;
}
