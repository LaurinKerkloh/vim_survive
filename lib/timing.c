#include "timing.h"
#include <sys/select.h>
#include <time.h>
#include <unistd.h>

int64_t timespec_to_milliseconds(struct timespec ts) {
  return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

struct timespec milliseconds_to_timespec(int64_t milliseconds) {
  struct timespec ts;
  ts.tv_sec = milliseconds / 1000;
  ts.tv_nsec = (milliseconds % 1000) * 1000000;
  return ts;
}

struct timeval milliseconds_to_timeval(int64_t milliseconds) {
  struct timeval tv;
  tv.tv_sec = milliseconds / 1000;
  tv.tv_usec = (milliseconds % 1000) * 1000;
  return tv;
}

int64_t now(void) {
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  return timespec_to_milliseconds(ts);
}

void wait(int64_t time) {
  if (time < 0) {
    return;
  }

  struct timespec ts = milliseconds_to_timespec(time);
  nanosleep(&ts, NULL);
}

bool check_stdin(int64_t timeout) {
  struct timeval timeout_tv = milliseconds_to_timeval(timeout);
  fd_set fds;
  FD_ZERO(&fds);
  FD_SET(STDIN_FILENO, &fds);

  select(STDIN_FILENO + 1, &fds, NULL, NULL, &timeout_tv);

  return (FD_ISSET(STDIN_FILENO, &fds) != 0);
}

int64_t until_end_of_frame(int64_t start_time, uint16_t target_frame_time) {
  return target_frame_time - (now() - start_time);
}
