#include "src/hello.h"
#include "cbor.h"

#include "gtest/gtest.h"

class CborTest : public ::testing::Test {};

TEST_F(CborTest, IntegerItem) {
  cbor_item_t * answer = cbor_build_uint8(42);
  EXPECT_EQ(cbor_get_uint8(answer), 42);
  cbor_decref(&answer);
  EXPECT_EQ(answer, nullptr);
}

