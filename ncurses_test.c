#include <ncurses.h>

int main(void) {
  int rows, cols;

  // Initialize ncurses
  initscr();
  noecho();
  curs_set(FALSE);

  // Get screen size
  getmaxyx(stdscr, rows, cols);

  // Fill the screen with 'o'
  for (int y = 0; y < rows; y++) {
    for (int x = 0; x < cols; x++) {
      mvaddch(y, x, 'o');
    }
  }

  // Refresh to show changes
  refresh();

  // Wait for user input before exiting
  getch();

  // End ncurses mode
  endwin();

  return 0;
}
