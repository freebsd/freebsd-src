#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

#ifdef FCPW
#include <windows.h>
#define FCP "fcpw"
#else
#define FCP "fcp"
#endif

#ifndef O_BINARY
#define O_BINARY 0
#endif

static void fatal(const char *format, ...)
{
	va_list ap;
#ifdef FCPW
	char buffer[640];

	va_start(ap, format);
	vsnprintf(buffer, sizeof buffer, format, ap);
	MessageBox(NULL, buffer, FCP, MB_ICONSTOP | MB_OK);
#else
	fprintf(stderr, "%s: ", FCP);
	va_start(ap, format);
	vfprintf(stderr, format, ap);
	fputc('\n', stderr);
#endif
	va_end(ap);
	exit(1);
}

static void error(const char *s)
{
#ifdef FCPW
	char buffer[640];

	snprintf(buffer, sizeof buffer, "%s: %s", s, strerror(errno));
	MessageBox(NULL, buffer, FCP, MB_ICONSTOP | MB_OK);
#else
	fprintf(stderr, "%s: ", FCP);
	perror(s);
#endif
	exit(1);
}

#define SIZE ((1 << 24) + 1)

#ifdef FCPW
#define main wrap
#endif

static const char *USAGE =
	"usage: " FCP " [OFFSET] FILE PATCH\n"
	"\n"
	FCP " 0.10.2, Copyright (C) 2017 Dimitar Toshkov Zhekov\n"
	"\n"
	"This program is free software; you can redistribute it and/or modify it\n"
	"under the terms of the GNU General Public License as published by the Free\n"
	"Software Foundation; either version 2 of the License, or (at your option)\n"
	"any later version.\n"
	"\n"
	"This program is distributed in the hope that it will be useful, but\n"
	"WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY\n"
	"or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License\n"
	"for more details.\n"
	"\n"
	"You should have received a copy of the GNU General Public License along\n"
	"with this program; if not, write to the Free Software Foundation, Inc.,\n"
	"51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.\n";

int main(int argc, char **argv)
{
	unsigned char *buffer;
	const char *name, *patch;
	int f, n;
	FILE *p;
	char s[0x80];
	long ignore;
	long minoff = -1, maxoff;

	if (argc == 4)
	{
		argv++;
		if (sscanf(*argv, "%lx%n", &ignore, &n) != 1 || n != (int) strlen(*argv)) fatal("%s: invalid offset", *argv);
		if (ignore < 0 || ignore >= SIZE) fatal("%s: offset out of range", ignore);
	}
	else if (argc != 3)
	{
	#ifdef FCPW
		MessageBox(NULL, USAGE, FCP, MB_OK);
	#else
		fputs(USAGE, stderr);
	#endif
		return 1;
	}
	if ((buffer = malloc(SIZE)) == NULL) error(FCP);
	name = argv[1];
	patch = argv[2];

	if ((f = open(name, O_RDWR | O_BINARY)) == -1) error(name);
	if ((n = read(f, buffer, SIZE)) == -1) error(name);
	if (n == SIZE) fatal("%s: larger than %d bytes", name, SIZE - 1);
	if ((p = fopen(patch, "r")) == NULL) error(patch);

	while (fgets(s, sizeof s, p) != NULL)
	{
		char *lf;
		long offset;
		unsigned orig, chng;

		if ((lf = strchr(s, '\n')) != NULL) *lf = '\0';
		if (sscanf(s, "%lx: %x %x%n", &offset, &orig, &chng, &n) == 3 && n == (int) strlen(s))
		{
			if (offset < 0 || offset >= SIZE) fatal("%s: offset out of range", s);
			if (orig > 0xFF || chng > 0xFF) fatal("%s: byte(s) out of range", s);
			if (offset >= ignore)
			{
				if (buffer[offset] != orig) fatal("offset %lx not equal to %x", offset, orig);
				if (orig != chng)
				{
					if (minoff == -1) minoff = offset;
					buffer[maxoff = offset] = chng;
				}
			}
		}
	#ifdef FCPW
	#else
		else
			fprintf(stderr, "%s: `%s'\n", patch, s);
	#endif
	}

	if (minoff >= 0)
	{
		if (lseek(f, minoff, SEEK_SET) == -1) error(name);
		if (write(f, buffer + minoff, maxoff - minoff + 1) == -1) error(name);
	}

	if (close(f) == -1) error(name);
	if (fclose(p) == -1) error(patch);

	return 0;
}

#ifdef FCPW
int PASCAL WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	int i;

	(void) hInstance;
	(void) hPrevInstance;
	(void) lpCmdLine;
	(void) nCmdShow;

	for (i = 1; i < __argc; i++)
		if (strlen(__argv[i]) > 255) fatal("argument %d too long", i);

	return wrap(__argc, __argv);
}
#endif
