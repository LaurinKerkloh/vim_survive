#include "../lib/terminalio.h"
#include <locale.h>
#include <unistd.h>

int main(void) {

  setlocale(LC_ALL, "");

  struct Display d = {
      "o", color_style(color_rgb(123, 225, 0), color_rgb(34, 123, 34))};

  unsigned int screen_size_x, screen_size_y;
  init_terminalio(&screen_size_x, &screen_size_y);

  for (unsigned int x = 0; x < screen_size_x; x++) {
    for (unsigned int y = 0; y < screen_size_y; y++) {
      draw_display(x, y, d);
    }
  }

  render_frame();

  sleep(3);
  return 0;
}
