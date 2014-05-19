#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/consio.h>
#include <sys/endian.h>
#include <sys/ioctl.h>
#include <sys/param.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct file_header {
	uint8_t		magic[8];
	uint8_t		width;
	uint8_t		height;
	uint16_t	pad;
	uint32_t	glyph_count;
	uint32_t	map_count[4];
} __packed;

static vfnt_map_t *
load_mappingtable(unsigned int nmappings)
{
	vfnt_map_t *t;
	unsigned int i;

	if (nmappings == 0)
		return (NULL);

	t = malloc(sizeof *t * nmappings);

	if (fread(t, sizeof *t * nmappings, 1, stdin) != 1) {
		perror("mappings");
		exit(1);
	}

	for (i = 0; i < nmappings; i++) {
		t[i].src = be32toh(t[i].src);
		t[i].dst = be16toh(t[i].dst);
		t[i].len = be16toh(t[i].len);
	}

	return (t);
}

int
main(int argc __unused, char *argv[] __unused)
{
	struct file_header fh;
	static vfnt_t vfnt;
	size_t glyphsize;
	unsigned int i;

	if (fread(&fh, sizeof fh, 1, stdin) != 1) {
		perror("file_header");
		return (1);
	}

	if (memcmp(fh.magic, "VFNT0002", 8) != 0) {
		fprintf(stderr, "Bad magic\n");
		return (1);
	}

	for (i = 0; i < VFNT_MAPS; i++)
		vfnt.map_count[i] = be32toh(fh.map_count[i]);
	vfnt.glyph_count = be32toh(fh.glyph_count);
	vfnt.width = fh.width;
	vfnt.height = fh.height;

	glyphsize = howmany(vfnt.width, 8) * vfnt.height * vfnt.glyph_count;
	vfnt.glyphs = malloc(glyphsize);

	if (fread(vfnt.glyphs, glyphsize, 1, stdin) != 1) {
		perror("glyphs");
		return (1);
	}

	for (i = 0; i < VFNT_MAPS; i++)
		vfnt.map[i] = load_mappingtable(vfnt.map_count[i]);

	if (ioctl(STDOUT_FILENO, PIO_VFONT, &vfnt) == -1) {
		perror("PIO_VFONT");
		return (1);
	}

	return (0);
}
