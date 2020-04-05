#include <gtest/gtest.h>

extern "C" {
#include <errno.h>
#include <stdint.h>
#include <sys/ebpf.h>

#include "../test_common.hpp"
}

namespace {
class MapDeleteTest : public CommonFixture {
 protected:
  struct ebpf_map *em;

  virtual void SetUp() {
    int error;

    CommonFixture::SetUp();

    struct ebpf_map_attr attr;
    attr.type = EBPF_MAP_TYPE_ARRAY;
    attr.key_size = sizeof(uint32_t);
    attr.value_size = sizeof(uint32_t);
    attr.max_entries = 100;
    attr.flags = 0;

    error = ebpf_map_create(ee, &em, &attr);
    ASSERT_TRUE(!error);
  }

  virtual void TearDown() {
    ebpf_map_destroy(em);
    CommonFixture::TearDown();
  }
};

TEST_F(MapDeleteTest, DeleteWithNULLMap) {
  int error;
  uint32_t key = 100;

  error = ebpf_map_delete_elem(NULL, &key);

  EXPECT_EQ(EINVAL, error);
}

TEST_F(MapDeleteTest, DeleteWithNULLKey) {
  int error;

  error = ebpf_map_delete_elem(em, NULL);

  EXPECT_EQ(EINVAL, error);
}
}  // namespace
