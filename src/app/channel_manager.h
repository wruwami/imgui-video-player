#pragma once

#include <cstddef>
#include <memory>
#include <vector>

#include "media/channel.h"
#include "media/video_source.h"

namespace vvp {

// Owns all playback channels and their lifecycles (issue #8: one pipeline
// instance per channel). UI-thread-only API — each Channel runs its own
// worker thread internally; the manager never touches channels from other
// threads.
//
// Indexing: channels are addressed by position in insertion order. Remove()
// shifts later indices down (dense vector); stable per-channel ids arrive
// with the grid binding work (issue #9) if needed.
class ChannelManager {
 public:
  ChannelManager() = default;
  ~ChannelManager();

  ChannelManager(const ChannelManager&) = delete;
  ChannelManager& operator=(const ChannelManager&) = delete;

  // Creates an unopened channel (state kIdle). Returns a stable pointer for
  // as long as the channel lives; it is invalidated by Remove()/RemoveAll().
  Channel& AddIdle();

  // Creates a channel and starts opening url immediately.
  Channel& AddAndOpen(const std::string& url, const SourceOptions& options);

  // Stops (joins the worker) and destroys the channel. Returns false if the
  // index is out of range.
  bool Remove(size_t index);

  // Stops and destroys every channel. Joins all workers before returning.
  void RemoveAll();

  Channel* Get(size_t index);
  const Channel* Get(size_t index) const;
  size_t count() const { return channels_.size(); }

 private:
  std::vector<std::unique_ptr<Channel>> channels_;
};

}  // namespace vvp
