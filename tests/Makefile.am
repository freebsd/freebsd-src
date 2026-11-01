test_scripts = basic.test \
		generate.test \
		variables.test \
		empty_chunks.test \
		schema.test \
		msgpack.test \
		msgpack_malformed.test \
		csexp.test \
		cbor.test \
		cbor_vectors.test \
		speed.test \
		roundtrip.test

EXTRA_DIST = $(test_scripts) streamline.test basic schema generate.res \
	streamline.res rcl_test.json.xz

TESTS = $(test_scripts) \
		test_schema_refs \
		test_limits \
		test_strnstr \
		test_size_overflow \
		test_copy_failure \
		test_object_free \
		test_dup_error_key \
		test_special_handlers

TESTS_ENVIRONMENT = $(SH) \
			TEST_DIR=$(top_srcdir)/tests \
			TEST_OUT_DIR=$(top_builddir)/tests \
			TEST_BINARY_DIR=$(top_builddir)/tests

common_test_cflags = -I$(top_srcdir)/include \
					-I$(top_srcdir)/src \
					-I$(top_srcdir)/uthash
common_test_ldadd = $(top_builddir)/src/libucl.la

test_basic_SOURCES = test_basic.c
test_basic_LDADD = $(common_test_ldadd)
test_basic_CFLAGS = $(common_test_cflags)

test_speed_SOURCES = test_speed.c
test_speed_LDADD = $(common_test_ldadd)
test_speed_CFLAGS = $(common_test_cflags)

test_generate_SOURCES = test_generate.c
test_generate_LDADD = $(common_test_ldadd)
test_generate_CFLAGS = $(common_test_cflags)

test_variables_SOURCES = test_variables.c
test_variables_LDADD = $(common_test_ldadd)
test_variables_CFLAGS = $(common_test_cflags)

test_empty_chunks_SOURCES = test_empty_chunks.c
test_empty_chunks_LDADD = $(common_test_ldadd)
test_empty_chunks_CFLAGS = $(common_test_cflags)

test_schema_SOURCES = test_schema.c
test_schema_LDADD = $(common_test_ldadd)
test_schema_CFLAGS = $(common_test_cflags)

test_schema_refs_SOURCES = test_schema_refs.c
test_schema_refs_LDADD = $(common_test_ldadd)
test_schema_refs_CFLAGS = $(common_test_cflags)

test_streamline_SOURCES = test_streamline.c
test_streamline_LDADD = $(common_test_ldadd)
test_streamline_CFLAGS = $(common_test_cflags)

test_msgpack_SOURCES = test_msgpack.c
test_msgpack_LDADD = $(common_test_ldadd)
test_msgpack_CFLAGS = $(common_test_cflags)

test_msgpack_malformed_SOURCES = test_msgpack_malformed.c
test_msgpack_malformed_LDADD = $(common_test_ldadd)
test_msgpack_malformed_CFLAGS = $(common_test_cflags)

test_csexp_SOURCES = test_csexp.c
test_csexp_LDADD = $(common_test_ldadd)
test_csexp_CFLAGS = $(common_test_cflags)

test_cbor_SOURCES = test_cbor.c
test_cbor_LDADD = $(common_test_ldadd)
test_cbor_CFLAGS = $(common_test_cflags)

test_cbor_vectors_SOURCES = test_cbor_vectors.c
test_cbor_vectors_LDADD = $(common_test_ldadd)
test_cbor_vectors_CFLAGS = $(common_test_cflags)

test_roundtrip_SOURCES = test_roundtrip.c
test_roundtrip_LDADD = $(common_test_ldadd)
test_roundtrip_CFLAGS = $(common_test_cflags)

check_PROGRAMS = test_basic test_speed test_generate test_variables test_empty_chunks test_schema \
		 test_schema_refs test_limits \
		 test_streamline test_msgpack test_msgpack_malformed test_csexp test_cbor test_cbor_vectors \
		 test_roundtrip test_strnstr test_size_overflow test_copy_failure test_object_free \
		 test_dup_error_key test_special_handlers

test_limits_SOURCES = test_limits.c
test_limits_LDADD = $(common_test_ldadd)
test_limits_CFLAGS = $(common_test_cflags)

test_strnstr_SOURCES = test_strnstr.c
test_strnstr_LDADD = $(common_test_ldadd)
test_strnstr_CFLAGS = $(common_test_cflags)

test_size_overflow_SOURCES = test_size_overflow.c
test_size_overflow_LDADD = $(common_test_ldadd)
test_size_overflow_CFLAGS = $(common_test_cflags) -I$(top_srcdir)/klib

test_object_free_SOURCES = test_object_free.c
test_object_free_LDADD = $(common_test_ldadd)
test_object_free_CFLAGS = $(common_test_cflags)

test_copy_failure_SOURCES = test_copy_failure.c
test_copy_failure_LDADD = $(common_test_ldadd)
test_copy_failure_CFLAGS = $(common_test_cflags) -I$(top_srcdir)/klib

test_dup_error_key_SOURCES = test_dup_error_key.c
test_dup_error_key_LDADD = $(common_test_ldadd)
test_dup_error_key_CFLAGS = $(common_test_cflags)

test_special_handlers_SOURCES = test_special_handlers.c
test_special_handlers_LDADD = $(common_test_ldadd)
# Includes ucl_parser.c, which libucl itself builds with -Wno-pointer-sign
test_special_handlers_CFLAGS = $(common_test_cflags) -I$(top_srcdir)/klib \
					-Wno-pointer-sign
