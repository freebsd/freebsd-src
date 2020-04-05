#include <gtest/gtest.h>

extern "C" {
#include <dev/ebpf/ebpf_platform.h>
}

int main(int argc, char **argv) {
  int error;

  error = ebpf_init();
  ebpf_assert(error == 0);

  ::testing::InitGoogleTest(&argc, argv);

  error = RUN_ALL_TESTS();
  ebpf_assert(error == 0);

  error = ebpf_deinit();
  ebpf_assert(error == 0);

  return 0;
}
