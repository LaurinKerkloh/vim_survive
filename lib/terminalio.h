#ifndef terminalio_h
#define terminalio_h

struct Display {
  char character[5];
  char modes[10];
};

void init_terminalio(unsigned int *screen_size_x, unsigned int *screen_size_y);
int draw_display(unsigned int x, unsigned int y, struct Display d);
int draw_string(int x, int y, char *modes, char *format, ...);
void render_frame(void);
void read_input(char *buf, unsigned int buf_len);
#endif
