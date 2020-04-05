#include <gtest/gtest.h>

extern "C" {
#include <errno.h>
#include <stdint.h>
#include <sys/ebpf.h>

#include "../test_common.hpp"
}

namespace {
class ArrayMapLookupTest : public CommonFixture {
 protected:
  struct ebpf_map *em;

  virtual void SetUp() {
    int error;
    uint32_t gkey = 50;
    uint64_t gval = 100;

    CommonFixture::SetUp();

    struct ebpf_map_attr attr;
    attr.type = EBPF_MAP_TYPE_ARRAY;
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

TEST_F(ArrayMapLookupTest, LookupMaxEntryPlusOne) {
  int error;
  uint32_t key = 100;
  uint64_t value;

  error = ebpf_map_lookup_elem_from_user(em, &key, &value);

  EXPECT_EQ(EINVAL, error);
}

TEST_F(ArrayMapLookupTest, LookupOutOfMaxEntry) {
  int error;
  uint32_t key = 102;
  uint64_t value;

  error = ebpf_map_lookup_elem_from_user(em, &key, &value);

  EXPECT_EQ(EINVAL, error);
}

TEST_F(ArrayMapLookupTest, CorrectLookup) {
  int error;
  uint32_t key = 50;
  uint64_t value;

  error = ebpf_map_lookup_elem_from_user(em, &key, &value);

  EXPECT_EQ(0, error);
  EXPECT_EQ(100, value);
}
}  // namespace
