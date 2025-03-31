/*
 *  Copyright (C) 2017 - This file is part of libecc project
 *
 *  Authors:
 *      Ryad BENADJILA <ryadbenadjila@gmail.com>
 *      Arnaud EBALARD <arnaud.ebalard@ssi.gouv.fr>
 *      Jean-Pierre FLORI <jean-pierre.flori@ssi.gouv.fr>
 *
 *  Contributors:
 *      Nicolas VIVET <nicolas.vivet@ssi.gouv.fr>
 *      Karim KHALFALLAH <karim.khalfallah@ssi.gouv.fr>
 *
 *  This software is licensed under a dual BSD and GPL v2 license.
 *  See LICENSE file at the root folder of the project.
 */
#include <libecc/libarith.h>
/* Include internal API as it is used in our tests */
#include "../nn/nn_div.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#ifdef WITH_ASSERT_BACKTRACE
#include <signal.h>
#include <execinfo.h>

#define BACKTRACE_SIZE 4096
static unsigned int backtrace_buffer_ptr = 0;
static char backtrace_buffer[BACKTRACE_SIZE];

/* assert trapping and backtracing */
static void assert_signal_handler(int sig)
{
	if (sig != SIGINT) {
		raise(sig);
	}
	/* Print the recorded backtrace */
	printf("**** BACKTRACE *****\n");
	printf("(from old to most recent calls)\n");
	printf("%s", backtrace_buffer);
	exit(-1);
}

#define ADD_TO_BACKTRACE(...) do {\
	int written_size;\
	written_size = snprintf(backtrace_buffer + backtrace_buffer_ptr, BACKTRACE_SIZE - 1 - backtrace_buffer_ptr, __VA_ARGS__);\
	backtrace_buffer_ptr += written_size;\
	if(backtrace_buffer_ptr >= BACKTRACE_SIZE - 1){\
		memset(backtrace_buffer, 0, sizeof(backtrace_buffer)-1);\
		backtrace_buffer_ptr = 0;\
	}\
} while(0)
#else

#define ADD_TO_BACKTRACE(...) do {\
} while(0)

#endif

/*
 * Import integer number (found in hexadecimal form in hbuf buffer
 * of length hbuflen) into already allocated out_nn. hbuflen is
 * expected to be of even size. out_nn parameter is expected to
 * have a large enough storage space (i.e. hbuflen / 2) to hold
 * imported number.
 */
static int nn_import_from_hexbuf(nn_t out_nn, const char *hbuf, u32 hbuflen)
{
	char buf[WORD_BYTES * 2 + 1];
	const char *start;
	u32 wlen;
	u32 k;
	int ret;

	ret = nn_check_initialized(out_nn); EG(ret, err);
	MUST_HAVE((hbuf != NULL), ret, err);
	MUST_HAVE(((hbuflen / 2) / WORD_BYTES) == out_nn->wlen, ret, err);

	wlen = (hbuflen + WORD_BYTES - 1) / (2 * WORD_BYTES);
	for (k = wlen; k > 0; k--) {
		/*
		 * Copy current hex encoded word into null terminated
		 * scratch buffer
		 */
		memset(buf, 0, WORD_BYTES * 2 + 1);
		start = hbuf + ((k - 1) * WORD_BYTES * 2);
		memcpy(buf, start, WORD_BYTES * 2);

		/* Let strtoull() convert the value for us */
		out_nn->val[wlen - k] = strtoull(buf, NULL, 16);
	}

	for (k = NN_MAX_WORD_LEN; k > wlen; k--) {
		out_nn->val[k - 1] = 0;
	}

err:
	return ret;
}

#define DISPATCH_TABLE_MAGIC "FEEDBABE"
struct dispatch_table {
	const char magic[sizeof(DISPATCH_TABLE_MAGIC)];
	const char *op_string;
	const char *op_string_helper;
	int (*fun) (const char *op, void **, int);
};

#define ADD_TO_DISPATCH_TABLE(fun, op_string, op_string_helper) \
	static const struct dispatch_table entry_##fun \
		ATTRIBUTE_SECTION("tests_dispatch_table_section") ATTRIBUTE_USED = \
		{ DISPATCH_TABLE_MAGIC, op_string, op_string_helper, fun };

#define FIND_IN_DISPATCH_TABLE(op, to_find, type) do {\
	extern struct dispatch_table __start_tests_dispatch_table_section;\
	extern struct dispatch_table __stop_tests_dispatch_table_section;\
	struct dispatch_table *dt, *begin, *end;\
	char *ptr;\
	\
	begin = &__start_tests_dispatch_table_section;\
	end = &__stop_tests_dispatch_table_section;\
	ptr = (char*)begin;\
	\
	to_find = NULL;\
	\
	while(ptr < (char*)end){\
		dt = (struct dispatch_table*)ptr;\
		/* Find the magic */\
		while(memcmp(dt->magic, DISPATCH_TABLE_MAGIC, sizeof(DISPATCH_TABLE_MAGIC)) != 0){\
			ptr++;\
			dt = (struct dispatch_table*)ptr;\
		}\
		if(strcmp(dt->op_string, op) == 0){ \
			to_find = dt->type;\
			break;\
		}\
		ptr += sizeof(struct dispatch_table);\
	}\
} while(0)

#define FIND_FUN_IN_DISPATCH_TABLE(op, function) FIND_IN_DISPATCH_TABLE(op, function, fun)

#define FIND_HELPER_IN_DISPATCH_TABLE(op, string_helper) FIND_IN_DISPATCH_TABLE(op, string_helper, op_string_helper)

/*****************/

#define GENERIC_TEST_FP_DECL_INIT0(name, ctx) \
	fp_t name##_ptr[] = {NULL};

#define GENERIC_TEST_FP_DECL_INIT1(name, ctx) \
	fp name##0; \
	fp_t name##_ptr[] = { &name##0 };\
	ret |= fp_init(&name##0, ctx);\

#define GENERIC_TEST_FP_DECL_INIT2(name, ctx) \
	fp name##0, name##1;\
	fp_t name##_ptr[] = { &name##0, &name##1 };\
	ret |= fp_init(&name##0, ctx);\
	ret |= fp_init(&name##1, ctx);\

#define GENERIC_TEST_FP_DECL_INIT3(name, ctx) \
	fp name##0, name##1, name##2;\
	fp_t name##_ptr[] = { &name##0, &name##1, &name##2 };\
	ret |= fp_init(&name##0, ctx);\
	ret |= fp_init(&name##1, ctx);\
	ret |= fp_init(&name##2, ctx);\

#define GENERIC_TEST_FP_DECL_INIT4(name, ctx) \
	fp name##0, name##1, name##2, name##3;\
	fp_t name##_ptr[] = { &name##0, &name##1, &name##2, &name##3 };\
	ret |= fp_init(&name##0, ctx);\
	ret |= fp_init(&name##1, ctx);\
	ret |= fp_init(&name##2, ctx);\
	ret |= fp_init(&name##3, ctx);\

#define GENERIC_TEST_FP_DECL_INIT5(name, ctx) \
	fp name##0, name##1, name##2, name##3, name##4;\
	fp_t name##_ptr[] = { &name##0, &name##1, &name##2, &name##3, &name##4 };\
	ret |= fp_init(&name##0, ctx);\
	ret |= fp_init(&name##1, ctx);\
	ret |= fp_init(&name##2, ctx);\
	ret |= fp_init(&name##3, ctx);\
	ret |= fp_init(&name##4, ctx);\

#define GENERIC_TEST_FP_DECL_INIT6(name, ctx) \
	fp name##0, name##1, name##2, name##3, name##4, name##5;\
	fp_t name##_ptr[] = { &name##0, &name##1, &name##2, &name##3, &name##4, &name##5 };\
	ret |= fp_init(&name##0, ctx);\
	ret |= fp_init(&name##1, ctx);\
	ret |= fp_init(&name##2, ctx);\
	ret |= fp_init(&name##3, ctx);\
	ret |= fp_init(&name##4, ctx);\
	ret |= fp_init(&name##5, ctx);\

#define GENERIC_TEST_NN_DECL_INIT0(name, size) \
	nn_t name##_ptr[] = {NULL};

#define GENERIC_TEST_NN_DECL_INIT1(name, size) \
	nn name##0;			       \
	nn_t name##_ptr[] = { &name##0 };      \
	ret |= nn_init(&name##0, size); \

#define GENERIC_TEST_NN_DECL_INIT2(name, size)       \
	nn name##0, name##1;			     \
	nn_t name##_ptr[] = { &name##0, &name##1 };  \
	ret |= nn_init(&name##0, size); \
	ret |= nn_init(&name##1, size); \

#define GENERIC_TEST_NN_DECL_INIT3(name, size)                  \
	nn name##0, name##1, name##2;				\
	nn_t name##_ptr[] = { &name##0, &name##1, &name##2 };	\
	ret |= nn_init(&name##0, size);		\
	ret |= nn_init(&name##1, size);		\
	ret |= nn_init(&name##2, size);   	\

#define GENERIC_TEST_NN_DECL_INIT4(name, size)				\
	nn name##0, name##1, name##2, name##3;				\
	nn_t name##_ptr[] = { &name##0, &name##1, &name##2, &name##3 }; \
	ret |= nn_init(&name##0, size);			\
	ret |= nn_init(&name##1, size);			\
	ret |= nn_init(&name##2, size);			\
	ret |= nn_init(&name##3, size);			\

#define GENERIC_TEST_NN_DECL_INIT5(name, size)		    \
	nn name##0, name##1, name##2, name##3, name##4;	    \
	nn_t name##_ptr[] = { &name##0, &name##1, &name##2, &name##3, &name##4 };\
	ret |= nn_init(&name##0, size);	    \
	ret |= nn_init(&name##1, size);	    \
	ret |= nn_init(&name##2, size);	    \
	ret |= nn_init(&name##3, size);	    \
	ret |= nn_init(&name##4, size);	    \

#define GENERIC_TEST_NN_DECL_INIT6(name, size)				\
	nn name##0, name##1, name##2, name##3, name##4, name##5;	\
	nn_t name##_ptr[] = { &name##0, &name##1, &name##2, &name##3, &name##4, &name##5 };\
	ret |= nn_init(&name##0, size);		\
	ret |= nn_init(&name##1, size);		\
	ret |= nn_init(&name##2, size);		\
	ret |= nn_init(&name##3, size);		\
	ret |= nn_init(&name##4, size);		\
	ret |= nn_init(&name##5, size);		\

#define GENERIC_TEST_FP_CLEAR0(name)

#define GENERIC_TEST_FP_CLEAR1(name) \
	fp_uninit(&name##0);\

#define GENERIC_TEST_FP_CLEAR2(name) \
	fp_uninit(&name##0);\
	fp_uninit(&name##1);\

#define GENERIC_TEST_FP_CLEAR3(name) \
	fp_uninit(&name##0);\
	fp_uninit(&name##1);\
	fp_uninit(&name##2);\

#define GENERIC_TEST_FP_CLEAR4(name) \
	fp_uninit(&name##0);\
	fp_uninit(&name##1);\
	fp_uninit(&name##2);\
	fp_uninit(&name##3);\

#define GENERIC_TEST_FP_CLEAR5(name) \
	fp_uninit(&name##0);\
	fp_uninit(&name##1);\
	fp_uninit(&name##2);\
	fp_uninit(&name##3);\
	fp_uninit(&name##4);\

#define GENERIC_TEST_FP_CLEAR6(name) \
	fp_uninit(&name##0);\
	fp_uninit(&name##1);\
	fp_uninit(&name##2);\
	fp_uninit(&name##3);\
	fp_uninit(&name##4);\
	fp_uninit(&name##5);\

#define GENERIC_TEST_nn_uninit0(name)

#define GENERIC_TEST_nn_uninit1(name) \
	nn_uninit(&name##0);\

#define GENERIC_TEST_nn_uninit2(name) \
	nn_uninit(&name##0);\
	nn_uninit(&name##1);\

#define GENERIC_TEST_nn_uninit3(name) \
	nn_uninit(&name##0);\
	nn_uninit(&name##1);\
	nn_uninit(&name##2);\

#define GENERIC_TEST_nn_uninit4(name) \
	nn_uninit(&name##0);\
	nn_uninit(&name##1);\
	nn_uninit(&name##2);\
	nn_uninit(&name##3);\

#define GENERIC_TEST_nn_uninit5(name) \
	nn_uninit(&name##0);\
	nn_uninit(&name##1);\
	nn_uninit(&name##2);\
	nn_uninit(&name##3);\
	nn_uninit(&name##4);\

#define GENERIC_TEST_nn_uninit6(name) \
	nn_uninit(&name##0);\
	nn_uninit(&name##1);\
	nn_uninit(&name##2);\
	nn_uninit(&name##3);\
	nn_uninit(&name##4);\
	nn_uninit(&name##5);\

#define FP_CTX_T_GENERIC_IN(num) ((fp_ctx_t)params[num])
#define FP_T_GENERIC_IN(num) ((fp_t)params[num])
#define NN_T_GENERIC_IN(num) ((nn_t)params[num])
#define UINT_GENERIC_IN(num) ((u64)*((u64*)params[num]))
#define WORD_T_GENERIC_IN(num) ((word_t)*((word_t*)params[num]))
#define INT_GENERIC_IN(num) ((int)*((int*)params[num]))

#define FP_T_GENERIC_OUT(num) (&fp_out##num)
#define NN_T_GENERIC_OUT(num) (&nn_out##num)
#define WORD_T_GENERIC_OUT(num) (&(word_out[num]))
#define INT_GENERIC_OUT(num) (&(int_out[num]))

#define CHECK_FUN_RET there_is_output = 1; fun_out_value = (int)

#define CHECK_FUN_NO_RET there_is_output = 0; fun_out_value = (int)

/* Number of pre-allocated */
#define NUM_PRE_ALLOCATED_NN 6
#define NUM_PRE_ALLOCATED_FP 6
#define MAX_PARAMS 6

#define GENERIC_TEST_NN_DECL_INIT_MAX(name, n) GENERIC_TEST_NN_DECL_INIT6(name, n)
#define GENERIC_TEST_FP_DECL_INIT_MAX(name, ctx) GENERIC_TEST_FP_DECL_INIT6(name, ctx)

/* Check that the string of parameters types only containes 'c', 'f', 'n' and 'u'
 * Check that the string of parameters I/O only contains 'i', 'o' and 'O'
 *
 */
#define PARAMETERS_SANITY_CHECK(test_num, param_types, param_io) do {\
	unsigned int i, real_output = 0;\
	assert(sizeof(param_types) == sizeof(param_io));\
	for(i = 0; i < sizeof(param_types)-1; i++){\
		if((param_types[i] != 'c') && (param_types[i] != 'f') && (param_types[i] != 'n') && (param_types[i] != 'u') && (param_types[i] != 's')){ \
			printf("Error: types parameters of test %d mismatch!\n", test_num);\
			return 0;\
		}\
		if((param_io[i] != 'i') && (param_io[i] != 'o') && (param_io[i] != 'O')){\
			printf("Error: I/O parameters of test %d mismatch!\n", test_num);\
			return 0;\
		}\
		if((param_io[i] == 'O') && (param_types[i] != 'u') && (param_types[i] != 's')){\
			printf("Error: types and I/O parameters of test %d mismatch!\n", test_num);\
			return 0;\
		}\
		if(param_io[i] == 'O'){\
			real_output++;\
		}\
	}\
	/* Check that we only have one function output */\
	if(real_output > 1){\
		printf("Error: multiple function output defined in I/O parameters of test %d!\n", test_num);\
		return 0;\
	}\
} while(0);

#define SET_PARAMETER_PRETTY_NAME1(a) a
#define SET_PARAMETER_PRETTY_NAME2(a, b) SET_PARAMETER_PRETTY_NAME1(a) "\0" b
#define SET_PARAMETER_PRETTY_NAME3(a, b, c) SET_PARAMETER_PRETTY_NAME2(a, b) "\0" c
#define SET_PARAMETER_PRETTY_NAME4(a, b, c, d) SET_PARAMETER_PRETTY_NAME3(a, b, c) "\0" d
#define SET_PARAMETER_PRETTY_NAME5(a, b, c, d, e) SET_PARAMETER_PRETTY_NAME4(a, b, c, d) "\0" e
#define SET_PARAMETER_PRETTY_NAME6(a, b, c, d, e, f) SET_PARAMETER_PRETTY_NAME5(a, b, c, d, e) "\0" f

#define SET_PARAMETER_PRETTY_NAME(num, ...) SET_PARAMETER_PRETTY_NAME##num(__VA_ARGS__)

/* Parse the helper string to get the pretty print names */
#define GET_PARAMETER_PRETTY_NAME(parameters_string_names_, parameters_string_names, num, out) do {\
	unsigned int cnt = 0;\
	out = 0;\
	\
	/* Find the proper position */\
	while(out < sizeof(parameters_string_names_)-1){\
		if(cnt == num){\
			break;\
		}\
		if(parameters_string_names[out] == '\0'){\
			cnt++;\
		}\
		out++;\
	}\
} while(0);

/* Print for a given test all the inputs, outpus and expected outputs */
#define PRINT_ALL(parameters_types, parameters_io, params, nn_out_ptr, fp_out_ptr, fun_output, there_is_output, parameters_string_names_, bad_num) do { \
	unsigned int j;\
	unsigned int nn_out_local_cnt = 0, fp_out_local_cnt = 0;\
	unsigned int str_pos;\
	const char parameters_string_names[] = parameters_string_names_;\
	const char real[] = "Real ";\
	const char expected[] = "Expected ";\
	char expected_modified_string_names[sizeof(expected)+sizeof(parameters_string_names_)];\
	char real_modified_string_names[sizeof(real)+sizeof(parameters_string_names_)];\
	/* First print the inputs */\
	for(j=0; j<sizeof(parameters_types)-1; j++){\
		GET_PARAMETER_PRETTY_NAME(parameters_string_names_, parameters_string_names, j, str_pos);\
		if(parameters_io[j] == 'i'){\
			/* This is an input */\
			if(parameters_types[j] == 'c'){\
				nn_print(&(parameters_string_names[str_pos]), &(FP_CTX_T_GENERIC_IN(j)->p)); \
			}\
			if(parameters_types[j] == 'f'){\
				nn_print(&(parameters_string_names[str_pos]), &(FP_T_GENERIC_IN(j)->fp_val)); \
			}\
			if(parameters_types[j] == 'n'){\
				nn_print(&(parameters_string_names[str_pos]), NN_T_GENERIC_IN(j));\
			}\
			if(parameters_types[j] == 'u'){\
				printf("%16s: 0x", &(parameters_string_names[str_pos])); \
				printf(PRINTF_WORD_HEX_FMT, WORD_T_GENERIC_IN(j));	 \
				printf("\n");						 \
			}\
			if(parameters_types[j] == 's'){\
				printf("%16s:", &(parameters_string_names[str_pos])); 	 \
				printf("%d", INT_GENERIC_IN(j));			 \
				printf("\n");						 \
			}\
		}\
	}\
	/* Then print the outputs */\
	for(j=0; j<sizeof(parameters_types)-1; j++){\
		GET_PARAMETER_PRETTY_NAME(parameters_string_names_, parameters_string_names, j, str_pos);\
		memset(expected_modified_string_names, 0, sizeof(expected_modified_string_names));\
		strcat(expected_modified_string_names, expected);\
		strcat(expected_modified_string_names, &(parameters_string_names[str_pos]));\
		memset(real_modified_string_names, 0, sizeof(real_modified_string_names));\
		strcat(real_modified_string_names, real);\
		strcat(real_modified_string_names, &(parameters_string_names[str_pos]));\
		if(parameters_io[j] == 'o'){\
			/* This is an input that is an output */\
			if(parameters_types[j] == 'f'){\
				nn_print(real_modified_string_names, &(fp_out_ptr[j]->fp_val)); \
				nn_print(expected_modified_string_names, &(FP_T_GENERIC_IN(j)->fp_val)); \
				fp_out_local_cnt++;\
			}\
			if(parameters_types[j] == 'n'){\
				nn_print(real_modified_string_names, nn_out_ptr[j]);\
				nn_print(expected_modified_string_names, NN_T_GENERIC_IN(j));\
				nn_out_local_cnt++;\
			}\
			if(parameters_types[j] == 'u'){\
				printf("%16s: 0x", real_modified_string_names); 	\
				printf(PRINTF_WORD_HEX_FMT, *(WORD_T_GENERIC_OUT(j)));	\
				printf("\n");						\
				printf("%16s: 0x", expected_modified_string_names); 	\
				printf(PRINTF_WORD_HEX_FMT, WORD_T_GENERIC_IN(j));	\
				printf("\n");						\
			}\
			if(parameters_types[j] == 's'){\
				printf("%16s: ", real_modified_string_names); 		\
				printf("%d", *(INT_GENERIC_OUT(j)));			\
				printf("\n");						\
				printf("%16s: ", expected_modified_string_names); 	\
				printf("%d", INT_GENERIC_IN(j));			\
				printf("\n");						\
			}\
		}\
		if((parameters_io[j] == 'O') && (there_is_output == 1)){\
			/* This is a real function output */\
			if(parameters_types[j] == 'u'){\
				printf("%16s: 0x", real_modified_string_names); 	\
				printf(PRINTF_WORD_HEX_FMT, (word_t)fun_output);	\
				printf("\n");						\
				printf("%16s: 0x", expected_modified_string_names); 	\
				printf(PRINTF_WORD_HEX_FMT, WORD_T_GENERIC_IN(j));	\
				printf("\n");						\
			}\
			if(parameters_types[j] == 's'){\
				printf("%16s: ", real_modified_string_names); 		\
				printf("%d", (int)fun_output);				\
				printf("\n");						\
				printf("%16s: ", expected_modified_string_names); 	\
				printf("%d", INT_GENERIC_IN(j));			\
				printf("\n");						\
			}\
		}\
	}\
} while(0)

/* Generic testing framework. Seems ugly but does the job! */
#define GENERIC_TEST(test_name, operation_, given_string_helper, fun_name, parameters_types_, parameters_io_, parameters_string_names, fun_output, nn_out_num, fp_out_num, ...) \
int test_##test_name(const char ATTRIBUTE_UNUSED *op, void **params, int test_num);\
int test_##test_name(const char ATTRIBUTE_UNUSED *op, void **params, int test_num){\
	unsigned int i;\
	int ret = 0, cmp, mismatch = 0;		\
	const char *op_string = NULL;\
	unsigned int n_len ATTRIBUTE_UNUSED = 0;\
	int fun_out_value = 0;\
	u8 there_is_output = 0;\
	unsigned int nn_out_local_cnt = 0, fp_out_local_cnt = 0;\
	fp_ctx_t fp_ctx_param ATTRIBUTE_UNUSED = NULL;\
	int fp_ctx_initialized ATTRIBUTE_UNUSED = 0;\
	\
	const char parameters_types[] = parameters_types_;\
	const char parameters_io[] = parameters_io_;\
	\
	const char operation[] = #operation_;\
	\
	/* Our words used as output of functions */\
	word_t word_out[MAX_PARAMS] ATTRIBUTE_UNUSED = { 0 };\
	int int_out[MAX_PARAMS] ATTRIBUTE_UNUSED = { 0 };\
	\
	assert(memcmp(operation, op, sizeof(operation)) == 0);\
	\
	/* Sanity check: check that the parameters passed from the file are the same as the ones declared in the test */\
	if(memcmp(global_parameters, parameters_types, LOCAL_MIN(MAX_PARAMS, strlen(parameters_types))) != 0){\
		printf("Error: parameters %s given in the test file differ from the test expected parameters (%s)\n", parameters_types, global_parameters);\
		return -1;\
	}\
	\
	PARAMETERS_SANITY_CHECK(test_num, parameters_types, parameters_io);\
	\
	/* If we find an fp or nn, assume its length is the common length. */\
	for(i=0; i<sizeof(parameters_io)-1; i++){\
		if((parameters_io[i] == 'o') && (parameters_types[i] == 'f')){\
			n_len = (FP_T_GENERIC_IN(i))->fp_val.wlen;\
			break;\
		}\
		if((parameters_io[i] == 'o') && (parameters_types[i] == 'n')){\
			n_len = (NN_T_GENERIC_IN(i))->wlen;\
			break;\
		}\
	}\
	for(i=0; i<sizeof(parameters_io)-1; i++){\
		if(parameters_types[i] == 'c'){\
			fp_ctx_param = (FP_CTX_T_GENERIC_IN(i));\
			fp_ctx_initialized = 1;\
			break;\
		}\
	}\
	GENERIC_TEST_NN_DECL_INIT##nn_out_num(nn_out, n_len * WORD_BYTES);\
	assert(fp_out_num == 0 || fp_ctx_initialized != 0);\
	GENERIC_TEST_FP_DECL_INIT##fp_out_num(fp_out, fp_ctx_param);\
	if(ret){\
		goto err;\
	}\
	\
	CHECK_FUN_##fun_output fun_name(__VA_ARGS__);\
	/* Check generic value return is 0 */\
	if(there_is_output == 0){\
		assert(fun_out_value == 0);\
	}\
	\
	/* Check result is what we expect */\
	FIND_HELPER_IN_DISPATCH_TABLE(operation, op_string);\
	assert(op_string != NULL);\
	\
	for(i=0; i<sizeof(parameters_io)-1; i++){\
		if(parameters_io[i] == 'o'){\
			/* We have an input that is an output, check it */\
			if (parameters_types[i] == 'f') {\
				ret = fp_cmp(fp_out_ptr[i], FP_T_GENERIC_IN(i), &cmp); \
				if(ret || cmp){\
					printf("[-] Test %d (%s): result mismatch\n", test_num, op_string);\
					/* Print the expected outputs */\
					PRINT_ALL(parameters_types, parameters_io, params, nn_out_ptr, fp_out_ptr, fun_out_value, there_is_output, parameters_string_names, i);\
					mismatch = 1;\
					break;\
				}\
				fp_out_local_cnt++;\
			}\
			if (parameters_types[i] == 'n') {\
				ret = nn_cmp(nn_out_ptr[i], NN_T_GENERIC_IN(i), &cmp); \
				if(ret || cmp){\
					printf("[-] Test %d (%s): result mismatch\n", test_num, op_string);\
					/* Print the expected outputs */\
					PRINT_ALL(parameters_types, parameters_io, params, nn_out_ptr, fp_out_ptr, fun_out_value, there_is_output, parameters_string_names, i);\
					mismatch = 1;\
					break;\
				}\
				nn_out_local_cnt++;\
			}\
			if (parameters_types[i] == 'u') {\
				if((*(WORD_T_GENERIC_OUT(i))) != WORD_T_GENERIC_IN(i)){\
					printf("[-] Test %d (%s): result mismatch\n", test_num, op_string);\
					/* Print the expected outputs */\
					PRINT_ALL(parameters_types, parameters_io, params, nn_out_ptr, fp_out_ptr, fun_out_value, there_is_output, parameters_string_names, i);\
					mismatch = 1;\
					break;\
				}\
			}\
			if (parameters_types[i] == 's') {\
				if((*(INT_GENERIC_OUT(i))) != INT_GENERIC_IN(i)){\
					printf("[-] Test %d (%s): result mismatch\n", test_num, op_string);\
					/* Print the expected outputs */\
					PRINT_ALL(parameters_types, parameters_io, params, nn_out_ptr, fp_out_ptr, fun_out_value, there_is_output, parameters_string_names, i);\
					mismatch = 1;\
					break;\
				}\
			}\
		}\
		if(parameters_io[i] == 'O'){\
			/* We have a function output, check it */\
			if(fun_out_value != INT_GENERIC_IN(i)){\
				printf("[-] Test %d (%s): result mismatch\n", test_num, op_string);\
				/* Print the expected outputs */\
				PRINT_ALL(parameters_types, parameters_io, params, nn_out_ptr, fp_out_ptr, fun_out_value, there_is_output, parameters_string_names, i);\
				mismatch = 1;\
				break;\
			}\
		}\
	}\
	\
	GENERIC_TEST_nn_uninit##nn_out_num(nn_out);\
	GENERIC_TEST_FP_CLEAR##fp_out_num(fp_out);\
	\
	return !mismatch;\
err:\
	printf("[-] Error: general error when initializing variables ...\n");\
	exit(-1);\
}\
ADD_TO_DISPATCH_TABLE(test_##test_name, #operation_, given_string_helper)

#define GENERIC_TEST_NN(test_name, operation_, given_string_helper, fun_name, parameters_types_, parameters_io_, parameters_string_names, fun_output, nn_out_num, ...) \
GENERIC_TEST(test_name, operation_, given_string_helper, fun_name, parameters_types_, parameters_io_, parameters_string_names, fun_output, nn_out_num, 0, __VA_ARGS__)

#define GENERIC_TEST_FP(test_name, operation_, given_string_helper, fun_name, parameters_types_, parameters_io_, parameters_string_names, fun_output, nn_out_num, ...) \
GENERIC_TEST(test_name, operation_, given_string_helper, fun_name, parameters_types_, parameters_io_, parameters_string_names, fun_output, nn_out_num, __VA_ARGS__)


/* Global variable to keep track of parameters */
static char global_parameters[MAX_PARAMS];

/*********** NN layer tests ************************************************/
/* Testing shifts and rotates */
	GENERIC_TEST_NN(nn_lshift_fixedlen, NN_SHIFT_LEFT_FIXEDLEN, "(fixed)<<", nn_lshift_fixedlen, "nnu", "oii",
		SET_PARAMETER_PRETTY_NAME(3, "output", "input", "fixed lshift"), NO_RET, 1,
		NN_T_GENERIC_OUT(0), NN_T_GENERIC_IN(1), (bitcnt_t)UINT_GENERIC_IN(2))
	GENERIC_TEST_NN(nn_rshift_fixedlen, NN_SHIFT_RIGHT_FIXEDLEN, "(fixed)>>", nn_rshift_fixedlen, "nnu", "oii",
		SET_PARAMETER_PRETTY_NAME(3, "output", "input", "fixed rshift"), NO_RET, 1,
		NN_T_GENERIC_OUT(0), NN_T_GENERIC_IN(1), (bitcnt_t)UINT_GENERIC_IN(2))
	GENERIC_TEST_NN(nn_lshift, NN_SHIFT_LEFT, "<<", nn_lshift, "nnu", "oii",
		SET_PARAMETER_PRETTY_NAME(3, "output", "input", "lshift"), NO_RET, 1,
		NN_T_GENERIC_OUT(0), NN_T_GENERIC_IN(1), (bitcnt_t)UINT_GENERIC_IN(2))
	GENERIC_TEST_NN(nn_rshift, NN_SHIFT_RIGHT, ">>", nn_rshift, "nnu", "oii",
		SET_PARAMETER_PRETTY_NAME(3, "output", "input", "rshift"), NO_RET, 1,
		NN_T_GENERIC_OUT(0), NN_T_GENERIC_IN(1), (bitcnt_t)UINT_GENERIC_IN(2))
	GENERIC_TEST_NN(nn_lrot, NN_ROTATE_LEFT, "lrot", nn_lrot, "nnuu", "oiii",
		SET_PARAMETER_PRETTY_NAME(4, "output", "input", "lrot", "bitlen_base"), NO_RET, 1,
		NN_T_GENERIC_OUT(0), NN_T_GENERIC_IN(1), (bitcnt_t)UINT_GENERIC_IN(2), (bitcnt_t)UINT_GENERIC_IN(3))
	GENERIC_TEST_NN(nn_rrot, NN_ROTATE_RIGHT, "rrot", nn_rrot, "nnuu", "oiii",
		SET_PARAMETER_PRETTY_NAME(4, "output", "input", "rrot", "bitlen_base"), NO_RET, 1,
		NN_T_GENERIC_OUT(0), NN_T_GENERIC_IN(1), (bitcnt_t)UINT_GENERIC_IN(2), (bitcnt_t)UINT_GENERIC_IN(3))


/* Testing xor, or, and, not */
	GENERIC_TEST_NN(nn_xor, NN_XOR, "^", nn_xor, "nnn", "iio",
		SET_PARAMETER_PRETTY_NAME(3, "input1", "input2", "output"), NO_RET, 3,
		NN_T_GENERIC_OUT(2), NN_T_GENERIC_IN(0), NN_T_GENERIC_IN(1))
	GENERIC_TEST_NN(nn_or, NN_OR, "|", nn_or, "nnn", "iio",
		SET_PARAMETER_PRETTY_NAME(3, "input1", "input2", "output"), NO_RET, 3,
		NN_T_GENERIC_OUT(2), NN_T_GENERIC_IN(0), NN_T_GENERIC_IN(1))
	GENERIC_TEST_NN(nn_and, NN_AND, "&", nn_and, "nnn", "iio",
		SET_PARAMETER_PRETTY_NAME(3, "input1", "input2", "output"), NO_RET, 3,
		NN_T_GENERIC_OUT(2), NN_T_GENERIC_IN(0), NN_T_GENERIC_IN(1))
	GENERIC_TEST_NN(nn_not, NN_NOT, "~", nn_not, "nn", "io",
		SET_PARAMETER_PRETTY_NAME(2, "input", "output"), NO_RET, 2,
		NN_T_GENERIC_OUT(1), NN_T_GENERIC_IN(0))

/* Testing add and sub */
	GENERIC_TEST_NN(nn_add, NN_ADD, "+", nn_add, "nnn", "iio",
		SET_PARAMETER_PRETTY_NAME(3, "input1", "input2", "output"),
		NO_RET, 3, NN_T_GENERIC_OUT(2), NN_T_GENERIC_IN(0),
		NN_T_GENERIC_IN(1))
	GENERIC_TEST_NN(nn_sub, NN_SUB, "-", nn_sub, "nnn", "iio",
		SET_PARAMETER_PRETTY_NAME(3, "input1", "input2", "output"),
		NO_RET, 3, NN_T_GENERIC_OUT(2), NN_T_GENERIC_IN(0),
		NN_T_GENERIC_IN(1))

/* Testing inc and dec */
	GENERIC_TEST_NN(nn_inc, NN_INC, "++", nn_inc, "nn", "io",
		SET_PARAMETER_PRETTY_NAME(2, "input", "output"), NO_RET, 2,
		NN_T_GENERIC_OUT(1), NN_T_GENERIC_IN(0))
	GENERIC_TEST_NN(nn_dec, NN_DEC, "--", nn_dec, "nn", "io",
		SET_PARAMETER_PRETTY_NAME(2, "input", "output"), NO_RET, 2,
		NN_T_GENERIC_OUT(1), NN_T_GENERIC_IN(0))

/* Testing modular add, sub, inc, dec, mul, exp (inputs are supposed < p except for exp) */
	GENERIC_TEST_NN(nn_mod_add, NN_MOD_ADD, "+%", nn_mod_add, "nnnn", "iiio",
		SET_PARAMETER_PRETTY_NAME(4, "input1", "input2", "modulo", "output"),
		NO_RET, 4, NN_T_GENERIC_OUT(3), NN_T_GENERIC_IN(0),
		NN_T_GENERIC_IN(1), NN_T_GENERIC_IN(2))
	GENERIC_TEST_NN(nn_mod_sub, NN_MOD_SUB, "-%", nn_mod_sub, "nnnn", "iiio",
		SET_PARAMETER_PRETTY_NAME(4, "input1", "input2", "modulo", "output"),
		NO_RET, 4, NN_T_GENERIC_OUT(3), NN_T_GENERIC_IN(0),
		NN_T_GENERIC_IN(1), NN_T_GENERIC_IN(2))
	GENERIC_TEST_NN(nn_mod_inc, NN_MOD_INC, "++%", nn_mod_inc, "nnn", "iio",
		SET_PARAMETER_PRETTY_NAME(3, "input1", "modulo", "output"),
		NO_RET, 3, NN_T_GENERIC_OUT(2), NN_T_GENERIC_IN(0),
		NN_T_GENERIC_IN(1))
	GENERIC_TEST_NN(nn_mod_dec, NN_MOD_DEC, "--%", nn_mod_dec, "nnn", "iio",
		SET_PARAMETER_PRETTY_NAME(3, "input1", "modulo", "output"),
		NO_RET, 3, NN_T_GENERIC_OUT(2), NN_T_GENERIC_IN(0),
		NN_T_GENERIC_IN(1))
	GENERIC_TEST_NN(nn_mod_mul, NN_MOD_MUL, "*%", nn_mod_mul, "nnnn", "iiio",
		SET_PARAMETER_PRETTY_NAME(4, "input1", "input2", "modulo", "output"),
		NO_RET, 4, NN_T_GENERIC_OUT(3), NN_T_GENERIC_IN(0),
		NN_T_GENERIC_IN(1), NN_T_GENERIC_IN(2))
	GENERIC_TEST_NN(nn_mod_pow, NN_MOD_POW, "exp%", nn_mod_pow, "nnnn", "iiio",
		SET_PARAMETER_PRETTY_NAME(4, "base", "exp", "modulo", "output"),
		NO_RET, 4, NN_T_GENERIC_OUT(3), NN_T_GENERIC_IN(0),
		NN_T_GENERIC_IN(1), NN_T_GENERIC_IN(2))


/* Testing mul */
	GENERIC_TEST_NN(nn_mul, NN_MUL, "*", nn_mul, "nnn", "oii",
		SET_PARAMETER_PRETTY_NAME(3, "output1", "input1", "input2"),
		NO_RET, 1, NN_T_GENERIC_OUT(0), NN_T_GENERIC_IN(1),
		NN_T_GENERIC_IN(2))
	GENERIC_TEST_NN(nn_sqr, NN_SQR, "(^2)", nn_sqr, "nn", "oi",
		SET_PARAMETER_PRETTY_NAME(2, "output1", "input1"),
		NO_RET, 1, NN_T_GENERIC_OUT(0), NN_T_GENERIC_IN(1))

/* Testing division */
	GENERIC_TEST_NN(nn_divrem, NN_DIVREM, "/", nn_divrem, "nnnn", "ooii",
		SET_PARAMETER_PRETTY_NAME(4, "quotient", "remainder", "input1", "input2"),
		NO_RET, 2, NN_T_GENERIC_OUT(0), NN_T_GENERIC_OUT(1),
		NN_T_GENERIC_IN(2), NN_T_GENERIC_IN(3))
	GENERIC_TEST_NN(nn_xgcd, NN_XGCD, "xgcd", nn_xgcd, "nnnnns", "oooiio",
		SET_PARAMETER_PRETTY_NAME(6, "xgcd", "u", "v", "input1", "input2", "sign"),
		NO_RET, 3, NN_T_GENERIC_OUT(0), NN_T_GENERIC_OUT(1), NN_T_GENERIC_OUT(2),
		NN_T_GENERIC_IN(3), NN_T_GENERIC_IN(4), INT_GENERIC_OUT(5))
	GENERIC_TEST_NN(nn_gcd, NN_GCD, "gcd", nn_gcd, "nnns", "oiio",
		SET_PARAMETER_PRETTY_NAME(4, "gcd", "input1", "input2", "sign"),
		NO_RET, 1, NN_T_GENERIC_OUT(0),
		NN_T_GENERIC_IN(1), NN_T_GENERIC_IN(2), INT_GENERIC_OUT(3))
	GENERIC_TEST_NN(nn_mod, NN_MOD, "%", nn_mod, "nnn", "oii",
		SET_PARAMETER_PRETTY_NAME(3, "output", "input1", "input2"),
		NO_RET, 1, NN_T_GENERIC_OUT(0),
		NN_T_GENERIC_IN(1), NN_T_GENERIC_IN(2))

/* Testing modular inversion */
	GENERIC_TEST_NN(nn_modinv, NN_MODINV, "(^-1%)", nn_modinv, "nnns", "oiiO",
		SET_PARAMETER_PRETTY_NAME(4, "output", "input1", "input2", "ret"),
		RET, 1, NN_T_GENERIC_OUT(0),
		NN_T_GENERIC_IN(1), NN_T_GENERIC_IN(2))

/* Testing modular inversion modulo a 2**n */
	GENERIC_TEST_NN(nn_modinv_2exp, NN_MODINV_2EXP, "(^-1%)(2exp)", nn_modinv_2exp, "nnus", "oiio",
		SET_PARAMETER_PRETTY_NAME(4, "output", "input1", "input2", "isodd"),
		NO_RET, 1, NN_T_GENERIC_OUT(0), NN_T_GENERIC_IN(1),
			UINT_GENERIC_IN(2), INT_GENERIC_OUT(3))

/* Check Montgomery multiplication redcify primitives */
	GENERIC_TEST_NN(nn_compute_redc1_coefs, NN_COEF_REDC1, "coef_redc1", nn_compute_redc1_coefs, "nnnu", "ooio",
		SET_PARAMETER_PRETTY_NAME(4, "r", "r_square", "p", "mpinv"),
		NO_RET, 3, NN_T_GENERIC_OUT(0), NN_T_GENERIC_OUT(1), NN_T_GENERIC_IN(2), WORD_T_GENERIC_OUT(3))
	GENERIC_TEST_NN(nn_compute_div_coefs, NN_COEF_DIV, "coef_div", nn_compute_div_coefs, "nuun", "oooi",
		SET_PARAMETER_PRETTY_NAME(4, "p_normalized", "p_shift", "p_reciprocal", "p"),
		NO_RET, 3, NN_T_GENERIC_OUT(0), WORD_T_GENERIC_OUT(1), WORD_T_GENERIC_OUT(2), NN_T_GENERIC_IN(3))
	GENERIC_TEST_NN(nn_mul_redc1, NN_MUL_REDC1, "*_redc1", nn_mul_redc1, "nnnnu", "oiiii",
		SET_PARAMETER_PRETTY_NAME(5, "output", "input1", "input2", "p", "mpinv"),
		NO_RET, 1, NN_T_GENERIC_OUT(0), NN_T_GENERIC_IN(1), NN_T_GENERIC_IN(2),
		NN_T_GENERIC_IN(3), WORD_T_GENERIC_IN(4))



/*********** Fp layer tests ************************************************/
/* Testing addition in F_p */
	GENERIC_TEST_FP(fp_add, FP_ADD, "+", fp_add, "cfff", "ioii",
	     SET_PARAMETER_PRETTY_NAME(4, "p", "sum", "input1", "input2"),
	     NO_RET, 0, 2,
	     FP_T_GENERIC_OUT(1), FP_T_GENERIC_IN(2), FP_T_GENERIC_IN(3))

/* Testing subtraction in F_p */
	GENERIC_TEST_FP(fp_sub, FP_SUB, "-", fp_sub, "cfff", "ioii",
	     SET_PARAMETER_PRETTY_NAME(4, "p", "diff", "input1", "input2"),
	     NO_RET, 0, 2,
	     FP_T_GENERIC_OUT(1), FP_T_GENERIC_IN(2), FP_T_GENERIC_IN(3))

/* Testing multiplication in F_p */
	GENERIC_TEST_FP(fp_mul, FP_MUL, "*", fp_mul, "cfff", "ioii",
	     SET_PARAMETER_PRETTY_NAME(4, "p", "prod", "input1", "input2"),
	     NO_RET, 0, 2,
	     FP_T_GENERIC_OUT(1), FP_T_GENERIC_IN(2), FP_T_GENERIC_IN(3))
	GENERIC_TEST_FP(fp_sqr, FP_SQR, "(^2)", fp_sqr, "cff", "ioi",
	     SET_PARAMETER_PRETTY_NAME(3, "p", "prod", "input1"),
	     NO_RET, 0, 2,
	     FP_T_GENERIC_OUT(1), FP_T_GENERIC_IN(2))

/* Testing division in F_p */
	GENERIC_TEST_FP(fp_div, FP_DIV, "/", fp_div, "cfff", "ioii",
	     SET_PARAMETER_PRETTY_NAME(4, "p", "quo", "input1", "input2"),
	     NO_RET, 0, 2,
	     FP_T_GENERIC_OUT(1), FP_T_GENERIC_IN(2), FP_T_GENERIC_IN(3))

/* Testing Montgomery multiplication in F_p */
	GENERIC_TEST_FP(fp_mul_monty, FP_MUL_MONTY, "*_monty", fp_mul_monty, "cfff", "ioii",
	     SET_PARAMETER_PRETTY_NAME(4, "p", "prod", "input1", "input2"),
	     NO_RET, 0, 2,
	     FP_T_GENERIC_OUT(1), FP_T_GENERIC_IN(2), FP_T_GENERIC_IN(3))
	GENERIC_TEST_FP(fp_sqr_monty, FP_SQR_MONTY, "(^2)_monty", fp_sqr_monty, "cff", "ioi",
	     SET_PARAMETER_PRETTY_NAME(3, "p", "prod", "input1"),
	     NO_RET, 0, 2,
	     FP_T_GENERIC_OUT(1), FP_T_GENERIC_IN(2))

/* Testing exponentiation in F_p */
	GENERIC_TEST_FP(fp_pow, FP_POW, "exp", fp_pow, "cffn", "ioii",
	     SET_PARAMETER_PRETTY_NAME(4, "p", "pow", "input", "exp"),
	     NO_RET, 0, 2,
	     FP_T_GENERIC_OUT(1), FP_T_GENERIC_IN(2), NN_T_GENERIC_IN(3))

/* Testing square residue in F_p */
	GENERIC_TEST_FP(fp_sqrt, FP_SQRT, "sqrt", fp_sqrt, "cfffs", "iooiO",
	     SET_PARAMETER_PRETTY_NAME(4, "sqrt1", "sqrt2", "p", "ret"),
	     RET, 0, 3,
	     FP_T_GENERIC_OUT(1), FP_T_GENERIC_OUT(2), FP_T_GENERIC_IN(3))

/*****************************************************************/

/*
 * Read data on given fd until first newline character and put it in buf
 * followed by a null character. buffer size is passed via buflen. The
 * length of read line is returned to the caller in buflen on success
 * (not including null character terminating read string).
 *
 *  0 is returned on success.
 * -1 is returned on end of file.
 * -2 is returned on error (buffer not sufficient, etc)
 */
int read_string(int fd, char *buf, unsigned int *buflen);
int read_string(int fd, char *buf, unsigned int *buflen)
{
	unsigned int pos = 0, len;
	int ret = -1;
	char c;

	MUST_HAVE((buf != NULL) && (buflen != NULL), ret, err);

	len = *buflen;

	if (len < 2) {
		ret = -2;
		goto err;
	}

	len -= 1;		/* keep some space to terminate the string */

	while ((len > 0) && ((ret = read(fd, &c, 1)) != 0) && (c != '\n')) {
		buf[pos++] = c;
		len -= 1;
	}

	if (len == 0) {
		ret = -2;
		goto err;
	}

	if (!ret) {
		ret = -1;
		goto err;
	}

	/* Terminate the string */
	buf[pos] = 0;
	*buflen = pos;
	ret = 0;

err:
	return ret;
}


/*
 * Parse a test file and perform the tests it provides, one
 * by one, in order.
 */
int main(int argc, char *argv[])
{
	nn fp_ctx_modulus, fp_ctx_r, fp_ctx_r_square, fp_ctx_mpinv;
	nn fp_ctx_pshift, fp_ctx_pnorm, fp_ctx_prec;
	fp_ctx fp_ctx_param;
	int ret, cmp;
	u64 u_params[MAX_PARAMS];
	void *params[MAX_PARAMS];
	unsigned int ibuflen = BIT_LEN_WORDS(NN_MAX_BIT_LEN) * WORD_BYTES * 10;
	unsigned long int test_num, line = 0, oktests = 0;
	int test_ret;
	unsigned int len = ibuflen;
	int nrecs;
	int fd = 0, nn_local_cnt = 0, fp_local_cnt = 0, fp_ctx_local_cnt = 0;
	unsigned int nn_len;
	char op[1024];
	char *ibuf = NULL, *rec = NULL;
	nn *tmp;
	fp *fp_tmp;
	int (*curr_test_fun) (const char *, void **, int);
	unsigned long p_tmp;

	ret = nn_init(&fp_ctx_modulus, 0);
	ret |= nn_init(&fp_ctx_r, 0);
	ret |= nn_init(&fp_ctx_r_square, 0);
	ret |= nn_init(&fp_ctx_mpinv, 0);
	ret |= nn_init(&fp_ctx_pshift, 0);
	ret |= nn_init(&fp_ctx_pnorm, 0);
	ret |= nn_init(&fp_ctx_prec, 0);

	/* First "fake" context initialization with junk value
	 * one as prime number
	 */
	ret |= nn_one(&fp_ctx_modulus);
	ret |= fp_ctx_init_from_p(&fp_ctx_param, &fp_ctx_modulus);
	GENERIC_TEST_FP_DECL_INIT_MAX(fp_params, &fp_ctx_param)
	GENERIC_TEST_NN_DECL_INIT_MAX(nn_params, 0)

	if(ret){
		goto err;
	}

#ifdef WITH_ASSERT_BACKTRACE
	memset(backtrace_buffer, 0, sizeof(backtrace_buffer) - 1);
	if (signal(SIGINT, assert_signal_handler) == SIG_ERR) {
		printf("Error: can't catch SIGINT signal ...\n");
		return -1;
	}
#endif

	if (argc > 2) {
		printf("Usage: %s [test_file]\n", argv[0]);
		printf("       If no test_file provided, stdin is taken\n");
		return -1;
	}

	/* Special case where we want to dump information */
	if (argc == 2) {
		if (memcmp(argv[1], "-info", sizeof("-info")) == 0){
			printf("%d %d\n", WORDSIZE, NN_MAX_BASE);
			return 0;
		}
	}

	ibuf = (char*)malloc(ibuflen);
	if (!ibuf) {
		return -1;
	}
	memset(ibuf, 0, ibuflen);

	if(argc == 2){
		fd = open(argv[1], O_RDONLY);
	}
	else{
		fd = STDIN_FILENO;
	}
	while (read_string(fd, ibuf, &len) == 0) {
		char *t, *s = ibuf;
		int i;

		/* Find end of first record (the test number) */
		t = strchr(s, ' ');
		if (t == NULL) {
			printf("\nLine %lu: unable to find record #1\n", line);
			return -1;
		}
		*t = 0;		/* mark end of record */
		test_num = strtoul(s, NULL, 10);
		assert(line == test_num);
		s = t + 1;	/* jump to beginning of next record */

		/* Find end of second record (operation type) */
		t = strchr(s, ' ');
		if (t == NULL) {
			printf("\nLine %lu: unable to find record #2\n", line);
			return -1;
		}
		*t = 0;		/* mark end of record */
		strncpy(op, s, sizeof(op) - 1);	/* Copy opcode */
		s = t + 1;	/* jump to beginning of next record */

		/* Pretty print the evolution of our tests */
		if((line % 1000 == 0) && (line != 0)){
			printf("\r%*s", 40, "");
			printf("\rTest %lu on the go [%s]", line, op);
			fflush(stdout);
		}

		/* Find end of third record (str of types for next records) */
		t = strchr(s, ' ');
		if (t == NULL) {
			printf("\nLine %lu: unable to find record #3\n", line);
			return -1;
		}
		*t = 0;		/* mark end of record */
		nrecs = (int)(t - s);

		rec = t + 1;
		ADD_TO_BACKTRACE("--------------\n");
		for (i = 0; i < nrecs; i++) {
			/* Find end of record */
			t = strchr(rec, ' ');
			if (t == NULL) {
				t = ibuf + len;
			}
			*t = 0;
			switch (s[i]) {
			case 'c':	/* fp_ctx */
				if (fp_ctx_local_cnt > 0) {
					printf("\nLine %lu: Only one fp_ctx allowed\n", line);
					ret = -1;
					goto err;
				}
				/*
				 * We expect a 3 nn of the same size (p, r, r^2)
				 * followed by a single word providing mpinv
				 * and an additional nn and two words.
				 */
				assert(((t - rec) % 2) == 0);
				nn_len = (unsigned int)(t - rec -
							3 * (WORD_BYTES * 2)) /
					(2 * 4);
				assert((nn_len % WORD_BYTES) == 0);
				fp_ctx_local_cnt++;
				tmp = &fp_ctx_modulus;
				ret = nn_set_wlen(tmp, (u8)(nn_len / WORD_BYTES)); EG(ret, err);
				ret = nn_import_from_hexbuf(tmp, rec, 2 * nn_len); EG(ret, err);

				/* Initialize fp context from the prime modulus */
				ret = fp_ctx_init_from_p(&fp_ctx_param, &fp_ctx_modulus); EG(ret, err);
				/* Now get the other Fp context values and check that
				 * everything is OK
				 */
				tmp = &fp_ctx_r;
				ret = nn_set_wlen(tmp, (u8)(nn_len / WORD_BYTES)); EG(ret, err);
				ret = nn_import_from_hexbuf(tmp, rec + (2 * nn_len),
						      2 * nn_len); EG(ret, err);

				/* Compare r */
				ret = nn_cmp(&fp_ctx_r, &(fp_ctx_param.r), &cmp);
				if(ret || cmp){
					printf("\nLine %lu: Fp context import failed\n", line);
					nn_print("Imported r from file   =", &fp_ctx_r);
					nn_print("Computed r from modulus=", &(fp_ctx_param.r));
					ret = -1;
					goto err;
				}
				tmp = &fp_ctx_r_square;
				ret = nn_set_wlen(tmp, (u8)(nn_len / WORD_BYTES)); EG(ret, err);
				ret = nn_import_from_hexbuf(tmp, rec + (4 * nn_len),
						      2 * nn_len); EG(ret, err);

				/* Compare r_square */
				ret = nn_cmp(&fp_ctx_r_square, &(fp_ctx_param.r_square), &cmp);
				if(ret || cmp){
					printf("\nLine %lu: Fp context import failed\n", line);
					nn_print("Imported r_square from file   =", &fp_ctx_r_square);
					nn_print("Computed r_square from modulus=", &(fp_ctx_param.r_square));
					ret = -1;
					goto err;
				}
				tmp = &fp_ctx_mpinv;
				ret = nn_set_wlen(tmp, 1); EG(ret, err);
				ret = nn_import_from_hexbuf(tmp, rec + (6 * nn_len),
						      WORD_BYTES * 2); EG(ret, err);

				/* Compare mpinv */
				if(fp_ctx_mpinv.val[0] != fp_ctx_param.mpinv){
					printf("\nLine %lu: Fp context import failed\n", line);
					printf("Imported mpinv from modulus=" PRINTF_WORD_HEX_FMT, fp_ctx_mpinv.val[0]);
					printf("Computed mpiv  from file   =" PRINTF_WORD_HEX_FMT, fp_ctx_param.mpinv);
					ret = -1;
					goto err;
				}
				tmp = &fp_ctx_pshift;
				ret = nn_set_wlen(tmp, 1); EG(ret, err);
				ret = nn_import_from_hexbuf(tmp, rec + (6 * nn_len + 2 * WORD_BYTES),
						      WORD_BYTES * 2); EG(ret, err);

				/* Compare p_shift */
				if((bitcnt_t)fp_ctx_pshift.val[0] != fp_ctx_param.p_shift){
					printf("\nLine %lu: Fp context import failed\n", line);
					printf("Imported mpinv from modulus=%d", (bitcnt_t)fp_ctx_pshift.val[0]);
					printf("Computed mpiv  from file   =%d", fp_ctx_param.p_shift);
					ret = -1;
					goto err;
				}
				tmp = &fp_ctx_pnorm;
				ret = nn_set_wlen(tmp, (u8)(nn_len / WORD_BYTES)); EG(ret, err);
				ret = nn_import_from_hexbuf(tmp, rec + (6 * nn_len + 4 * WORD_BYTES),
						      nn_len * 2); EG(ret, err);

				/* Compare p_normalized */
				ret = nn_cmp(&fp_ctx_pnorm, &(fp_ctx_param.p_normalized), &cmp);
				if(ret || (cmp != 0)){
					printf("\nLine %lu: Fp context import failed\n", line);
					nn_print("Imported r_square from file   =", &fp_ctx_pnorm);
					nn_print("Computed r_square from modulus=", &(fp_ctx_param.p_normalized));
					return -1;
				}
				tmp = &fp_ctx_prec;
				ret = nn_set_wlen(tmp, 1); EG(ret, err);
				ret = nn_import_from_hexbuf(tmp, rec + (8 * nn_len + 4 * WORD_BYTES),
						      WORD_BYTES * 2); EG(ret, err);

				/* Compare p_reciprocal */
				if(fp_ctx_prec.val[0] != fp_ctx_param.p_reciprocal){
					printf("\nLine %lu: Fp context import failed\n", line);
					printf("Imported mpinv from modulus=" PRINTF_WORD_HEX_FMT, fp_ctx_prec.val[0]);
					printf("Computed mpiv  from file   =" PRINTF_WORD_HEX_FMT, fp_ctx_param.p_reciprocal);
					ret = -1;
					goto err;
				}
				params[i] = &fp_ctx_param;
				ADD_TO_BACKTRACE("'c' param: %s\n", rec);
				break;
			case 'f':	/* fp */
				if (fp_ctx_local_cnt != 1) {
					printf("\nLine %lu: No fp_ctx available\n", line);
					ret = -1;
					goto err;
				}
				if (fp_local_cnt >= NUM_PRE_ALLOCATED_FP) {
					printf("\nLine %lu: Not enough fp\n",
					       line);
					ret = -1;
					goto err;
				}
				assert(((t - rec) % 2) == 0);
				nn_len = (unsigned int)(t - rec) / 2;
				assert((nn_len / WORD_BYTES) <=
				       fp_ctx_param.p.wlen);
				fp_tmp = fp_params_ptr[fp_local_cnt++];
				fp_tmp->ctx = &fp_ctx_param;
				tmp = &(fp_tmp->fp_val);
				ret = nn_set_wlen(tmp, (u8)(nn_len / WORD_BYTES)); EG(ret, err);
				ret = nn_import_from_hexbuf(tmp, rec, 2 * nn_len); EG(ret, err);
				ret = nn_set_wlen(tmp, fp_ctx_param.p.wlen); EG(ret, err);
				params[i] = fp_tmp;
				ADD_TO_BACKTRACE("'f' param: %s\n", rec);
				break;
			case 'p':	/* raw pointer value. Useful for NULL */
				p_tmp = strtoull(rec, NULL, 10);
				params[i] = (void *)p_tmp;
				ADD_TO_BACKTRACE("'p' param: %s\n", rec);
				/* If this is not a NULL pointer, this is weird!
				 * Abort ...
				 */
				if(params[i] != NULL){
					printf("\nLine %lu: imported a pointer (type 'p') non NULL\n",
					       line);
					ret = -1;
					goto err;
				}
				break;
			case 'n':	/* nn */
				if (nn_local_cnt >= NUM_PRE_ALLOCATED_NN) {
					printf("\nLine %lu: Not enough nn\n",
					       line);
					return -1;
				}
				assert(((t - rec) % 2) == 0);
				nn_len = (unsigned int)(t - rec) / 2;
				assert((nn_len % WORD_BYTES) == 0);
				tmp = nn_params_ptr[nn_local_cnt++];
				ret = nn_set_wlen(tmp, (u8)(nn_len / WORD_BYTES)); EG(ret, err);
				ret = nn_import_from_hexbuf(tmp, rec, 2 * nn_len); EG(ret, err);
				params[i] = tmp;
				ADD_TO_BACKTRACE("'n' param: %s\n", rec);
				break;
			case 'u':	/* unsigned long int (in base 10) */
				u_params[i] = (u64)strtoull(rec, NULL, 10);
				params[i] = &u_params[i];
				ADD_TO_BACKTRACE("'u' param: %s\n", rec);
				break;
			case 's':	/* signed long int (in base 10) */
				u_params[i] = (u64)strtoll(rec, NULL, 10);
				params[i] = &u_params[i];
				ADD_TO_BACKTRACE("'s' param: %s\n", rec);
				break;
			default:
				printf("\nUnknown record type '%c'\n", s[i]);
				ret = -1;
				goto err;
			}
			rec = t + 1;
		}
		/* Save current parameters format in the global variable */
		memcpy(global_parameters, s, LOCAL_MIN(nrecs, MAX_PARAMS));
		curr_test_fun = NULL;
		FIND_FUN_IN_DISPATCH_TABLE(op, curr_test_fun);
		if (curr_test_fun == NULL) {
			printf("\nLine %lu: unknown opcode %s\n", line, op);
		} else {
			ADD_TO_BACKTRACE("\nLine %lu: testing opcode %s\n", line, op);
			test_ret = curr_test_fun(op, params, (int)test_num);
			if (test_ret == 1) {
				ADD_TO_BACKTRACE("-- TEST OK ---\n");
				oktests += (unsigned long)test_ret;
			} else {
				ADD_TO_BACKTRACE("-- TEST NOK --\n");
			}
		}
		line += 1;
		len = ibuflen;
		nn_local_cnt = 0;
		fp_local_cnt = 0;
		fp_ctx_local_cnt = 0;
	}

	printf("\n%lu/%lu tests passed successfully (%lu on error)\n",
	       oktests, line, line - oktests);

	if(fd != 0){
		close(fd);
	}
	if(ibuf != NULL){
		free(ibuf);
	}

	return 0;
err:
	printf("Error: critical error occured! Leaving ...\n");
	if(fd != 0){
		close(fd);
	}
	if(ibuf != NULL){
		free(ibuf);
	}
	return -1;
}
