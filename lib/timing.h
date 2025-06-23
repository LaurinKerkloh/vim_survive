#ifndef timing_h
#define timing_h
#include <stdbool.h>
#include <stdint.h>

int64_t now(void);
void wait(int64_t time);
bool check_stdin(int64_t timeout);
int64_t until_end_of_frame(int64_t start_time, uint16_t target_frame_time);
#endif
