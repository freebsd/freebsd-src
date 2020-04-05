#include <gtest/gtest.h>

extern "C" {
#include <errno.h>
#include <stdint.h>
#include <sys/ebpf.h>

#include "../test_common.hpp"
}

class MapCreateTest : public CommonFixture {
 protected:
  struct ebpf_map *em;

  virtual void SetUp() {
    int error;
    CommonFixture::SetUp();
    em = NULL;
  }

  virtual void TearDown() {
    int error;
    if (em != NULL) ebpf_map_destroy(em);
    CommonFixture::TearDown();
  }
};

TEST_F(MapCreateTest, CreateWithNULLMapPointer) {
  int error;

  struct ebpf_map_attr attr;
  attr.type = EBPF_MAP_TYPE_ARRAY;
  attr.key_size = sizeof(uint32_t);
  attr.value_size = sizeof(uint32_t);
  attr.max_entries = 100;
  attr.flags = 0;

  error = ebpf_map_create(ee, NULL, &attr);

  EXPECT_EQ(EINVAL, error);
}

TEST_F(MapCreateTest, CreateWithInvalidMapType1) {
  int error;

  struct ebpf_map_attr attr;
  attr.type = EBPF_MAP_TYPE_MAX;
  attr.key_size = sizeof(uint32_t);
  attr.value_size = sizeof(uint32_t);
  attr.max_entries = 100;
  attr.flags = 0;

  error = ebpf_map_create(ee, &em, &attr);

  EXPECT_EQ(EINVAL, error);
}

TEST_F(MapCreateTest, CreateWithInvalidMapType2) {
  int error;

  struct ebpf_map_attr attr;
  attr.type = EBPF_MAP_TYPE_MAX + 1;
  attr.key_size = sizeof(uint32_t);
  attr.value_size = sizeof(uint32_t);
  attr.max_entries = 100;
  attr.flags = 0;

  error = ebpf_map_create(ee, &em, &attr);

  EXPECT_EQ(EINVAL, error);
}

TEST_F(MapCreateTest, CreateWithZeroKey) {
  int error;

  struct ebpf_map_attr attr;
  attr.type = EBPF_MAP_TYPE_ARRAY;
  attr.key_size = 0;
  attr.value_size = sizeof(uint32_t);
  attr.max_entries = 100;
  attr.flags = 0;

  error = ebpf_map_create(ee, &em, &attr);

  EXPECT_EQ(EINVAL, error);
}

TEST_F(MapCreateTest, CreateWithZeroValue) {
  int error;

  struct ebpf_map_attr attr;
  attr.type = EBPF_MAP_TYPE_ARRAY;
  attr.key_size = sizeof(uint32_t);
  attr.value_size = 0;
  attr.max_entries = 100;
  attr.flags = 0;

  error = ebpf_map_create(ee, &em, &attr);

  EXPECT_EQ(EINVAL, error);
}

TEST_F(MapCreateTest, CreateWithZeroMaxEntries) {
  int error;

  struct ebpf_map_attr attr;
  attr.type = EBPF_MAP_TYPE_ARRAY;
  attr.key_size = sizeof(uint32_t);
  attr.value_size = sizeof(uint32_t);
  attr.max_entries = 0;
  attr.flags = 0;

  error = ebpf_map_create(ee, &em, &attr);

  EXPECT_EQ(EINVAL, error);
}
