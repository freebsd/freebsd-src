#include <gtest/gtest.h>

extern "C" {
#include <errno.h>
#include <stdint.h>
#include <sys/ebpf.h>
#include <sys/ebpf_vm_isa.h>

#include "../test_common.hpp"
}

class ProgLoadTest : public CommonFixture {
 protected:
  struct ebpf_prog *ep;

  virtual void SetUp() {
    int error;
    CommonFixture::SetUp();
    ep = NULL;
  }

  virtual void TearDown() {
    int error;
    if (ep != NULL) ebpf_prog_destroy(ep);
    CommonFixture::TearDown();
  }
};

TEST_F(ProgLoadTest, LoadWithNULLProgPointer) {
  int error;

  struct ebpf_inst insts[] = {{EBPF_OP_EXIT, 0, 0, 0, 0}};

  struct ebpf_prog_attr attr = {
      .type = EBPF_PROG_TYPE_TEST, .prog = insts, .prog_len = 1};

  error = ebpf_prog_create(ee, NULL, &attr);

  EXPECT_EQ(EINVAL, error);
}

TEST_F(ProgLoadTest, LoadWithInvalidProgType1) {
  int error;

  struct ebpf_inst insts[] = {{EBPF_OP_EXIT, 0, 0, 0, 0}};

  struct ebpf_prog_attr attr = {
      .type = EBPF_PROG_TYPE_MAX, .prog = insts, .prog_len = 1};

  error = ebpf_prog_create(ee, &ep, &attr);

  EXPECT_EQ(EINVAL, error);
}

TEST_F(ProgLoadTest, LoadWithInvalidProgType2) {
  int error;

  struct ebpf_inst insts[] = {{EBPF_OP_EXIT, 0, 0, 0, 0}};

  struct ebpf_prog_attr attr = {
      .type = EBPF_PROG_TYPE_MAX + 1, .prog = insts, .prog_len = 1};

  error = ebpf_prog_create(ee, &ep, &attr);

  EXPECT_EQ(EINVAL, error);
}

TEST_F(ProgLoadTest, LoadWithZeroLen) {
  int error;

  struct ebpf_inst insts[] = {{EBPF_OP_EXIT, 0, 0, 0, 0}};

  struct ebpf_prog_attr attr = {
      .type = EBPF_PROG_TYPE_TEST, .prog = insts, .prog_len = 0};

  error = ebpf_prog_create(ee, &ep, &attr);

  EXPECT_EQ(EINVAL, error);
}

TEST_F(ProgLoadTest, LoadWithNULLProg) {
  int error;

  struct ebpf_prog_attr attr = {
      .type = EBPF_PROG_TYPE_TEST, .prog = NULL, .prog_len = 1};

  error = ebpf_prog_create(ee, &ep, &attr);

  EXPECT_EQ(EINVAL, error);
}

TEST_F(ProgLoadTest, CorrectLoad) {
  int error;

  struct ebpf_inst insts[] = {{EBPF_OP_EXIT, 0, 0, 0, 0}};

  struct ebpf_prog_attr attr = {
      .type = EBPF_PROG_TYPE_TEST, .prog = insts, .prog_len = 1};

  error = ebpf_prog_create(ee, &ep, &attr);

  EXPECT_EQ(0, error);
}
