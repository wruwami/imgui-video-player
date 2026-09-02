#include "media/ffmpeg_utils.h"

#include <cstring>

namespace vvp {

std::string AvErrorString(int errnum) {
  char buf[AV_ERROR_MAX_STRING_SIZE] = {0};
  av_strerror(errnum, buf, sizeof(buf));
  return buf;
}

double NowSeconds() { return av_gettime() * 1e-6; }

}  // namespace vvp
