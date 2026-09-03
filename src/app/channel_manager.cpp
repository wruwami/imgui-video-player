#include "app/channel_manager.h"

#include <spdlog/spdlog.h>

#include <utility>

namespace vvp {

ChannelManager::~ChannelManager() { RemoveAll(); }

Channel& ChannelManager::AddIdle() {
  channels_.push_back(std::make_unique<Channel>());
  return *channels_.back();
}

Channel& ChannelManager::AddAndOpen(const std::string& url, const SourceOptions& options) {
  auto channel = std::make_unique<Channel>();
  Channel& ref = *channel;
  channels_.push_back(std::move(channel));
  ref.Open(url, options);
  return ref;
}

bool ChannelManager::Remove(size_t index) {
  if (index >= channels_.size()) {
    return false;
  }
  // Destroying a Channel closes it (joins the worker) before the pointer dies.
  channels_.erase(channels_.begin() + static_cast<std::ptrdiff_t>(index));
  spdlog::debug("ChannelManager: removed channel #{} ({} remaining)", index, channels_.size());
  return true;
}

void ChannelManager::RemoveAll() {
  if (channels_.empty()) {
    return;
  }
  channels_.clear();
  spdlog::debug("ChannelManager: all channels removed");
}

Channel* ChannelManager::Get(size_t index) { return index < channels_.size() ? channels_[index].get() : nullptr; }

const Channel* ChannelManager::Get(size_t index) const {
  return index < channels_.size() ? channels_[index].get() : nullptr;
}

}  // namespace vvp
