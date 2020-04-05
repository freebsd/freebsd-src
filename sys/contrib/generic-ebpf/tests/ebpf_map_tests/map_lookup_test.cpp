#include <gtest/gtest.h>

extern "C" {
#include <errno.h>
#include <stdint.h>
#include <sys/ebpf.h>

#include "../test_common.hpp"
}

class MapLookupTest : public CommonFixture {
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

TEST_F(MapLookupTest, LookupWithNULLMap) {
  int error;
  uint32_t key = 50;
  void *value;

  value = ebpf_map_lookup_elem(NULL, (void *)&key);

  EXPECT_EQ(NULL, value);
}

TEST_F(MapLookupTest, LookupWithNULLKey) {
  int error;
  void *value;

  value = ebpf_map_lookup_elem(em, NULL);

  EXPECT_EQ(NULL, value);
}

TEST_F(MapLookupTest, LookupWithNULLValue) {
  int error;
  uint32_t key = 100;
  void *value;

  value = ebpf_map_lookup_elem(em, (void *)&key);

  EXPECT_EQ(NULL, value);
}
