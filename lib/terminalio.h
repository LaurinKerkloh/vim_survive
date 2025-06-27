#ifndef terminalio_h
#define terminalio_h
#include <stdbool.h>
#include <stdint.h>

//  OUTPUT MODES
// TODO: enum?
#define BOLD 1
#define DIM 2
#define ITALIC 3
#define UNDERLINE 4
#define BLINKING 5
#define INVERSE 7
#define HIDDEN 8
#define STRIKETHROUGH 9

//  COLOR
// TODO: enum?
#define BLACK 0
#define RED 1
#define GREEN 2
#define YELLOW 3
#define BLUE 4
#define MAGENTA 5
#define CYAN 6
#define WHITE 7

enum ColorType { DEFAULT, _8, _256, TRUE };

struct Color {
  enum ColorType type;
  uint8_t color;
  uint8_t red, green, blue;
};

struct Style {
  struct Color color, background;
  char modes[10];
};

struct Display {
  char character[5];
  struct Style style;
};

void init_terminalio(unsigned int *screen_size_x, unsigned int *screen_size_y);
int draw_display(unsigned int x, unsigned int y, struct Display d);
int draw_styled_string(int x, int y, struct Style style, char *format, ...);
int draw_string(int x, int y, char *format, ...);
void render_frame(void);

void read_input(char *buf, unsigned int buf_len);

struct Color default_color(void);
struct Color color_rgb(uint8_t r, uint8_t g, uint8_t b);
struct Color color_256(uint8_t color);
struct Color color_256_rgb(uint8_t r, uint8_t g, uint8_t b);
struct Color color_8(uint8_t color);

struct Style default_style(void);
struct Style color_style(struct Color color, struct Color background);
struct Style full_style(struct Color color, struct Color background,
                        unsigned int modes_count, ...);
void change_modes(struct Style *s, unsigned int modes_count, ...);
#endif
