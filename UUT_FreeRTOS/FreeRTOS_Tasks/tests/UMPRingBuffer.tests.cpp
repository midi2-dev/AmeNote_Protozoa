#include "../UMPRingBuffer.h"

#include <gtest/gtest.h>

//-----------------------------------------------

TEST(UMPRingBuffer, basics)
{
  UMPRingBuffer<16> b;

  uint16_t readPtr = 0;
  EXPECT_FALSE(b.itemsAvail(readPtr));

  uint32_t word;
  EXPECT_FALSE(b.read(readPtr, word));

  b.write(0x20901234);
  EXPECT_TRUE(b.read(readPtr, word));
  EXPECT_EQ(0x20901234, word);
  EXPECT_EQ(1u, readPtr);

  EXPECT_FALSE(b.read(readPtr, word));
  EXPECT_EQ(0x20901234, word);
  EXPECT_EQ(1u, readPtr);

  b.write(0x10F10000);
  b.write(0x30041122);
  b.write(0x33445566);

  EXPECT_TRUE(b.read(readPtr, word));
  EXPECT_EQ(0x10F10000, word);
  EXPECT_EQ(2u, readPtr);

  EXPECT_TRUE(b.read(readPtr, word));
  EXPECT_EQ(0x30041122, word);
  EXPECT_EQ(3u, readPtr);

  EXPECT_TRUE(b.read(readPtr, word));
  EXPECT_EQ(0x33445566, word);
  EXPECT_EQ(4u, readPtr);

  EXPECT_FALSE(b.read(readPtr, word));
  EXPECT_FALSE(b.read(readPtr, word));
  EXPECT_FALSE(b.read(readPtr, word));
}

TEST(UMPRingBuffer, wrap_around)
{
  UMPRingBuffer<128> b;

  uint16_t readPtr = 0;
  EXPECT_FALSE(b.itemsAvail(readPtr));

  uint32_t word;
  for (unsigned i=0; i<1000; ++i)
  {
    b.write(1000+i);

    EXPECT_TRUE(b.read(readPtr, word));
    EXPECT_EQ(1000+i, word);
    EXPECT_EQ((i+1) % 128, readPtr);
  }
}
