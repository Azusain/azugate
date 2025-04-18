#include <gtest/gtest.h>
TEST(RouteTest, _1) { EXPECT_EQ(1 + 2, 3); }

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}