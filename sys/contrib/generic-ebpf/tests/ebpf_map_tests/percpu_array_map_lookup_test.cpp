#include <gtest/gtest.h>

extern "C" {
#include <errno.h>
#include <stdint.h>
#include <sys/ebpf.h>
#include <dev/ebpf/ebpf_platform.h>

#include "../test_common.hpp"
}

namespace {
class PercpuArrayMapLookupTest : public CommonFixture {
 protected:
  struct ebpf_map *em;

  virtual void SetUp() {
    int error;
    uint32_t gkey = 50;
    uint64_t gval = 100;

    CommonFixture::SetUp();

    struct ebpf_map_attr attr;
    attr.type = EBPF_MAP_TYPE_PERCPU_ARRAY;
    attr.key_size = sizeof(uint32_t);
    attr.value_size = sizeof(uint64_t);
    attr.max_entries = 100;
    attr.flags = 0;

    error = ebpf_map_create(ee, &em, &attr);
    ASSERT_TRUE(!error);

    error = ebpf_map_update_elem_from_user(em, &gkey, &gval, 0);
    ASSERT_TRUE(!error);
  }

  virtual void TearDown() {
    ebpf_map_destroy(em);
    CommonFixture::TearDown();
  }
};

TEST_F(PercpuArrayMapLookupTest, LookupMaxEntryPlusOne) {
  int error;
  uint32_t key = 100;
  uint64_t value;

  error = ebpf_map_lookup_elem_from_user(em, &key, &value);

  EXPECT_EQ(EINVAL, error);
}

TEST_F(PercpuArrayMapLookupTest, LookupOutOfMaxEntry) {
  int error;
  uint32_t key = 102;
  uint64_t value;

  error = ebpf_map_lookup_elem_from_user(em, &key, &value);

  EXPECT_EQ(EINVAL, error);
}

TEST_F(PercpuArrayMapLookupTest, CorrectLookup) {
  int error;
  uint32_t key = 50;
  uint64_t value[ebpf_ncpus()];

  error = ebpf_map_lookup_elem_from_user(em, &key, value);
  EXPECT_EQ(0, error);

  for (uint16_t i = 0; i < ebpf_ncpus(); i++) {
    EXPECT_EQ(100, value[i]);
  }
}
}  // namespace
