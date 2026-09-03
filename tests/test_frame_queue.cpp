#include <gtest/gtest.h>

#include "media/frame.h"
#include "media/frame_queue.h"

namespace {

// Creates a minimal frame carrying the given id in its width field.
vvp::VideoFramePtr MakeFrame(int id) {
  vvp::FramePtr raw(av_frame_alloc());
  raw->width = id;
  return std::make_shared<const vvp::VideoFrame>(std::move(raw), 0.0);
}

TEST(FrameQueue, Push_BeyondCapacity_DropsOldest) {
  vvp::FrameQueue queue(3);
  for (int i = 1; i <= 5; ++i) {
    queue.Push(MakeFrame(i));
  }
  EXPECT_EQ(queue.Size(), 3u);
  EXPECT_EQ(queue.DroppedCount(), 2u);

  const vvp::VideoFramePtr newest = queue.PopLatest();
  ASSERT_NE(newest, nullptr);
  EXPECT_EQ(newest->width(), 5);  // Items 1 and 2 were dropped, 3 survives.
}

TEST(FrameQueue, PopLatest_DrainsAndCountsUnconsumedAsDropped) {
  vvp::FrameQueue queue(4);
  for (int i = 1; i <= 4; ++i) {
    queue.Push(MakeFrame(i));
  }
  const vvp::VideoFramePtr newest = queue.PopLatest();
  ASSERT_NE(newest, nullptr);
  EXPECT_EQ(newest->width(), 4);
  EXPECT_EQ(queue.Size(), 0u);
  EXPECT_EQ(queue.DroppedCount(), 3u);  // Frames 1..3 were never consumed.
}

TEST(FrameQueue, PopLatest_OnEmpty_ReturnsNull) {
  vvp::FrameQueue queue(2);
  EXPECT_EQ(queue.PopLatest(), nullptr);
  EXPECT_EQ(queue.DroppedCount(), 0u);
}

TEST(FrameQueue, Clear_DropsEverything) {
  vvp::FrameQueue queue(4);
  queue.Push(MakeFrame(1));
  queue.Push(MakeFrame(2));
  queue.Clear();
  EXPECT_EQ(queue.Size(), 0u);
  EXPECT_EQ(queue.DroppedCount(), 2u);
  EXPECT_EQ(queue.PopLatest(), nullptr);
}

TEST(FrameQueue, ClosedQueue_IgnoresPushAndReportsClosed) {
  vvp::FrameQueue queue(2);
  queue.Push(MakeFrame(1));
  queue.Close();
  EXPECT_TRUE(queue.IsClosed());
  queue.Push(MakeFrame(2));  // Must be ignored, not crash or unblock oddly.
  EXPECT_EQ(queue.Size(), 1u);
  EXPECT_EQ(queue.DroppedCount(), 0u);
}

}  // namespace
