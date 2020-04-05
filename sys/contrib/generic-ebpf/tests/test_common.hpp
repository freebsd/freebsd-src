#pragma once

extern "C" {
#include <sys/ebpf.h>
}

enum test_emts {
	EBPF_MAP_TYPE_ARRAY,
	EBPF_MAP_TYPE_PERCPU_ARRAY,
	EBPF_MAP_TYPE_HASHTABLE,
	EBPF_MAP_TYPE_PERCPU_HASHTABLE,
	EBPF_MAP_TYPE_MAX
};

enum test_epts {
	EBPF_PROG_TYPE_TEST,
	EBPF_PROG_TYPE_MAX
};

enum test_ehts {
	EBPF_HELPER_TYPE_map_lookup_elem,
	EBPF_HELPER_TYPE_map_update_elem,
	EBPF_HELPER_TYPE_map_delete_elem,
	EBPF_HELPER_TYPE_MAX
};

static bool
test_is_map_usable(struct ebpf_map_type *emt)
{
	if (emt == &emt_array) return true;
	if (emt == &emt_percpu_array) return true;
	if (emt == &emt_hashtable) return true;
	if (emt == &emt_percpu_hashtable) return true;
	return false;
}

static bool
test_is_helper_usable(struct ebpf_helper_type *eht)
{
	if (eht == &eht_map_lookup_elem) return true;
	if (eht == &eht_map_update_elem) return true;
	if (eht == &eht_map_delete_elem) return true;
	return false;
}

static const struct ebpf_prog_type ept_test = {
	"test",
	{
		test_is_map_usable,
		test_is_helper_usable
	}
};

static const struct ebpf_preprocessor_type eppt_test = {
	"test",
	{ NULL }
};

static const struct ebpf_config ebpf_test_config = {
	.prog_types = {
		[EBPF_PROG_TYPE_TEST] = &ept_test
	},
	.map_types = {
		[EBPF_MAP_TYPE_ARRAY] = &emt_array,
		[EBPF_MAP_TYPE_PERCPU_ARRAY] = &emt_percpu_array,
		[EBPF_MAP_TYPE_HASHTABLE] = &emt_hashtable,
		[EBPF_MAP_TYPE_PERCPU_HASHTABLE] = &emt_percpu_hashtable
	},
	.helper_types = {
		[EBPF_HELPER_TYPE_map_lookup_elem] = &eht_map_lookup_elem,
		[EBPF_HELPER_TYPE_map_update_elem] = &eht_map_update_elem,
		[EBPF_HELPER_TYPE_map_delete_elem] = &eht_map_delete_elem
	},
	.preprocessor_type = &eppt_test
};

class CommonFixture : public ::testing::Test {
protected:
  struct ebpf_env *ee;

  virtual void SetUp() {
    int error;

    error = ebpf_env_create(&ee, &ebpf_test_config);
    EXPECT_EQ(error, 0);
  }

  virtual void TearDown() {
    int error;

    error = ebpf_env_destroy(ee);
    EXPECT_EQ(error, 0);
  }
};
