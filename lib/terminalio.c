#include "terminalio.h"
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unibilium.h>
#include <unistd.h>

#define OUTPUT_BUFFER_SIZE 1024 * 8

struct winsize winsize;

unsigned int buffer_rows, buffer_cols;
struct Display **next_frame_buffer, **previous_frame_buffer;
unsigned int screen_size_rows, screen_size_cols;
unibi_term *ut;

void append_char(char *string, char c) {
  unsigned int l = strlen(string);
  string[l] = c;
  string[l + 1] = '\0';
}

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
  if (!truecolor) {
    fprintf(stderr, "COLORTERM env not set.\n");
    return false;
  }

  return true;
}

void set_screen_size(void) {
  ioctl(STDOUT_FILENO, TIOCGWINSZ, &winsize);
  screen_size_rows = winsize.ws_row;
  screen_size_cols = winsize.ws_col;
}

void resize_signal(int signal) { set_screen_size(); }

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

  fputs(unibi_get_str(ut, unibi_exit_ca_mode), stdout);
  fputs(unibi_get_str(ut, unibi_cursor_normal), stdout);

  fprintf(stderr, "END");
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

  fputs(unibi_get_str(ut, unibi_enter_ca_mode), stdout);
  fputs(unibi_get_str(ut, unibi_cursor_invisible), stdout);

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

  if ((a->type == _8 || a->type == _256) && a->color == b->color) {
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

void log_color(struct Color c) {
  fprintf(stderr, "color:\n  type: %d\n", c.type);
  if (c.type == _256 || c.type == _8) {
    fprintf(stderr, "  color: %d\n", c.color);
  }
  if (c.type == TRUE) {
    fprintf(stderr, "  r g b: %d %d %d\n", c.red, c.green, c.blue);
  }
}

////////////
// Styles //
////////////

int qsort_output_mode_compare(const void *a, const void *b) {
  return (*(enum OutputMode *)a - *(enum OutputMode *)b);
}

char unset_mode(enum OutputMode mode) {
  if (mode == BOLD) {
    return 22;
  } else {
    return mode + 20;
  }
}

bool valid_mode(enum OutputMode mode) {
  switch (mode) {
  case BOLD:
  case DIM:
  case ITALIC:
  case UNDERLINE:
  case BLINKING:
  case INVERSE:
  case HIDDEN:
  case STRIKETHROUGH:
    return true;
  default:
    return false;
  }
}

struct ModesList empty_modes_list(void) {
  struct ModesList ml;
  ml.count = 0;
  return ml;
}

bool modes_equal(struct ModesList *a, struct ModesList *b) {
  if (a->count != b->count)
    return false;

  for (unsigned int i = 0; i < a->count; i++) {
    if (a->modes[i] != b->modes[i])
      return false;
  }
  return true;
}

bool style_equal(struct Style *a, struct Style *b) {
  return (color_equal(&a->color, &b->color) &&
          color_equal(&a->background, &b->background) &&
          modes_equal(&a->modes_list, &b->modes_list));
}

struct Style default_style(void) {
  struct Style s;
  s.color = default_color();
  s.background = default_color();
  s.modes_list = empty_modes_list();
  return s;
}

struct Style color_style(struct Color color, struct Color background) {
  struct Style s;
  s.color = color;
  s.background = background;
  s.modes_list = empty_modes_list();
  return s;
}

struct ModesList modes_list(unsigned int modes_count, va_list args) {
  struct ModesList ml;
  ml.count = 0;

  for (uint8_t i = 0; i < modes_count; i++) {
    enum OutputMode mode = va_arg(args, int);

    if (valid_mode(mode)) {
      bool duplicate = false;
      for (uint8_t x = 0; x < ml.count; x++) {
        if (ml.modes[x] == mode) {
          duplicate = true;
        }
      }

      if (!duplicate) {
        ml.modes[ml.count] = mode;
        ml.count++;
      }
    }
  }
  qsort(ml.modes, ml.count, sizeof(enum OutputMode), qsort_output_mode_compare);
  return ml;
}

void remove_mode(struct ModesList *ml, unsigned int index) {
  ml->count--;
  for (unsigned int i = index; i < ml->count; i++) {
    ml->modes[i] = ml->modes[i + 1];
  }
}

void add_mode(struct ModesList *ml, enum OutputMode mode) {
  ml->modes[ml->count] = mode;
  ml->count++;
}

void modes_string(char *string, struct ModesList *ml, bool set) {
  string[0] = '\0';
  for (unsigned int i = 0; i < ml->count; i++) {
    char tmp[3];

    if (set) {
      sprintf(tmp, "%d", ml->modes[i]);
    } else {
      sprintf(tmp, "%d", unset_mode(ml->modes[i]));
    }

    strcat(string, tmp);

    if (!(i == ml->count - 1)) {
      append_char(string, ';');
    }
  }
}

struct Style full_style(struct Color color, struct Color background,
                        unsigned int modes_count, ...) {
  struct Style s;
  s.color = color;
  s.background = background;

  va_list args;
  va_start(args, modes_count);
  s.modes_list = modes_list(modes_count, args);
  va_end(args);

  return s;
}

void change_modes(struct Style *s, unsigned int modes_count, ...) {
  va_list args;
  va_start(args, modes_count);
  s->modes_list = modes_list(modes_count, args);
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

unsigned int cursor_y;
unsigned int cursor_x;

void clear_screen(void) {
  fputs(unibi_get_str(ut, unibi_clear_screen), stdout);
  cursor_x = 0;
  cursor_y = 0;
}

void move_cursor(unsigned int x, unsigned int y) {
  if (x == cursor_x && y == cursor_y) {
    return;
  }
  // TODO: use unibilium string here? or is the ansi standard good enough?
  printf("\033[%d;%dH", y + 1, x + 1);

  cursor_x = x;
  cursor_y = y;
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

void set_style(struct Style s) {
  if (style_equal(&current_style, &s)) {
    return;
  }
  char output_string[200] = "";
  if (!color_equal(&current_style.color, &s.color)) {
    char new_color_string[40];
    color_string(new_color_string, &s.color, false);
    strcat(output_string, new_color_string);
  }

  if (!color_equal(&current_style.background, &s.background)) {
    char new_color_string[40];
    color_string(new_color_string, &s.background, true);
    if (strlen(output_string) > 0) {
      append_char(output_string, ';');
    }
    strcat(output_string, new_color_string);
  }

  if (!modes_equal(&current_style.modes_list, &s.modes_list)) {
    struct ModesList old_modes = current_style.modes_list;
    struct ModesList new_modes = empty_modes_list();

    for (uint8_t i = 0; i < s.modes_list.count; i++) {
      bool mode_already_set = false;
      for (uint8_t j = 0; j < old_modes.count; j++) {
        if (s.modes_list.modes[i] == old_modes.modes[j]) {
          remove_mode(&old_modes, j);
          mode_already_set = true;
          break;
        }
      }

      if (!mode_already_set) {
        add_mode(&new_modes, s.modes_list.modes[i]);
      }
    }

    char output_modes_string[20];

    if (new_modes.count > 0) {
      if (strlen(output_string) > 0) {
        append_char(output_string, ';');
      }
      modes_string(output_modes_string, &new_modes, true);
      strcat(output_string, output_modes_string);
    }

    if (old_modes.count > 0) {
      if (strlen(output_string) > 0) {
        append_char(output_string, ';');
      }
      modes_string(output_modes_string, &old_modes, false);
      strcat(output_string, output_modes_string);
    }
  }

  printf("\033[%sm", output_string);

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

// void resize_frame_buffer(struct Display **buffer, unsigned int current_rows,
//                          unsigned int current_cols, unsigned int
//                          change_rows_to, unsigned int change_cols_to) {
//
//   if (change_rows_to < current_rows) {
//     for (unsigned int i = change_rows_to - 1; i < current_rows; i++) {
//       free(buffer[i]);
//     }
//   }
//
//   if (change_rows_to != current_rows) {
//     buffer = realloc(buffer, change_rows_to * sizeof(struct Display *));
//   };
//
//   if (change_rows_to > current_rows) {
//     for (unsigned int i = current_rows - 1; i < change_rows_to; i++) {
//       buffer[i] = malloc(change_cols_to * sizeof(struct Display));
//     }
//   }
//
//   if (change_cols_to != current_cols) {
//     for (unsigned int i = 0; i < change_rows_to; i++) {
//       buffer[i] = realloc(buffer[i], change_cols_to * sizeof(struct
//       Display));
//     }
//   }
// }

void free_frame_buffers(void) {
  free_frame_buffer(previous_frame_buffer, buffer_rows);
  free_frame_buffer(next_frame_buffer, buffer_rows);
}

void init_frame_buffers(unsigned int rows, unsigned int cols) {
  previous_frame_buffer = init_frame_buffer(rows, cols);
  next_frame_buffer = init_frame_buffer(rows, cols);
  buffer_rows = rows;
  buffer_cols = cols;

  atexit(free_frame_buffers);
}

void resize_frame_buffers(unsigned int rows, unsigned int cols) {
  if (rows == buffer_rows && cols == buffer_cols) {
    return;
  }

  free_frame_buffers();
  init_frame_buffers(rows, cols);
  clear_screen();
}

///////////////////////
// Render Functions ///
///////////////////////

void render_display(unsigned int x, unsigned int y, struct Display d) {
  move_cursor(x, y);
  set_style(d.style);
  printf("%s", d.character);
  cursor_x++;
}

/////////////////
// Public Api ///
/////////////////

void init_terminalio(void) {
  fprintf(stderr, "START");
  if (!check_terminal_capabilities()) {
    exit(-1);
  }

  configure_terminal();

  clear_screen();

  set_screen_size();
  signal(SIGWINCH, resize_signal);

  init_frame_buffers(screen_size_rows, screen_size_cols);
  current_style = default_style();
}

int draw_display(unsigned int x, unsigned int y, struct Display d) {
  if (x >= buffer_cols || y >= buffer_rows) {
    return -1;
  }

  next_frame_buffer[y][x] = d;
  return 0;
}

int draw_sstring(int x, int y, struct Style style, char *format, ...) {
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
  result = draw_sstring(x, y, default_style(), format, args);
  va_end(args);
  return result;
}

void render_frame(void) {

  for (unsigned int row = 0; row < buffer_rows; row++) {
    for (unsigned int col = 0; col < buffer_cols; col++) {
      if (!display_equal(&previous_frame_buffer[row][col],
                         &next_frame_buffer[row][col])) {
        render_display(col, row, next_frame_buffer[row][col]);
      }
    }
  }

  clear_frame_buffer(previous_frame_buffer, buffer_rows, buffer_cols);
  switch_frame_buffers();

  resize_frame_buffers(screen_size_rows, screen_size_cols);

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

unsigned int get_max_x(void) { return buffer_cols; }
unsigned int get_max_y(void) { return buffer_rows; }

void get_max_xy(unsigned int *x, unsigned int *y) {
  *x = buffer_cols;
  *y = buffer_rows;
}
