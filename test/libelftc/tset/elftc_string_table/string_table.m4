/*-
 * Copyright (c) 2013 Joseph Koshy
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id: string_table.m4 3021 2014-04-17 16:32:00Z jkoshy $
 */

/*
 * include(`elfts.m4')
 */
#include <sys/types.h>

#include <errno.h>
#include <fcntl.h>
#include <libelf.h>
#include <libelftc.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "tet_api.h"

/*
 * A list of test strings.
 *
 * For the curious, these are titles of stories by Cordwainer Smith.
 */

static const char *test_strings[] = {
	"Mark Elf",
	"Scanners Live in Vain",
	"The Dead Lady of Clown Town",
	"The Lady Who Sailed the \"The Soul\"",
	"No, No, Not Rogov!",
	NULL
};

static const int nteststrings = sizeof(test_strings) / sizeof(test_strings[0]);

static char test_image[] = {
	0,
	'M', 'a', 'r', 'k', ' ', 'E', 'l', 'f', 0,
	'S', 'c', 'a', 'n', 'n', 'e', 'r', 's', ' ',
	'L', 'i', 'v', 'e', ' ', 'i', 'n', ' ', 'V', 'a', 'i', 'n', 0,
	'T', 'h', 'e', ' ', 'D', 'e', 'a', 'd', ' ',
	'L', 'a', 'd', 'y', ' ', 'o', 'f', ' ', 'C', 'l', 'o', 'w', 'n', ' ',
	'T', 'o', 'w', 'n', 0,
	'T', 'h', 'e', ' ', 'L', 'a', 'd', 'y', ' ', 'W', 'h', 'o', ' ',
	'S', 'a', 'i', 'l', 'e', 'd', ' ', 't', 'h', 'e', ' ',
	'"', 'T', 'h', 'e',  'S', 'o', 'u', 'l', '"', 0,
	'N', 'o', ',', ' ', 'N', 'o', ',', ' ', 'N', 'o', 't', ' ',
	'R', 'o', 'g', 'o', 'v', '!', 0
};

#define	UNKNOWN_STRING	"Don't Panic!"

/*
 * Verify that strings are inserted at the expected offsets, and that
 * the returned value is equivalent to the original string.
 */
void
tcInsertReturnValues(void)
{
	int	result;
	const char **s;
	unsigned int expectedindex, hashindex;
	Elftc_String_Table *table;

	result = TET_UNRESOLVED;

	TP_ANNOUNCE("Insertion returns the expected offsets.");

	if ((table = elftc_string_table_create(0)) == NULL) {
		TP_UNRESOLVED("elftc_string_table_create() failed: %s",
		    strerror(errno));
		goto done;
	}

	expectedindex = 1;
	/* Insert test strings. */
	for (s = test_strings; *s != NULL; s++) {
		hashindex = elftc_string_table_insert(table, *s);
		if (hashindex != expectedindex) {
			TP_FAIL("incorrect hash index: expected %d, actual %d",
				expectedindex, hashindex);
			goto done;
		}

		expectedindex += strlen(*s) + 1;
	}

	result = TET_PASS;

done:
	if (table)
		(void) elftc_string_table_destroy(table);
	tet_result(result);
}

/*
 * Verify that multiple insertions of the same string yield the same
 * return values and offsets.
 */

void
tcInsertDuplicate(void)
{
	const char **s;
	int	n, result;
	Elftc_String_Table *table;
	unsigned int hindex, *hashrecord;

	result = TET_UNRESOLVED;

	TP_ANNOUNCE("Multiple insertions return the same offset value.");

	hashrecord = NULL;

	if ((table = elftc_string_table_create(0)) == NULL) {
		TP_UNRESOLVED("elftc_string_table_create() failed: %s",
		    strerror(errno));
		goto done;
	}

	if ((hashrecord = malloc(nteststrings*sizeof(*hashrecord))) == NULL) {
		TP_UNRESOLVED("memory allocation failed.");
		goto done;
	}

	/* Insert test strings. */
	for (n = 0, s = test_strings; *s != NULL; s++, n++)
		hashrecord[n] = elftc_string_table_insert(table, *s);

	/* Re-insert, and verify the returned pointers and offsets. */
	for (n = 0, s = test_strings; *s != NULL; s++, n++) {
		hindex = elftc_string_table_insert(table, *s);

		if (hindex != hashrecord[n]) {
			TP_FAIL("incorrect hash index: expected %d, actual %d",
				hashrecord[n], hindex);
			goto done;
		}
	}

	result = TET_PASS;

done:
	if (table)
		(void) elftc_string_table_destroy(table);
	free(hashrecord);

	tet_result(result);
}

/*
 * Verify that the lookup() API returns the expected
 * values.
 */

void
tcLookupReturn(void)
{
	int	result;
	const char **s, *str;
	unsigned int expectedindex, hashindex;
	Elftc_String_Table *table;

	result = TET_UNRESOLVED;

	TP_ANNOUNCE("A lookup after an insertion returns the correct "
	    "string.");

	if ((table = elftc_string_table_create(0)) == NULL) {
		TP_UNRESOLVED("elftc_string_table_create() failed: %s",
		    strerror(errno));
		goto done;
	}

	/* Insert test strings. */
	for (s = test_strings; *s != NULL; s++)
		(void) elftc_string_table_insert(table, *s);

	expectedindex = 1;
	for (s = test_strings; *s != NULL; s++) {
		hashindex = elftc_string_table_lookup(table, *s);

		if (hashindex != expectedindex) {
			TP_FAIL("incorrect hash index: expected %d, actual %d",
				expectedindex, hashindex);
			goto done;
		}

		str = elftc_string_table_to_string(table, hashindex);

		if (str == NULL || strcmp(str, *s)) {
			TP_FAIL("Lookup of \"%s\" returned \"%s\".", *s,
			    str);
			goto done;
		}

		expectedindex += strlen(*s) + 1;
	}

	result = TET_PASS;

done:
	if (table)
		(void) elftc_string_table_destroy(table);
	tet_result(result);
}

/*
 * Verify that multiple lookups return the same pointer
 * and string offset.
 */

void
tcLookupDuplicate(void)
{
	int	n, result;
	const char **s, *str1, *str2;
	unsigned int hindex1, hindex2, *hashrecord;
	Elftc_String_Table *table;

	result = TET_UNRESOLVED;

	TP_ANNOUNCE("Multiple invocations of lookup on a valid string "
	    "return the same value.");

	hashrecord = NULL;

	if ((table = elftc_string_table_create(0)) == NULL) {
		TP_UNRESOLVED("elftc_string_table_create() failed: %s",
		    strerror(errno));
		goto done;
	}

	if ((hashrecord = malloc(nteststrings*sizeof(*hashrecord))) == NULL) {
		TP_UNRESOLVED("memory allocation failed.");
		goto done;
	}

	/* Insert test strings. */
	for (n = 0, s = test_strings; *s != NULL; s++, n++)
		hashrecord[n] = elftc_string_table_insert(table, *s);

	for (n = 0, s = test_strings; *s != NULL; s++, n++) {
		hindex1 = elftc_string_table_lookup(table, *s);
		hindex2 = elftc_string_table_lookup(table, *s);

		if (hindex1 != hindex2 || hindex1 != hashrecord[n]) {
			TP_FAIL("incorrect hash index: expected %d, "
			    "actual %d & %d", hashrecord[n], hindex1,
			    hindex2);
			goto done;
		}

		str1 = elftc_string_table_to_string(table, hindex1);
		str2 = elftc_string_table_to_string(table, hindex2);
		if (str1 == NULL || str2 == NULL || str1 != str2 ||
		    strcmp(str1, *s)) {
			TP_FAIL("Lookup of \"%s\" returned \"%s\" & \"%s\".",
			    *s, str1, str2);
			goto done;
		}
	}

	result = TET_PASS;

done:
	if (table)
		(void) elftc_string_table_destroy(table);
	free(hashrecord);

	tet_result(result);
}

/*
 * Verify that a deleted string cannot be subsequently looked up.
 */

void
tcDeletionCheck(void)
{
	const char **s;
	Elftc_String_Table *table;
	int hindex, n, result, status;

	result = TET_UNRESOLVED;

	TP_ANNOUNCE("Lookup after deletion should fail.");

	if ((table = elftc_string_table_create(0)) == NULL) {
		TP_UNRESOLVED("elftc_string_table_create() failed: %s",
		    strerror(errno));
		goto done;
	}

	/* Insert test strings. */
	for (n = 0, s = test_strings; *s != NULL; s++, n++)
		(void) elftc_string_table_insert(table, *s);

	/* Delete strings, and look them up. */
	for (n = 0, s = test_strings; *s != NULL; s++, n++) {
		status = elftc_string_table_remove(table, *s);
		if (status == 0) {
			TP_FAIL("Deletion of \"%s\" failed.", *s);
			goto done;
		}

		hindex = elftc_string_table_lookup(table, *s);
		if (hindex != 0) {
			TP_FAIL("Lookup of \"%s\" succeeded unexpectedly.");
			goto done;
		}
	}

	result = TET_PASS;

done:
	if (table)
		(void) elftc_string_table_destroy(table);

	tet_result(result);

}

/*
 * Verify that a deleted string is re-inserted at the old index.
 */

void
tcDeletionInsertion(void)
{
	const char **s;
	unsigned int *hashrecord, hindex;
	Elftc_String_Table *table;
	int n, result, status;

	result = TET_UNRESOLVED;

	TP_ANNOUNCE("Re-insertion of a string after deletion should "
	    "return the prior offset.");

	hashrecord = NULL;

	if ((table = elftc_string_table_create(0)) == NULL) {
		TP_UNRESOLVED("elftc_string_table_create() failed: %s",
		    strerror(errno));
		goto done;
	}

	if ((hashrecord = malloc(nteststrings*sizeof(*hashrecord))) == NULL) {
		TP_UNRESOLVED("memory allocation failed.");
		goto done;
	}

	/* Insert test strings. */
	for (n = 0, s = test_strings; *s != NULL; s++, n++)
		hashrecord[n] = elftc_string_table_insert(table, *s);

	/* Delete strings ... */
	for (n = 0, s = test_strings; *s != NULL; s++, n++) {
		status = elftc_string_table_remove(table, *s);
		if (status == 0) {
			TP_UNRESOLVED("Deletion of \"%s\" failed.", *s);
			goto done;
		}
	}

	/* and re-insert them, and check. */
	for (n = 0, s = test_strings; *s != NULL; s++, n++) {
		hindex = elftc_string_table_insert(table, *s);

		if (hindex != hashrecord[n]) {
			TP_FAIL("Re-insertion at a different offset: "
			    "old %d, new %p", hashrecord[n], hindex);
			goto done;
		}
	}

	result = TET_PASS;

done:
	if (table)
		(void) elftc_string_table_destroy(table);
	if (hashrecord)
		free(hashrecord);

	tet_result(result);

}

/*
 * Verify that the 2nd deletion of the string fails.
 */

void
tcDoubleDeletion(void)
{
	const char **s;
	int n, result, status;
	Elftc_String_Table *table;

	result = TET_UNRESOLVED;

	TP_ANNOUNCE("Double deletion of a string should fail.");

	if ((table = elftc_string_table_create(0)) == NULL) {
		TP_UNRESOLVED("elftc_string_table_create() failed: %s",
		    strerror(errno));
		goto done;
	}

	/* Insert test strings. */
	for (n = 0, s = test_strings; *s != NULL; s++, n++)
		(void) elftc_string_table_insert(table, *s);

	/* Delete strings twice. */
	for (n = 0, s = test_strings; *s != NULL; s++, n++) {
		status = elftc_string_table_remove(table, *s);
		if (status == 0) {
			TP_FAIL("First deletion of \"%s\" failed.", *s);
			goto done;
		}
		status = elftc_string_table_remove(table, *s);
		if (status != 0) {
			TP_FAIL("Second deletion of \"%s\" succeeded.", *s);
			goto done;
		}
	}

	result = TET_PASS;

done:
	if (table)
		(void) elftc_string_table_destroy(table);

	tet_result(result);

}

/*
 * Verify that deletion of an unknown string fails.
 */

void
tcUnknownDeletion(void)
{
	const char **s;
	int n, result, status;
	Elftc_String_Table *table;

	result = TET_UNRESOLVED;

	TP_ANNOUNCE("Deletion of an unknown string should fail.");

	if ((table = elftc_string_table_create(0)) == NULL) {
		TP_UNRESOLVED("elftc_string_table_create() failed: %s",
		    strerror(errno));
		goto done;
	}

	/* Insert test strings. */
	for (n = 0, s = test_strings; *s != NULL; s++, n++)
		(void) elftc_string_table_insert(table, *s);

	status = elftc_string_table_remove(table, UNKNOWN_STRING);
	if (status != 0) {
		TP_FAIL("Deletion of an unknown string succeeded.");
		goto done;
	}

	result = TET_PASS;

done:
	if (table)
		(void) elftc_string_table_destroy(table);

	tet_result(result);

}

/*
 * Ensure that string indices remain constant after the underlying
 * string pool is resized/moved.
 */

#define	TC_STRING_SIZE	64
#define	TC_INSERT_SIZE	20

void
tcIndicesAfterRebase(void)
{
	int j, n, result;
	const char *str1, *str2;
	char buf[TC_STRING_SIZE];
	Elftc_String_Table *table;
	unsigned int offset1, offset2, expectedoffset;

	n = 0;
	result = TET_UNRESOLVED;

	TP_ANNOUNCE("Indices are consistent after a pool resize.");

	if ((table = elftc_string_table_create(0)) == NULL) {
		TP_UNRESOLVED("elftc_string_table_create() failed: %s",
		    strerror(errno));
		goto done;
	}

	/* Insert test strings. */
#define	TC_BUILD_STRING(buf, n) do {				\
		(void) snprintf(buf, sizeof(buf), "%-*.*d",	\
		    TC_INSERT_SIZE - 1, TC_INSERT_SIZE - 1, n);	\
	} while (0)

	TC_BUILD_STRING(buf, n);

	offset1 = elftc_string_table_insert(table, buf);
	str1 = elftc_string_table_to_string(table, offset1);

	if (offset1 == 0 || str1 == NULL) {
		TP_UNRESOLVED("Initialization failed.");
		goto done;
	}

	n++;

	/*
	 * Insert unique strings till we detect a move of the initial
	 * string.
	 */
	do {

		TC_BUILD_STRING(buf, n);

		if ((offset2 = elftc_string_table_insert(table, buf)) == 0) {
			TP_UNRESOLVED("String insertion failed at %d", n);
			goto done;
		}

		if ((str2 = elftc_string_table_to_string(table, offset1)) ==
		    NULL) {
			TP_UNRESOLVED("String looked failed at %d", n);
			goto done;
		}

		n++;
	} while (str2 == str1);

	/*
	 * Verify the offset for each string inserted so far.
	 */
	expectedoffset = 1;
	for (j = 0; j < n; j++) {
		TC_BUILD_STRING(buf, j);
		offset1 = elftc_string_table_lookup(table, buf);
		if (offset1 != expectedoffset) {
			TP_FAIL("Offset mismatch: string #%d, "
			    "expected %d, actual %d", j, expectedoffset,
			    offset1);
			goto done;
		}
		expectedoffset += TC_INSERT_SIZE;
	}

	result = TET_PASS;

done:
	if (table)
		(void) elftc_string_table_destroy(table);

	tet_result(result);
}

void
tcEmptyImage(void)
{
	int result;
	size_t tblsz;
	const char *image;
	Elftc_String_Table *table;

	TP_ANNOUNCE("Check the image for an empty table.");

	if ((table = elftc_string_table_create(0)) == NULL) {
		TP_UNRESOLVED("elftc_string_table_create() failed: %s",
		    strerror(errno));
		goto done;
	}

	tblsz = 0;
	image = elftc_string_table_image(table, &tblsz);
	if (image == NULL) {
		TP_FAIL("Null image returned.");
		goto done;
	}

	if (*image != 0 || tblsz != 1) {
		TP_FAIL("Incorrect image parameters: [0]=%d, sz=%d",
		    *image, tblsz);
		goto done;
	}

	result = TET_PASS;

done:
	if (table)
		(void) elftc_string_table_destroy(table);

	tet_result(result);
}

void
tcImageDeleted(void)
{
	const char **s;
	int n, result;
	size_t tblsz;
	const char *image;
	Elftc_String_Table *table;

	TP_ANNOUNCE("Check the image for an empty table.");

	if ((table = elftc_string_table_create(0)) == NULL) {
		TP_UNRESOLVED("elftc_string_table_create() failed: %s",
		    strerror(errno));
		goto done;
	}

	/* Insert, then delete a set of strings. */
	for (n = 0, s = test_strings; *s != NULL; s++, n++)
		if (elftc_string_table_insert(table, *s) == 0) {
			TP_UNRESOLVED("String insertion of \"%s\" failed.",
			    *s);
			goto done;
		}

	for (n = 0, s = test_strings; *s != NULL; s++, n++)
		if (elftc_string_table_remove(table, *s) == 0) {
			TP_UNRESOLVED("String deletion of \"%s\" failed.",
			    *s);
			goto done;
		}

	/* Check for an empty table. */
	tblsz = 0;
	image = elftc_string_table_image(table, &tblsz);
	if (image == NULL) {
		TP_FAIL("Null image returned.");
		goto done;
	}

	if (*image != 0 || tblsz != 1) {
		TP_FAIL("Incorrect image parameters: [0]=%d, sz=%d",
		    *image, tblsz);
		goto done;
	}

	result = TET_PASS;

done:
	if (table)
		(void) elftc_string_table_destroy(table);

	tet_result(result);
}

static int
validate_string_table(int nstrings, const char *image, size_t imagesz,
    const char **strings)
{
	int n, result, *seen;
	const char *s, *end;

	if (*image != '\0')
		return (0);

	if (nstrings == 0)
		return (1);

	if ((seen = calloc(nstrings, sizeof(int))) == NULL)
		return (0);

	/*
	 * Each string in the image should be in strings[],
	 * and vice-versa.
	 */
	s = image + 1;
	end = image + imagesz;
	while (s < end) {
		/* Look for this string in the strings[] array. */
		for (n = 0; n < nstrings; n++)
			if (strcmp(s, strings[n]) == 0) {
				seen[n] = 1;
				break;
			}
		if (n == nstrings) /* Not in the strings[] array. */
			goto fail;

		s += strlen(s) + 1;
	}

	/* Verify all strings in the array were seen. */
	for (n = 0; n < nstrings; n++)
		if (seen[n] == 0)
			goto fail;

	free(seen);

	return (1);

fail:
	free(seen);
	return (0);
}

void
tcImageInsertOnly(void)
{
	int result;
	const char **s;
	const char *image;
	Elftc_String_Table *table;
	size_t expectedsize, imagesz;

	TP_ANNOUNCE("Insertion returns the expected image.");

	if ((table = elftc_string_table_create(0)) == NULL) {
		TP_UNRESOLVED("elftc_string_table_create() failed: %s",
		    strerror(errno));
		goto done;
	}

	result = TET_PASS;

	expectedsize = 1;
	for (s = test_strings; *s != NULL; s++) {
		expectedsize += strlen(*s) + 1;
		if(elftc_string_table_insert(table, *s) == 0) {
			TP_UNRESOLVED("String insertion failed for \"%s\".",
			    *s);
			goto done;
		}
	}

	imagesz = 0;
	image = elftc_string_table_image(table, &imagesz);

	if (image == NULL || imagesz != expectedsize) {
		TP_FAIL("Incorrect image parameters: [0]=%d, sz=%d != %d",
		    *image, imagesz, expectedsize);
		goto done;
	}

	if (!validate_string_table(nteststrings - 1, image, imagesz,
		test_strings)) {
		TP_FAIL("Image mismatch.");
		goto done;
	}

	result = TET_PASS;

done:
	if (table)
		(void) elftc_string_table_destroy(table);
	tet_result(result);
}

void
tcImagePartiallyDeleted(void)
{
	int n, nstr, result;
	Elftc_String_Table *table;
	size_t expectedsize, imagesz;
	const char *image, **s, **savedstr;

	TP_ANNOUNCE("Insertion+deletion returns the expected image.");

	savedstr = NULL;

	if ((table = elftc_string_table_create(0)) == NULL) {
		TP_UNRESOLVED("elftc_string_table_create() failed: %s",
		    strerror(errno));
		goto done;
	}

	expectedsize = 1;
	for (nstr = 0, s = test_strings; *s != NULL; s++, nstr++) {
		expectedsize += strlen(*s) + 1;
		if(elftc_string_table_insert(table, *s) == 0) {
			TP_UNRESOLVED("String insertion failed for \"%s\".",
			    *s);
			goto done;
		}
	}

	if ((savedstr = malloc(sizeof(*savedstr) * nstr)) == NULL) {
		TP_UNRESOLVED("Memory allocation failed.");
		goto done;
	}

	for (nstr = n = 0, s = test_strings; *s != NULL; s++, n++) {
		if ((n & 1) == 0) {
			savedstr[nstr++] = *s;
			continue;
		}

		expectedsize -= (strlen(*s) + 1);
		if (elftc_string_table_remove(table, *s) == 0) {
			TP_UNRESOLVED("String removal failed for \"%s\".",
			    *s);
			goto done;
		}
	}

	imagesz = 0;
	image = elftc_string_table_image(table, &imagesz);

	if (image == NULL || imagesz != expectedsize) {
		TP_FAIL("Incorrect image parameters: [0]=%d, sz=%d != %d",
		    *image, imagesz, expectedsize);
		goto done;
	}

	if (!validate_string_table(nstr, image, imagesz, savedstr)) {
		TP_FAIL("Image mismatch.");
		goto done;
	}

	result = TET_PASS;

done:
	if (table)
		(void) elftc_string_table_destroy(table);
	free(savedstr);
	tet_result(result);
}

/*
 * Verify that initialization from a ELF string table works.
 */

void
tcFromSection(void)
{
	Elf *e;
	Elf_Data *d;
	Elf_Scn *scn;
	int fd, result;
	const char *image;
	Elf32_Ehdr *eh;
	Elf32_Shdr *shdr;
	Elftc_String_Table *table;
	size_t imagesz, scnindex;

	table = NULL;
	result = TET_UNRESOLVED;

	TP_ANNOUNCE("Loading a table form an ELF section works correctly.");

	fd = -1;
	e = NULL;
	scn = NULL;
	d = NULL;

	/*
	 * Create the ELF section.
	 */
	if ((fd = open("/dev/null", O_RDONLY)) < 0) {
		TP_UNRESOLVED("File open failed.");
		goto done;
	}

	if (elf_version(EV_CURRENT) == EV_NONE) {
		TP_UNRESOLVED("libelf initialization failed.");
		goto done;
	}

	if ((e = elf_begin(fd, ELF_C_WRITE, NULL)) == NULL) {
		TP_UNRESOLVED("Elf open failed: %s",
		    elf_errmsg(-1));
		goto done;
	}

	if ((eh = elf32_getehdr(e)) == NULL) {
		TP_UNRESOLVED("Elf open failed: %s",
		    elf_errmsg(-1));
		goto done;
	}

	eh->e_ident[EI_DATA] = ELFDATA2LSB;
	eh->e_machine = EM_386;
	eh->e_version = EV_CURRENT;

	if ((scn = elf_newscn(e)) == NULL) {
		TP_UNRESOLVED("Elf newscn failed: %s",
		    elf_errmsg(-1));
		goto done;
	}

	scnindex = elf_ndxscn(scn);

	if ((shdr = elf32_getshdr(scn)) == NULL) {
		TP_UNRESOLVED("Elf getshdr failed: %s",
		    elf_errmsg(-1));
		goto done;
	}

	shdr->sh_type = SHT_STRTAB;

	if ((d = elf_newdata(scn)) == NULL) {
		TP_UNRESOLVED("Elf newdata failed: %s",
		    elf_errmsg(-1));
		goto done;
	}

	d->d_buf = test_image;
	d->d_size = sizeof(test_image);

	if (elf_update(e, ELF_C_NULL) < 0) {
		TP_UNRESOLVED("elf_update failed: %s",
		    elf_errmsg(-1));
		goto done;
	}

	if ((scn = elf_getscn(e, scnindex)) == NULL) {
		TP_UNRESOLVED("Elf getscn failed: %s",
		    elf_errmsg(-1));
		goto done;
	}

	/* Create a string table from the contents. */
	if ((table = elftc_string_table_from_section(scn, 0)) == NULL) {
		TP_FAIL("from_section call failed.");
		goto done;
	}

	/* Retrieve the image. */
	if ((image = elftc_string_table_image(table, &imagesz)) == NULL) {
		TP_FAIL("from_section call failed.");
		goto done;
	}

	/* Check the retrieved image against the original. */
	if (imagesz != sizeof(test_image) ||
	    memcmp(image, test_image, imagesz) != 0) {
		TP_FAIL("image compare failed.");
		goto done;
	}

	result = TET_PASS;

done:
	if (table)
		(void) elftc_string_table_destroy(table);
	if (e)
		elf_end(e);
	if (fd != -1)
		(void) close(fd);
	tet_result(result);
}
