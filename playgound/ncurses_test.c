#include <ncurses.h>
#include <unistd.h>

int main(void) {
  int rows, cols;

  printf("START");
  // Initialize ncurses
  initscr();
  noecho();
  curs_set(FALSE);

  // Get screen size
  getmaxyx(stdscr, rows, cols);

  // Fill the screen with 'o'
  for (int x = 0; x < cols; x++) {
    for (int y = 0; y < rows; y++) {
      mvaddch(y, x, 'o');
    }
  }

  // Refresh to show changes
  refresh();

  sleep(3);

  // End ncurses mode
  endwin();
  printf("END");
  return 0;
}
