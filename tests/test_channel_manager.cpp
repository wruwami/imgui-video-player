#include <gtest/gtest.h>

#include <chrono>
#include <thread>

#include "app/channel_manager.h"

namespace {

using vvp::Channel;
using vvp::ChannelState;

// Polls until the predicate holds or the deadline passes (worker threads run
// asynchronously; tests must stay deterministic without sleeps).
template <typename Predicate>
bool WaitFor(const Predicate& pred, std::chrono::milliseconds timeout = std::chrono::milliseconds(3000)) {
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  while (std::chrono::steady_clock::now() < deadline) {
    if (pred()) {
      return true;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  return pred();
}

TEST(ChannelManager, AddIdle_CreatesIndependentIdleChannels) {
  vvp::ChannelManager mgr;
  Channel& a = mgr.AddIdle();
  Channel& b = mgr.AddIdle();

  EXPECT_EQ(mgr.count(), 2u);
  EXPECT_NE(&a, &b);
  EXPECT_EQ(a.state(), ChannelState::kIdle);
  EXPECT_EQ(b.state(), ChannelState::kIdle);
}

TEST(ChannelManager, Get_OutOfRange_ReturnsNull) {
  vvp::ChannelManager mgr;
  mgr.AddIdle();
  EXPECT_NE(mgr.Get(0), nullptr);
  EXPECT_EQ(mgr.Get(1), nullptr);
  const vvp::ChannelManager& cmgr = mgr;
  EXPECT_EQ(cmgr.Get(1), nullptr);
}

TEST(ChannelManager, OpenManyChannels_AllReachTerminalStateWithoutDeadlock) {
  vvp::ChannelManager mgr;
  vvp::SourceOptions options;  // Defaults; no renderer device needed in tests.

  // Nonexistent sources: workers must fail fast into kError — exercising the
  // full open -> fail -> stop lifecycle concurrently.
  for (int i = 0; i < 4; ++i) {
    mgr.AddAndOpen("Z:/definitely/missing_" + std::to_string(i) + ".mp4", options);
  }
  ASSERT_EQ(mgr.count(), 4u);

  EXPECT_TRUE(WaitFor([&] {
    for (size_t i = 0; i < mgr.count(); ++i) {
      const Channel* c = mgr.Get(i);
      if (c == nullptr || c->state() != ChannelState::kError) {
        return false;
      }
    }
    return true;
  })) << "all channels should reach kError";
}

TEST(ChannelManager, Remove_JoinsWorkerAndShiftsIndices) {
  vvp::ChannelManager mgr;
  for (int i = 0; i < 3; ++i) {
    mgr.AddAndOpen("Z:/definitely/missing_" + std::to_string(i) + ".mp4", vvp::SourceOptions{});
  }

  EXPECT_TRUE(WaitFor([&] { return mgr.Get(0) != nullptr && mgr.Get(0)->state() == ChannelState::kError; }));

  const Channel* second = mgr.Get(1);  // Capture before removal (slots shift down).
  EXPECT_TRUE(mgr.Remove(0));
  EXPECT_EQ(mgr.count(), 2u);
  EXPECT_EQ(mgr.Get(0), second);  // Dense vector: later channels shift down.
  EXPECT_FALSE(mgr.Remove(2));    // Out of range after removal.

  EXPECT_TRUE(WaitFor([&] {
    for (size_t i = 0; i < mgr.count(); ++i) {
      if (mgr.Get(i)->state() != ChannelState::kError) {
        return false;
      }
    }
    return true;
  }));
}

TEST(ChannelManager, RemoveAll_JoinsAllWorkers) {
  vvp::ChannelManager mgr;
  for (int i = 0; i < 4; ++i) {
    mgr.AddAndOpen("Z:/definitely/missing_" + std::to_string(i) + ".mp4", vvp::SourceOptions{});
  }

  mgr.RemoveAll();  // Must join every worker before returning; hanging here fails the test via timeout.
  EXPECT_EQ(mgr.count(), 0u);
  EXPECT_EQ(mgr.Get(0), nullptr);
}

TEST(ChannelManager, Destructor_ClosesOpenChannels) {
  {
    vvp::ChannelManager mgr;
    mgr.AddAndOpen("Z:/definitely/missing.mp4", vvp::SourceOptions{});
    // Manager destroyed while a worker is still running: destructor must join.
  }
  SUCCEED();
}

}  // namespace
