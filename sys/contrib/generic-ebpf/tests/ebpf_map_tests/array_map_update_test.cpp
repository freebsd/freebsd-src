#include <gtest/gtest.h>

extern "C" {
#include <errno.h>
#include <stdint.h>
#include <sys/ebpf.h>

#include "../test_common.hpp"
}

namespace {
class ArrayMapUpdateTest : public CommonFixture {
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

TEST_F(ArrayMapUpdateTest, UpdateWithMaxPlusOneKey) {
  int error;
  uint32_t key = 100, value = 100;

  error = ebpf_map_update_elem_from_user(em, &key, &value, EBPF_ANY);

  EXPECT_EQ(EINVAL, error);
}

TEST_F(ArrayMapUpdateTest, CorrectUpdate) {
  int error;
  uint32_t key = 50, value = 100;

  error = ebpf_map_update_elem_from_user(em, &key, &value, EBPF_ANY);

  EXPECT_EQ(0, error);
}

TEST_F(ArrayMapUpdateTest, CorrectUpdateOverwrite) {
  int error;
  uint32_t key = 50, value = 100;

  error = ebpf_map_update_elem_from_user(em, &key, &value, EBPF_ANY);
  ASSERT_TRUE(!error);

  value = 101;
  error = ebpf_map_update_elem_from_user(em, &key, &value, EBPF_ANY);

  EXPECT_EQ(0, error);
}

TEST_F(ArrayMapUpdateTest, CreateMoreThenMaxEntries) {
  int error;
  uint32_t key, value = 100;

  for (int i = 0; i < 100; i++) {
    key = i;
    error = ebpf_map_update_elem_from_user(em, &key, &value, EBPF_ANY);
    ASSERT_TRUE(!error);
  }

  key++;
  error = ebpf_map_update_elem_from_user(em, &key, &value, EBPF_ANY);

  /*
   * In array map, max_entries equals to max key, so
   * returns EINVAL, not EBUSY
   */
  EXPECT_EQ(EINVAL, error);
}

TEST_F(ArrayMapUpdateTest, UpdateElementWithNOEXISTFlag) {
  int error;
  uint32_t key = 50, value = 100;

  error = ebpf_map_update_elem_from_user(em, &key, &value, EBPF_NOEXIST);

  EXPECT_EQ(EEXIST, error);
}
}  // namespace
