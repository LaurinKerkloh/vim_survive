#include "frame_info.h"

struct FrameInfo *initialize_frame_info(struct FrameInfo *frame_info,
                                        uint16_t size) {
  for (uint16_t i = 0; i < size; i++) {
    frame_info[i].start = -1;
    frame_info[i].end = -1;
  }

  return &frame_info[0];
}

struct FrameInfoBuffer
initialize_frame_info_buffer(struct FrameInfo *frame_info, uint16_t size) {
  struct FrameInfoBuffer buffer;
  buffer.frame_info = frame_info;
  buffer.current_frame = frame_info;
  buffer.current_frame_index = 0;
  buffer.length = size;

  for (uint16_t i = 0; i < size; i++) {
    frame_info[i].start = -1;
    frame_info[i].end = -1;
  }

  return buffer;
}

void advance_frame_info_buffer(struct FrameInfoBuffer *b) {
  b->current_frame_index = (b->current_frame_index + 1) % b->length;
  b->current_frame = &b->frame_info[b->current_frame_index];
}

int64_t average_active_time(struct FrameInfoBuffer *b) {
  int64_t total_active_time = 0;
  uint16_t count = 0;
  for (uint16_t i = 0; i < b->length; i++) {
    if (i == b->current_frame_index)
      continue;
    if (b->frame_info[i].start == -1 || b->frame_info[i].end == -1)
      continue;
    count++;
    total_active_time += b->frame_info[i].end - b->frame_info[i].start;
  }
  if (count == 0) {
    return -1;
  }
  return total_active_time / count;
}

int64_t average_fps(struct FrameInfoBuffer *b) {
  int64_t minimum_start = 0;
  int64_t maximum_start = 0;

  for (uint16_t i = 0; i < b->length; i++) {
    if (b->frame_info[i].start < minimum_start) {
      minimum_start = b->frame_info[i].start;
    }
    if (b->frame_info[i].start > maximum_start) {
      maximum_start = b->frame_info[i].start;
    }
  }

  int64_t duration = maximum_start - minimum_start;

  return (b->length - 1 * 1000 / duration);
}
