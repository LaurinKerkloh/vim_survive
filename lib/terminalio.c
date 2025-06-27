#include "terminalio.h"
#include <ctype.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unibilium.h>
#include <unistd.h>

#define OUTPUT_BUFFER_SIZE 1024 * 8

struct Display **next_frame_buffer, **previous_frame_buffer;
unsigned int screen_size_x, screen_size_y;

unibi_term *ut;

char *cursor_home;

bool check_terminal_capabilities(void) {
  const char *term = getenv("TERM");
  if (!term) {
    fprintf(stderr, "TERM not set in environment.\n");
    return false;
  }
  ut = unibi_from_term(term);
  if (!ut) {
    fprintf(stderr, "Could not load terminfo for terminal '%s'\n", term);
    return false;
  }

  const char *truecolor = getenv("COLORTERM");
  return true;
}

////////////////////////////
// Terminal Configuration //
////////////////////////////

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

  printf("\033[0m");
  printf("\033[2J");
  printf("\033[H");
  // show cursor
  printf("\033[?25h\n");

  printf("END");
  fflush(stdout);
}

void restore_terminal_on_signal(int sig) {
  restore_terminal();
  signal(sig, SIG_DFL);
  raise(sig);
}

void configure_terminal(void) {
  tcgetattr(STDIN_FILENO, &original_termios);
  flags = fcntl(STDIN_FILENO, F_GETFL, 0);

  atexit(restore_terminal);
  signal(SIGSEGV, restore_terminal_on_signal);

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

////////////
// Colors //
////////////

bool color_equal(struct Color *a, struct Color *b) {
  if (a->type != b->type) {
    return false;
  }

  if (a->type == DEFAULT) {
    return true;
  }

  if (a->type == _256 && a->color == b->color) {
    return true;
  }

  if (a->type == TRUE && a->red == b->red && a->green == b->green &&
      a->blue == b->blue) {
    return true;
  }

  return false;
}

struct Color color_rgb(uint8_t r, uint8_t g, uint8_t b) {
  struct Color c;
  c.type = TRUE;
  c.red = r;
  c.green = g;
  c.blue = b;
  return c;
}

struct Color color_256(uint8_t color) {
  struct Color c;
  c.type = _256;
  c.color = color;
  return c;
}

int min(int a, int b) {
  if (a < b)
    return a;
  else
    return b;
}

struct Color color_256_rgb(uint8_t r, uint8_t g, uint8_t b) {
  struct Color c;
  c.type = _256;
  c.color = 16 + 36 * min(r, 5) + 6 * min(g, 5) + min(b, 5);
  return c;
}

struct Color color_8(uint8_t color) {
  struct Color c;
  c.type = _8;
  c.color = min(color, 7);
  return c;
}

struct Color default_color(void) {
  struct Color c;
  c.type = DEFAULT;
  return c;
}

////////////
// Styles //
////////////

int qsort_char_compare(const void *a, const void *b) {
  return (*(char *)a - *(char *)b);
}

char unset_mode(char mode) {
  if (mode == BOLD) {
    return 22;
  } else {
    return mode + 20;
  }
}

bool valid_mode(char mode) {
  return (mode == BOLD || mode == DIM || mode == ITALIC || mode == UNDERLINE ||
          mode == BLINKING || mode == INVERSE || mode == HIDDEN ||
          mode == STRIKETHROUGH);
}

bool style_equal(struct Style *a, struct Style *b) {
  return (color_equal(&a->color, &b->color) &&
          color_equal(&a->background, &b->background) &&
          strcmp(a->modes, b->modes) == 0);
}

struct Style default_style(void) {
  struct Style s;
  s.color = default_color();
  s.background = default_color();
  s.modes[0] = '\0';
  return s;
}

struct Style color_style(struct Color color, struct Color background) {
  struct Style s;
  s.color = color;
  s.background = background;
  s.modes[0] = '\0';
  return s;
}

void stringify_modes(char *modes_string, unsigned int modes_count, ...) {
  modes_string[0] = '\0';
  uint8_t valid_modes_length = 0;

  va_list modes;
  va_start(modes, modes_count);

  for (uint8_t i = 0; i < modes_count; i++) {
    int mode = va_arg(modes, int);

    if (valid_mode(mode)) {
      bool duplicate = false;
      for (uint8_t x = 0; x < strlen(modes_string); x++) {
        if (modes_string[x] == mode) {
          duplicate = true;
        }
      }

      if (!duplicate) {
        modes_string[valid_modes_length] = mode;
        modes_string[valid_modes_length + 1] = '\0';
        valid_modes_length++;
      }
    }
  }
  va_end(modes);

  qsort(modes_string, strlen(modes_string), sizeof(char), qsort_char_compare);
}

struct Style full_style(struct Color color, struct Color background,
                        unsigned int modes_count, ...) {
  struct Style s;
  s.color = color;
  s.background = background;

  va_list args;
  va_start(args, modes_count);
  stringify_modes(s.modes, modes_count, args);
  va_end(args);

  return s;
}

void change_modes(struct Style *s, unsigned int modes_count, ...) {
  va_list args;
  va_start(args, modes_count);
  stringify_modes(s->modes, modes_count, args);
  va_end(args);
}
/////////////
// Display //
/////////////
bool display_equal(struct Display *a, struct Display *b) {
  return (strcmp(a->character, b->character) == 0 &&
          style_equal(&a->style, &b->style));
}

///////////////////////////
// Terminal Interactions //
///////////////////////////

struct Style current_style;

void set_screen_size(void) {
  set_blocking_input();
  // Move cursor to bottom right as far as possible
  printf("\033[9999;9999H");
  // Ask for cursor position
  printf("\033[6n");
  fflush(stdout);

  // Read response from stdin
  char in[100];
  unsigned int each = 0;
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
  printf("\033[H");

  screen_size_x = 0;
  screen_size_y = 0;

  sscanf(in, "[%d;%d", &screen_size_y, &screen_size_x);

  set_non_blocking_input();
}

void clear_screen(void) { printf("\033[2J"); }

// TODO: be more efficient with cursor moves, track position and only move when
// nececerry
void move_cursor(unsigned int x, unsigned int y) {
  // fprintf(stderr, "ESC[%d;%dH\n", y + 1, x + 1);
  printf("\033[%d;%dH", y + 1, x + 1);
}

void reset_display_modes(void) { printf("\033[0m"); }

void color_string(char *string, struct Color *c, bool background) {
  switch (c->type) {
  case DEFAULT:
    if (background) {
      sprintf(string, "49");
    } else {
      sprintf(string, "39");
    }
    break;

  case _8:
    if (background) {
      sprintf(string, "%d", c->color + 40);
    } else {
      sprintf(string, "%d", c->color + 30);
    }
    break;
  case _256:
    if (background) {
      sprintf(string, "48;5;%d", c->color + 40);
    } else {
      sprintf(string, "38;5;%d", c->color + 30);
    }
    break;
  case TRUE:
    if (background) {
      sprintf(string, "48:2:%d:%d:%d", c->red, c->green, c->blue);
    } else {
      sprintf(string, "38:2:%d:%d:%d", c->red, c->green, c->blue);
    }
    break;
  }
}

void remove_from_index(char *string, unsigned int index) {
  for (unsigned int i = index; i < strlen(string); i++) {
    string[i] = string[i + 1];
  }
}

void append_char(char *string, char c) {
  unsigned int l = strlen(string);
  string[l] = c;
  string[l + 1] = '\0';
}

void set_style(struct Style s) {
  if (style_equal(&current_style, &s))
    return;

  char output_modes[200] = "";
  if (!color_equal(&current_style.color, &s.color)) {
    char new_color_string[40];
    color_string(new_color_string, &s.color, false);
    strcat(output_modes, new_color_string);
  }

  if (!color_equal(&current_style.background, &s.background)) {
    char new_color_string[40];
    color_string(new_color_string, &s.background, true);
    if (strlen(output_modes) > 0) {
      append_char(output_modes, ';');
    }
    strcat(output_modes, new_color_string);
  }

  if (strcmp(current_style.modes, s.modes) != 0) {
    char old_modes[10];
    strcpy(old_modes, current_style.modes);

    char new_modes[10] = "";

    for (uint8_t i = 0; i < strlen(s.modes); i++) {
      bool mode_already_set = false;
      for (uint8_t j = 0; j < strlen(old_modes); j++) {
        if (s.modes[i] == old_modes[j]) {
          remove_from_index(old_modes, j);
          mode_already_set = true;
          break;
        }
      }

      if (!mode_already_set) {
        append_char(new_modes, s.modes[i]);
      }
    }

    // TODO if modes are sorted this check is redundant
    if (strlen(old_modes) > 0 || strlen(new_modes) > 0) {
      if (strlen(output_modes) > 0) {
        append_char(output_modes, ';');
      }

      for (uint8_t i = 0; i < strlen(old_modes); i++) {
        old_modes[i] = unset_mode(old_modes[i]);
      }

      strcat(output_modes, new_modes);
      strcat(output_modes, old_modes);
    }
  }

  printf("\033[%sm", output_modes);

  fprintf(stderr, "ESC[%sm\n", output_modes);

  current_style = s;
}

/////////////////////////////
// Frame Buffer Management //
/////////////////////////////

void switch_frame_buffers(void) {
  struct Display **temp = previous_frame_buffer;
  previous_frame_buffer = next_frame_buffer;
  next_frame_buffer = temp;
}

void clear_frame_buffer(struct Display **buffer, unsigned int rows,
                        unsigned int cols) {
  struct Display empty = {" ", default_style()};
  for (unsigned int row = 0; row < rows; row++) {
    for (unsigned int col = 0; col < cols; col++) {
      buffer[row][col] = empty;
    }
  }
}

struct Display **init_frame_buffer(unsigned int rows, unsigned int cols) {
  struct Display **buffer = malloc(rows * sizeof(struct Display *));
  for (unsigned int i = 0; i < rows; i++) {
    buffer[i] = malloc(cols * sizeof(struct Display));
  }

  clear_frame_buffer(buffer, rows, cols);
  return buffer;
}

void free_frame_buffer(struct Display **buffer, unsigned int rows) {
  for (unsigned int i = 0; i < rows; i++) {
    free(buffer[i]);
  }

  free(buffer);
}

void free_frame_buffers(void) {
  free_frame_buffer(previous_frame_buffer, screen_size_y);
  free_frame_buffer(next_frame_buffer, screen_size_y);
}

void init_frame_buffers(void) {
  previous_frame_buffer = init_frame_buffer(screen_size_y, screen_size_x);
  next_frame_buffer = init_frame_buffer(screen_size_y, screen_size_x);

  atexit(free_frame_buffers);
}
///////////////////////
// Render Functions ///
///////////////////////

void render_display(unsigned int x, unsigned int y, struct Display d) {
  move_cursor(x, y);
  set_style(d.style);
  printf("%s", d.character);
}

/////////////////
// Public Api ///
/////////////////

void init_terminalio(unsigned int *size_x, unsigned int *size_y) {
  printf("START");
  configure_terminal();
  clear_screen();
  set_screen_size();
  init_frame_buffers();
  current_style = default_style();

  *size_x = screen_size_x;
  *size_y = screen_size_y;
}

int draw_display(unsigned int x, unsigned int y, struct Display d) {
  if (x >= screen_size_x || y >= screen_size_y) {
    return -1;
  }

  next_frame_buffer[y][x] = d;
  return 0;
}

int draw_styled_string(int x, int y, struct Style style, char *format, ...) {
  char string[100];
  va_list args;
  va_start(args, format);
  vsprintf(string, format, args);
  va_end(args);

  int result;
  for (unsigned int i = 0; i < strlen(string); i++) {
    struct Display d;
    d.character[0] = string[i];
    d.character[1] = '\0';
    d.style = style;

    result = draw_display(x + i, y, d);
    if (result == -1) {
      return -1;
    }
  }
  return 0;
}

int draw_string(int x, int y, char *format, ...) {
  int result;
  va_list args;
  va_start(args, format);
  result = draw_styled_string(x, y, default_style(), format, args);
  va_end(args);
  return result;
}

void render_frame(void) {
  for (unsigned int y = 0; y < screen_size_y; y++) {
    for (unsigned int x = 0; x < screen_size_x; x++) {
      if (!display_equal(&previous_frame_buffer[y][x],
                         &next_frame_buffer[y][x])) {
        render_display(x, y, next_frame_buffer[y][x]);
      }
    }
  }

  clear_frame_buffer(previous_frame_buffer, screen_size_y, screen_size_x);
  switch_frame_buffers();

  fflush(stdout);
}

void read_input(char *buf, unsigned int buf_len) {
  ssize_t read_bytes = read(STDIN_FILENO, buf, (buf_len - 1) * sizeof(char));
  if (read_bytes == -1) {
    buf[0] = '\0';
  } else {
    buf[read_bytes] = '\0';
  }

  for (ssize_t i = 0; i < read_bytes; i++) {
    buf[i] = buf[i];
    buf[i + 1] = '\0';
  }
}
