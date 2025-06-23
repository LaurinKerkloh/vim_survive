#include "terminalio.h"
#include <ctype.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#define OUTPUT_BUFFER_SIZE 1024 * 8

struct Display **next_frame_buffer, **previous_frame_buffer;
unsigned int screen_size_x, screen_size_y;

///////////////////////////////////
//    Terminal Configuration     //
///////////////////////////////////

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

///////////////////////////
// Terminal Interactions //
///////////////////////////
char current_display_modes[10] = "";

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
  printf("\033[1;1H");

  screen_size_x = 0;
  screen_size_y = 0;

  sscanf(in, "[%d;%d", &screen_size_y, &screen_size_x);

  set_non_blocking_input();
}

void clear_screen(void) { printf("\033[2J\033[1;1H"); }

void move_cursor(unsigned int x, unsigned int y) {
  printf("\033[%d;%dH", y + 1, x + 1);
}

void reset_display_modes(void) { printf("\033[0m"); }

int qsort_char_compare(const void *a, const void *b) {
  return (*(char *)a - *(char *)b);
}

void set_display_modes(char *modes) {
  qsort(modes, strlen(modes), sizeof(char), qsort_char_compare);
  if (strcmp(modes, current_display_modes) == 0) {
    return;
  }

  strncpy(current_display_modes, modes, sizeof(current_display_modes));

  reset_display_modes();

  printf("\033[");
  for (unsigned int i = 0; i < strlen(modes); i++) {
    printf("%d", modes[i]);
    if (i + 1 != strlen(modes)) {
      printf(";");
    }
  }
  printf("m");
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

  struct Display empty = {" ", ""};
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
  fprintf(stderr, "init_screen_x: %d, init_screen_y: %d\n", screen_size_x,
          screen_size_y);
  previous_frame_buffer = init_frame_buffer(screen_size_y, screen_size_x);
  next_frame_buffer = init_frame_buffer(screen_size_y, screen_size_x);

  atexit(free_frame_buffers);
}
///////////////////////
// Render Functions ///
///////////////////////
bool display_equal(struct Display this, struct Display other) {
  return (strcmp(this.character, other.character) == 0 &&
          strcmp(this.modes, other.modes) == 0);
}

void render_display(unsigned int x, unsigned int y, struct Display d) {
  fprintf(stderr, "draw: x:%d y:%d\n", x, y);
  fprintf(stderr, "char: '%s'\n", d.character);

  move_cursor(x, y);
  set_display_modes(d.modes);
  fprintf(stderr, "modes: ");
  for (unsigned int i = 0; i < strlen(current_display_modes); i++) {
    fprintf(stderr, "%d ", current_display_modes[i]);
  }
  printf("\n%s", d.character);
}

/////////////////
// Public Api ///
/////////////////

void init_renderer(unsigned int *size_x, unsigned int *size_y) {
  configure_terminal();
  clear_screen();
  set_screen_size();
  init_frame_buffers();

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

int draw_string(int x, int y, char *modes, char *format, ...) {
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
    strncpy(d.modes, modes, sizeof(d.modes));
    result = draw_display(x + i, y, d);
    if (result == -1) {
      return -1;
    }
  }
  return 0;
}

void render_frame(void) {
  for (unsigned int y = 0; y < screen_size_y; y++) {
    for (unsigned int x = 0; x < screen_size_x; x++) {
      if (!display_equal(previous_frame_buffer[y][x],
                         next_frame_buffer[y][x])) {
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
