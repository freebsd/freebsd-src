#include <gtest/gtest.h>

extern "C" {
#include <errno.h>
#include <stdint.h>
#include <sys/ebpf.h>

#include "../test_common.hpp"
}

namespace {

class PercpuArrayMapDeleteTest : public CommonFixture {
 protected:
  struct ebpf_map *em;

  virtual void SetUp() {
    int error;
    uint32_t gkey = 50;
    uint32_t gval = 100;

    CommonFixture::SetUp();

    struct ebpf_map_attr attr;
    attr.type = EBPF_MAP_TYPE_PERCPU_ARRAY;
    attr.key_size = sizeof(uint32_t);
    attr.value_size = sizeof(uint32_t);
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

/* Delete always failes */
TEST_F(PercpuArrayMapDeleteTest, CorrectDelete) {
  int error;
  uint32_t key = 50;

  error = ebpf_map_delete_elem_from_user(em, &key);

  EXPECT_EQ(EINVAL, error);
}
}  // namespace
