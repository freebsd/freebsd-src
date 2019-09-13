/*	$NetBSD: test_filecompletion.c,v 1.5 2019/09/08 05:50:58 abhinav Exp $	*/

/*-
 * Copyright (c) 2017 Abhinav Upadhyay <abhinav@NetBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"

#include <assert.h>
#include <err.h>
#include <stdio.h>
#include <histedit.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "filecomplete.h"
#include "el.h"

typedef struct {
	const wchar_t *user_typed_text; /* The actual text typed by the user on the terminal */
	const char *completion_function_input ; /*the text received by fn_filename_completion_function */
	const char *expanded_text[2]; /* the value to which completion_function_input should be expanded */
	const wchar_t *escaped_output; /* expected escaped value of expanded_text */
} test_input;

static test_input inputs[] = {
	{
		/* simple test for escaping angular brackets */
		L"ls ang",
		"ang",
		{"ang<ular>test", NULL},
		L"ls ang\\<ular\\>test "
	},
	{
		/* test angular bracket inside double quotes: ls "dq_ang */
		L"ls \"dq_ang",
		"dq_ang",
		{"dq_ang<ular>test", NULL},
		L"ls \"dq_ang<ular>test\""
	},
	{
		/* test angular bracket inside singlq quotes: ls "sq_ang */
		L"ls 'sq_ang",
		"sq_ang",
		{"sq_ang<ular>test", NULL},
		L"ls 'sq_ang<ular>test'"
	},
	{
		/* simple test for backslash */
		L"ls back",
		"back",
		{"backslash\\test", NULL},
		L"ls backslash\\\\test "
	},
	{
		/* backslash inside single quotes */
		L"ls 'sback",
		"sback",
		{"sbackslash\\test", NULL},
		L"ls 'sbackslash\\test'"
	},
	{
		/* backslash inside double quotes */
		L"ls \"dback",
		"dback",
		{"dbackslash\\test", NULL},
		L"ls \"dbackslash\\\\test\""
	},
	{
		/* test braces */
		L"ls br",
		"br",
		{"braces{test}", NULL},
		L"ls braces\\{test\\} "
	},
	{
		/* test braces inside single quotes */
		L"ls 'sbr",
		"sbr",
		{"sbraces{test}", NULL},
		L"ls 'sbraces{test}'"
	},
	{
		/* test braces inside double quotes */
		L"ls \"dbr",
		"dbr",
		{"dbraces{test}", NULL},
		L"ls \"dbraces{test}\""
	},
	{
		/* test dollar */
		L"ls doll",
		"doll",
		{"doll$artest", NULL},
		L"ls doll\\$artest "
	},
	{
		/* test dollar inside single quotes */
		L"ls 'sdoll",
		"sdoll",
		{"sdoll$artest", NULL},
		L"ls 'sdoll$artest'"
	},
	{
		/* test dollar inside double quotes */
		L"ls \"ddoll",
		"ddoll",
		{"ddoll$artest", NULL},
		L"ls \"ddoll\\$artest\""
	},
	{
		/* test equals */
		L"ls eq",
		"eq",
		{"equals==test", NULL},
		L"ls equals\\=\\=test "
	},
	{
		/* test equals inside sinqle quotes */
		L"ls 'seq",
		"seq",
		{"sequals==test", NULL},
		L"ls 'sequals==test'"
	},
	{
		/* test equals inside double quotes */
		L"ls \"deq",
		"deq",
		{"dequals==test", NULL},
		L"ls \"dequals==test\""
	},
	{
		/* test \n */
		L"ls new",
		"new",
		{"new\\nline", NULL},
		L"ls new\\\\nline "
	},
	{
		/* test \n inside single quotes */
		L"ls 'snew",
		"snew",
		{"snew\nline", NULL},
		L"ls 'snew\nline'"
	},
	{
		/* test \n inside double quotes */
		L"ls \"dnew",
		"dnew",
		{"dnew\nline", NULL},
		L"ls \"dnew\nline\""
	},
	{
		/* test single space */
		L"ls spac",
		"spac",
		{"space test", NULL},
		L"ls space\\ test "
	},
	{
		/* test single space inside singlq quotes */
		L"ls 's_spac",
		"s_spac",
		{"s_space test", NULL},
		L"ls 's_space test'"
	},
	{
		/* test single space inside double quotes */
		L"ls \"d_spac",
		"d_spac",
		{"d_space test", NULL},
		L"ls \"d_space test\""
	},
	{
		/* test multiple spaces */
		L"ls multi",
		"multi",
		{"multi space  test", NULL},
		L"ls multi\\ space\\ \\ test "
	},
	{
		/* test multiple spaces inside single quotes */
		L"ls 's_multi",
		"s_multi",
		{"s_multi space  test", NULL},
		L"ls 's_multi space  test'"
	},
	{
		/* test multiple spaces inside double quotes */
		L"ls \"d_multi",
		"d_multi",
		{"d_multi space  test", NULL},
		L"ls \"d_multi space  test\""
	},
	{
		/* test double quotes */
		L"ls doub",
		"doub",
		{"doub\"quotes", NULL},
		L"ls doub\\\"quotes "
	},
	{
		/* test double quotes inside single quotes */
		L"ls 's_doub",
		"s_doub",
		{"s_doub\"quotes", NULL},
		L"ls 's_doub\"quotes'"
	},
	{
		/* test double quotes inside double quotes */
		L"ls \"d_doub",
		"d_doub",
		{"d_doub\"quotes", NULL},
		L"ls \"d_doub\\\"quotes\""
	},
	{
		/* test multiple double quotes */
		L"ls mud",
		"mud",
		{"mud\"qu\"otes\"", NULL},
		L"ls mud\\\"qu\\\"otes\\\" "
	},
	{
		/* test multiple double quotes inside single quotes */
		L"ls 'smud",
		"smud",
		{"smud\"qu\"otes\"", NULL},
		L"ls 'smud\"qu\"otes\"'"
	},
	{
		/* test multiple double quotes inside double quotes */
		L"ls \"dmud",
		"dmud",
		{"dmud\"qu\"otes\"", NULL},
		L"ls \"dmud\\\"qu\\\"otes\\\"\""
	},
	{
		/* test one single quote */
		L"ls sing",
		"sing",
		{"single'quote", NULL},
		L"ls single\\'quote "
	},
	{
		/* test one single quote inside single quote */
		L"ls 'ssing",
		"ssing",
		{"ssingle'quote", NULL},
		L"ls 'ssingle'\\''quote'"
	},
	{
		/* test one single quote inside double quote */
		L"ls \"dsing",
		"dsing",
		{"dsingle'quote", NULL},
		L"ls \"dsingle'quote\""
	},
	{
		/* test multiple single quotes */
		L"ls mu_sing",
		"mu_sing",
		{"mu_single''quotes''", NULL},
		L"ls mu_single\\'\\'quotes\\'\\' "
	},
	{
		/* test multiple single quotes inside single quote */
		L"ls 'smu_sing",
		"smu_sing",
		{"smu_single''quotes''", NULL},
		L"ls 'smu_single'\\'''\\''quotes'\\\'''\\'''"
	},
	{
		/* test multiple single quotes inside double quote */
		L"ls \"dmu_sing",
		"dmu_sing",
		{"dmu_single''quotes''", NULL},
		L"ls \"dmu_single''quotes''\""
	},
	{
		/* test parenthesis */
		L"ls paren",
		"paren",
		{"paren(test)", NULL},
		L"ls paren\\(test\\) "
	},
	{
		/* test parenthesis inside single quote */
		L"ls 'sparen",
		"sparen",
		{"sparen(test)", NULL},
		L"ls 'sparen(test)'"
	},
	{
		/* test parenthesis inside double quote */
		L"ls \"dparen",
		"dparen",
		{"dparen(test)", NULL},
		L"ls \"dparen(test)\""
	},
	{
		/* test pipe */
		L"ls pip",
		"pip",
		{"pipe|test", NULL},
		L"ls pipe\\|test "
	},
	{
		/* test pipe inside single quote */
		L"ls 'spip",
		"spip",
		{"spipe|test", NULL},
		L"ls 'spipe|test'",
	},
	{
		/* test pipe inside double quote */
		L"ls \"dpip",
		"dpip",
		{"dpipe|test", NULL},
		L"ls \"dpipe|test\""
	},
	{
		/* test tab */
		L"ls ta",
		"ta",
		{"tab\ttest", NULL},
		L"ls tab\\\ttest "
	},
	{
		/* test tab inside single quote */
		L"ls 'sta",
		"sta",
		{"stab\ttest", NULL},
		L"ls 'stab\ttest'"
	},
	{
		/* test tab inside double quote */
		L"ls \"dta",
		"dta",
		{"dtab\ttest", NULL},
		L"ls \"dtab\ttest\""
	},
	{
		/* test back tick */
		L"ls tic",
		"tic",
		{"tick`test`", NULL},
		L"ls tick\\`test\\` "
	},
	{
		/* test back tick inside single quote */
		L"ls 'stic",
		"stic",
		{"stick`test`", NULL},
		L"ls 'stick`test`'"
	},
	{
		/* test back tick inside double quote */
		L"ls \"dtic",
		"dtic",
		{"dtick`test`", NULL},
		L"ls \"dtick\\`test\\`\""
	},
	{
		/* test for @ */
		L"ls at",
		"at",
		{"atthe@rate", NULL},
		L"ls atthe\\@rate "
	},
	{
		/* test for @ inside single quote */
		L"ls 'sat",
		"sat",
		{"satthe@rate", NULL},
		L"ls 'satthe@rate'"
	},
	{
		/* test for @ inside double quote */
		L"ls \"dat",
		"dat",
		{"datthe@rate", NULL},
		L"ls \"datthe@rate\""
	},
	{
		/* test ; */
		L"ls semi",
		"semi",
		{"semi;colon;test", NULL},
		L"ls semi\\;colon\\;test "
	},
	{
		/* test ; inside single quote */
		L"ls 'ssemi",
		"ssemi",
		{"ssemi;colon;test", NULL},
		L"ls 'ssemi;colon;test'"
	},
	{
		/* test ; inside double quote */
		L"ls \"dsemi",
		"dsemi",
		{"dsemi;colon;test", NULL},
		L"ls \"dsemi;colon;test\""
	},
	{
		/* test & */
		L"ls amp",
		"amp",
		{"ampers&and", NULL},
		L"ls ampers\\&and "
	},
	{
		/* test & inside single quote */
		L"ls 'samp",
		"samp",
		{"sampers&and", NULL},
		L"ls 'sampers&and'"
	},
	{
		/* test & inside double quote */
		L"ls \"damp",
		"damp",
		{"dampers&and", NULL},
		L"ls \"dampers&and\""
	},
	{
		/* test completion when cursor at \ */
		L"ls foo\\",
		"foo",
		{"foo bar", NULL},
		L"ls foo\\ bar "
	},
	{
		/* test completion when cursor at single quote */
		L"ls foo'",
		"foo'",
		{"foo bar", NULL},
		L"ls foo\\ bar "
	},
	{
		/* test completion when cursor at double quote */
		L"ls foo\"",
		"foo\"",
		{"foo bar", NULL},
		L"ls foo\\ bar "
	},
	{
		/* test multiple completion matches */
		L"ls fo",
		"fo",
		{"foo bar", "foo baz"},
		L"ls foo\\ ba"
	},
	{
		L"ls ba",
		"ba",
		{"bar <bar>", "bar <baz>"},
		L"ls bar\\ \\<ba"
	}
};

static const wchar_t break_chars[] = L" \t\n\"\\'`@$><=;|&{(";

/*
 * Custom completion function passed to fn_complet, NULLe.
 * The function returns hardcoded completion matches
 * based on the test cases present in inputs[] (above)
 */
static char *
mycomplet_func(const char *text, int index)
{
	static int last_index = 0;
	size_t i = 0;
	if (last_index == 2) {
		last_index = 0;
		return NULL;
	}

	for (i = 0; i < sizeof(inputs)/sizeof(inputs[0]); i++) {
		if (strcmp(text, inputs[i].completion_function_input) == 0) {
			if (inputs[i].expanded_text[last_index] != NULL)
				return strdup(inputs[i].expanded_text[last_index++]);
			else {
				last_index = 0;
				return NULL;
			}
		}
	}

	return NULL;
}

int
main(int argc, char **argv)
{
	EditLine *el = el_init(argv[0], stdin, stdout, stderr);
	size_t i; 
	size_t input_len;
	el_line_t line;
	wchar_t *buffer = malloc(64 * sizeof(*buffer));
	if (buffer == NULL)
		err(EXIT_FAILURE, "malloc failed");

	for (i = 0; i < sizeof(inputs)/sizeof(inputs[0]); i++) {
		memset(buffer, 0, 64 * sizeof(*buffer));
		input_len = wcslen(inputs[i].user_typed_text);
		wmemcpy(buffer, inputs[i].user_typed_text, input_len);
		buffer[input_len] = 0;
		line.buffer = buffer;
		line.cursor = line.buffer + input_len ;
		line.lastchar = line.cursor - 1;
		line.limit = line.buffer + 64 * sizeof(*buffer);
		el->el_line = line;
		fn_complete(el, mycomplet_func, NULL, break_chars, NULL, NULL, 10, NULL, NULL, NULL, NULL);

		/*
		 * fn_complete would have expanded and escaped the input in el->el_line.buffer.
		 * We need to assert that it matches with the expected value in our test data
		 */
		printf("User input: %ls\t Expected output: %ls\t Generated output: %ls\n",
				inputs[i].user_typed_text, inputs[i].escaped_output, el->el_line.buffer);
		assert(wcscmp(el->el_line.buffer, inputs[i].escaped_output) == 0);
	}
	el_end(el);
	return 0;

}
