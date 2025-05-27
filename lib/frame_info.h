#ifndef FRAME_INFO_H
#define FRAME_INFO_H

#include <stdint.h>

struct FrameInfo {
  int64_t start;
  int64_t end;
};

struct FrameInfoBuffer {
  struct FrameInfo *frame_info;
  struct FrameInfo *current_frame;
  uint16_t current_frame_index;
  uint16_t length;
};

struct FrameInfo *initialize_frame_info(struct FrameInfo *frame_info,
                                        uint16_t size);

struct FrameInfoBuffer
initialize_frame_info_buffer(struct FrameInfo *frame_info, uint16_t size);

void advance_frame_info_buffer(struct FrameInfoBuffer *b);

int64_t average_active_time(struct FrameInfoBuffer *b);

int64_t average_fps(struct FrameInfoBuffer *b);

#endif
