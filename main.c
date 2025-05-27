#include "lib/frame_info.h"
#include "lib/timing.h"
#include <ctype.h>
#include <curses.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

#define FPS 30
#define FRAME_TIME 1000 / FPS
#define RECENT_FRAMES_SIZE FPS

#define INPUT_BUFFER_SIZE 20

#define CTRL_KEY(k) ((k) & 0x1f)
#define ESC 27

struct Point {
  uint16_t x;
  uint16_t y;
};

struct Point screen_size;

//////////////////////////////////
// TERMINAL AND INPUT SETTINGS
//////////////////////////////////

struct termios original_termios;
int flags;

void set_blocking_input(void) {
  fcntl(STDIN_FILENO, F_SETFL, flags & ~O_NONBLOCK);
}

void set_non_blocking_input(void) {
  fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
}

void restore_terminal(void) {
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_termios);
  set_blocking_input();

  // show cursor
  printf("\033[?25h");
  fflush(stdout);
}

void configure_terminal(void) {
  tcgetattr(STDIN_FILENO, &original_termios);
  flags = fcntl(STDIN_FILENO, F_GETFL, 0);

  atexit(restore_terminal);

  struct termios changed = original_termios;
  changed.c_iflag &= ~(IXON | ICRNL);
  changed.c_oflag &= ~(OPOST);
  changed.c_lflag &= ~(ICANON | ECHO | IEXTEN | ISIG);
  changed.c_cc[VMIN] = 1;
  changed.c_cc[VTIME] = 0;
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &changed);

  set_non_blocking_input();

  // hide cursor
  printf("\033[?25l");
  fflush(stdout);
}

void set_screen_size(void) {
  set_blocking_input();
  // Move cursor to bottom right as far as possible
  printf("\033[9999;9999H");
  // Ask for cursor position
  printf("\033[6n");
  fflush(stdout);

  // Read response from stdin
  char in[100];
  uint16_t each = 0;
  char ch = 0;

  while ((ch = getchar()) != 'R') {
    if (ch == EOF) {
      break;
    }
    if (isprint(ch)) {
      if (each + 1 < sizeof(in)) {
        in[each] = ch;
        each++;
        in[each] = '\0';
      }
    }
  }
  printf("\033[1;1H");

  screen_size.x = 0;
  screen_size.y = 0;

  sscanf(in, "[%hd;%hd", &screen_size.y, &screen_size.x);

  set_non_blocking_input();
}

/////////////////////////////////////////////////////
void clear_screen(void) {
  printf("\033[2J\033[1;1H");
  fflush(stdout);
}

int main(void) {
  configure_terminal();
  clear_screen();

  set_screen_size();

  printf("Rows: %d, Cols: %d", screen_size.y, screen_size.x);
  fflush(stdout);
  sleep(1);

  struct FrameInfo recent_frames_data[RECENT_FRAMES_SIZE];
  struct FrameInfoBuffer frame_info =
      initialize_frame_info_buffer(recent_frames_data, RECENT_FRAMES_SIZE);

  bool exited = false;
  char input_buffer[20];
  char last_input[sizeof(input_buffer) + 1];

  while (!exited) {
    frame_info.current_frame->start = now();

    // TODO: allways read until EOF, and consider all inputted characters
    // correctly identify end of special characters
    ssize_t read_bytes = read(STDIN_FILENO, input_buffer, sizeof(input_buffer));
    for (ssize_t i = 0; i < read_bytes; i++) {
      last_input[i] = input_buffer[i];
      last_input[i + 1] = '\0';

      switch (input_buffer[i]) {
      case ESC: // escape char
        switch (input_buffer[i + 1]) {
        case '[':
          break;
        default: // ESC pressed
          exited = true;
          break;
        }
      case CTRL_KEY('r'):
        set_screen_size();
        break;
      }
    }

    clear_screen();
    // Frame info
    printf("\033[1;%dH", screen_size.x - 8);
    printf("FPS :%4ld", average_fps(&frame_info));
    printf("\033[2;%dH", screen_size.x - 8);
    printf("Load:%3ld%%", average_active_time(&frame_info) * 100 / FRAME_TIME);

    // Input info
    printf("\033[1;1H");

    printf("Last Input: ");
    for (uint8_t i = 0; i < sizeof(last_input); i++) {
      if (last_input[i] == '\0') {
        break;
      }
      if (isprint(last_input[i])) {
        printf("%d (%c) ", last_input[i], last_input[i]);
      } else {
        printf("%d ", last_input[i]);
      }
    }

    fflush(stdout);

    frame_info.current_frame->end = now();

    wait(until_end_of_frame(frame_info.current_frame->start, FRAME_TIME));

    advance_frame_info_buffer(&frame_info);
  }

  return 0;
}
