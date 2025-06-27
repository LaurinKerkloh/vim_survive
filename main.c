#include "lib/frame_info.h"
#include "lib/terminalio.h"
#include "lib/timing.h"
#include <ctype.h>
#include <fcntl.h>
#include <inttypes.h>
#include <locale.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#define FPS 30
#define FRAME_TIME 1000 / FPS
#define RECENT_FRAMES_SIZE FPS

#define INPUT_BUFFER_SIZE 20
#define INPUT_CHAIN_SIZE 6
#define COMMAND_CHAIN 10
#define GAME_WIDTH 10
#define GAME_HEIGHT 5

#define UP 0
#define DOWN 1
#define LEFT 2
#define RIGHT 3

#define CTRL_KEY(k) ((k) & 0x1f)
#define ESC 27

// COLORS

#define SMILING_FACE "\xE2\x98\xBB\0"
#define SQUARE "\xE2\x96\xA0\0"

struct Vector {
  int32_t x;
  int32_t y;
};

struct Drawable {
  struct Vector position;
  struct Display display;
};

//////////////////////
// Global Variables //
//////////////////////
struct Vector screen_size;

struct FrameInfo recent_frames_data[RECENT_FRAMES_SIZE];
struct FrameInfoBuffer frame_info;

char input_buffer[INPUT_BUFFER_SIZE];
char last_input[INPUT_BUFFER_SIZE];

//////////////////////////////
// TERMINAL AND IO SETTINGS //
//////////////////////////////

///////////////////////////////////////////////

char input_chain[INPUT_CHAIN_SIZE];

void print_frame_info(void) {
  // draw_string(screen_size.x - 9, 0, "FPS :%4ld", average_fps(&frame_info));
  // draw_string(screen_size.x - 9, 1, "Load:%3ld%%",
  //             average_active_time(&frame_info) * 100 / FRAME_TIME);
}

////////////////
// Game State //
////////////////

bool exited = false;
bool command_mode = false;
struct Vector level_size = {GAME_WIDTH, GAME_HEIGHT};
struct Vector game_offset;

struct Drawable player;

bool set_game_offset(void) {
  if (level_size.x * 2 > screen_size.x || level_size.y > screen_size.y) {
    return false;
  }
  game_offset.x = screen_size.x / 2 - level_size.x / 2;
  game_offset.y = screen_size.y / 2 - level_size.y / 2;
  return true;
}

struct Vector game_vector_to_terminal(struct Vector game) {
  struct Vector p = {game_offset.x + game.x, game_offset.y + game.y};
  return p;
}

// void draw_border(void) {
//   for (int x = 0; x < level_size.x + 1; x++) {
//     for (int y = 0; y < level_size.y + 1; y++) {
//       if (x == 0 || x == level_size.x || y == 0 || y == level_size.y) {
//         draw_display(x + game_offset.x, y + game_offset.y,
//                      (struct Display){" ", {BACKGROUND(WHITE), '\0'}});
//       }
//     }
//   }
// }

struct Vector vector_from_direction(uint8_t direction, uint8_t distance) {
  struct Vector result;
  switch (direction) {
  case UP:
    result.x = 0;
    result.y = -distance;
    break;
  case DOWN:
    result.x = 0;
    result.y = distance;
    break;
  case LEFT:
    result.x = -distance;
    result.y = 0;
    break;
  case RIGHT:
    result.x = distance;
    result.y = 0;
    break;
  }
  return result;
}

struct Vector add_vector(struct Vector a, struct Vector b) {
  struct Vector result = {a.x + b.x, a.y + b.y};
  return result;
}

bool out_off_bounds(struct Vector v) {
  return (v.x < 0 || v.x > GAME_WIDTH || v.y < 0 || v.y > GAME_HEIGHT);
}

bool try_player_move(struct Vector vector) {
  struct Vector new_location = add_vector(player.position, vector);
  if (out_off_bounds(new_location)) {
    return false;
  }
  player.position = new_location;
  return true;
}

void add_to_input_chain(char c) {
  if (strlen(input_chain) >= INPUT_CHAIN_SIZE - 1) {
    for (uint8_t i = 0; i < strlen(input_chain); i++) {
      input_chain[i] = input_chain[i + 1];
    }
  }

  uint8_t length = strlen(input_chain);
  input_chain[length] = c;
  input_chain[length + 1] = '\0';
}

void clear_input_chain(void) { input_chain[0] = '\0'; }

void process_input(void) {
  for (ssize_t i = 0; input_buffer[i] != '\0'; i++) {
    if (isdigit(input_buffer[i])) {
      add_to_input_chain(input_buffer[i]);
    } else {
      switch (input_buffer[i]) {
      case 'h':
        try_player_move(vector_from_direction(LEFT, 1));
        break;
      case 'j':
        try_player_move(vector_from_direction(DOWN, 1));
        break;
      case 'k':
        try_player_move(vector_from_direction(UP, 1));
        break;
      case 'l':
        try_player_move(vector_from_direction(RIGHT, 1));
        break;
      case ':':
        command_mode = true;
        break;
      case ESC: // escape char
        switch (input_buffer[i + 1]) {
        case '[':
          break;
        default: // ESC pressed
          clear_input_chain();
          break;
        }
      }
    }
  }
}

void process_command_input(void) {
  for (ssize_t i = 0; input_buffer[i] != '\0'; i++) {
    switch (input_buffer[i]) {
    case 'q':
      exited = true;
      break;
    case 'r':
      // TODO:
      // set_screen_size();
      break;
    case ESC: // escape char
      switch (input_buffer[i + 1]) {
      case '[':
        break;
      default: // ESC pressed
        command_mode = false;
        break;
      }
    }
  }
}

void print_command_mode_info(void) {
  int height = screen_size.y / 2;
  int width = screen_size.x / 2;
  int top = screen_size.y / 2 - height / 2;
  int left = screen_size.x / 2 - width / 2;

  struct Style style = color_style(color_8(BLACK), color_8(WHITE));
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      draw_display(left + x, top + y, (struct Display){" ", style});
    }
  }

  change_modes(&style, 1, BOLD);
  draw_styled_string(left + (width - 6) / 2, top + 1, style, "PAUSED");
  change_modes(&style, 0);
  draw_styled_string(left + (width - 14) / 2, top + 3, style,
                     "  r: resize screen");
  draw_styled_string(left + (width - 14) / 2, top + 4, style, "  q:    quit");
  draw_styled_string(left + (width - 14) / 2, top + 5, style, "ESC:  continue");
}

// void print_input_info(void) {
//   char string[10];
//   draw_string(1, 1, "Last Input: ", "");
//   for (uint8_t i = 0; i < sizeof(last_input); i++) {
//     if (last_input[i] == '\0') {
//       break;
//     }
//     if (isprint(last_input[i])) {
//       sprintf(string, "%d (%c) ", last_input[i], last_input[i]);
//     } else {
//       sprintf(string, "%d ", last_input[i]);
//     }
//
//   }
//
//   move_cursor(1, 2);
//   printf("Chain length: %lu", strlen(input_chain));
//   move_cursor(1, 3);
//   printf("Chain: %s", input_chain);
// }

int main(void) {

  // TODO: this in terminalio?
  setlocale(LC_ALL, "");

  struct Display d = {"o", default_style()};

  unsigned int screen_size_x, screen_size_y;
  init_terminalio(&screen_size_x, &screen_size_y);
  screen_size.x = (int64_t)screen_size_x;
  screen_size.y = (int64_t)screen_size_y;

  set_game_offset();

  // move_cursor(1, 1);
  // printf("Screen Size: %d, %d", screen_size.x, screen_size.y);
  // move_cursor(1, 2);
  // printf("Game Offset: %d, %d", game_offset.x, game_offset.y);
  // fflush(stdout);
  // sleep(2);

  player.position.x = GAME_WIDTH / 2;
  player.position.y = GAME_HEIGHT / 2;
  player.display = (struct Display){
      SQUARE, color_style(color_rgb(255, 23, 46), default_color())};

  frame_info =
      initialize_frame_info_buffer(recent_frames_data, RECENT_FRAMES_SIZE);

  while (!exited) {
    frame_info.current_frame->start = now();
    read_input(input_buffer, sizeof(input_buffer));

    if (command_mode) {
      process_command_input();
    } else {
      process_input();
    }

    // struct Display d = {"o", color_style(color_8(RED), color_8(GREEN))};
    for (int x = 0; x < screen_size.x; x++) {
      for (int y = 0; y < screen_size.y; y++) {
        draw_display(x, y, d);
      }
    }

    // draw_display(player.position.x, player.position.y, player.display);

    if (command_mode) {
      // print_command_mode_info();
    }

    print_frame_info();
    // print_input_info();

    render_frame();

    // Frame end
    frame_info.current_frame->end = now();

    wait(until_end_of_frame(frame_info.current_frame->start, FRAME_TIME));

    advance_frame_info_buffer(&frame_info);
  }

  return 0;
}
