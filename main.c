#include "lib/frame_info.h"
#include "lib/timing.h"
#include <ctype.h>
#include <fcntl.h>
#include <inttypes.h>
#include <locale.h>
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
#define OUTPUT_BUFFER_SIZE 16384
#define GAME_WIDTH 30
#define GAME_HEIGHT 30

#define CTRL_KEY(k) ((k) & 0x1f)
#define ESC 27
#define BLOCK 0x88
struct Point {
  int32_t x;
  int32_t y;
};

struct Point screen_size;

//////////////////////////////////
// TERMINAL AND IO SETTINGS
//////////////////////////////////

char output_buffer[OUTPUT_BUFFER_SIZE];
struct termios original_termios;
int flags;

void set_output_buffer(void) {
  setvbuf(stdout, output_buffer, _IOFBF, OUTPUT_BUFFER_SIZE);
}

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
  set_output_buffer();
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

///////////////////////////////////////////////
void clear_screen(void) { printf("\033[2J\033[1;1H"); }

void move_cursor_point(struct Point p) { printf("\033[%d;%dH", p.y, p.x); }
void move_cursor(uint16_t x, uint16_t y) { printf("\033[%d;%dH", y, x); }

////////////////
// Game State //
////////////////

struct Point level_size = {GAME_WIDTH, GAME_HEIGHT};
struct Point player = {10, 10};

struct Point game_offset;

bool set_game_offset(void) {
  if (level_size.x * 2 > screen_size.x || level_size.y > screen_size.y) {
    return false;
  }
  game_offset.x = screen_size.x / 2 - level_size.x;
  game_offset.y = screen_size.y / 2 - level_size.y / 2;
  return true;
}

struct Point game_point_to_terminal(struct Point game) {
  struct Point p = {game_offset.x + game.x * 2, game_offset.y + game.y};
  return p;
}

void move_cursor_game(int16_t x, int16_t y) {
  move_cursor_point(game_point_to_terminal((struct Point){x, y}));
}
void move_cursor_game_point(struct Point p) {
  move_cursor_point(game_point_to_terminal(p));
}

void draw_border(void) {
  for (int x = -1; x <= level_size.x + 1; x++) {
    for (int y = -1; y <= level_size.y + 1; y++) {
      if (x == -1 || x == level_size.x + 1 || y == -1 ||
          y == level_size.y + 1) {
        move_cursor_game(x, y);
        printf("██");
      }
    }
  }
}

int main(void) {
  setlocale(LC_ALL, "");
  configure_terminal();
  clear_screen();

  set_screen_size();
  set_game_offset();

  move_cursor(1, 1);
  printf("Screen Size: %d, %d", screen_size.x, screen_size.y);
  move_cursor(1, 2);
  printf("Game Offset: %d, %d", game_offset.x, game_offset.y);
  fflush(stdout);
  sleep(2);

  struct FrameInfo recent_frames_data[RECENT_FRAMES_SIZE];
  struct FrameInfoBuffer frame_info =
      initialize_frame_info_buffer(recent_frames_data, RECENT_FRAMES_SIZE);

  bool exited = false;
  char input_buffer[20];
  char last_input[sizeof(input_buffer) + 1];

  while (!exited) {
    frame_info.current_frame->start = now();

    ssize_t read_bytes =
        read(STDIN_FILENO, input_buffer, sizeof(input_buffer) - 1);
    input_buffer[read_bytes] = '\0';
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
      case 'h':
        break;
      }
    }

    clear_screen();
    draw_border();

    // Frame info
    move_cursor(screen_size.x - 8, 1);
    printf("FPS :%4ld", average_fps(&frame_info));
    move_cursor(screen_size.x - 8, 2);
    printf("Load:%3ld%%", average_active_time(&frame_info) * 100 / FRAME_TIME);

    // Input info
    move_cursor(1, 1);

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
