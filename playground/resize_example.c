#include <signal.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>

void get_and_print_screen_size(void) {
  struct winsize w;
  ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);

  printf("lines %d\n", w.ws_row);
  printf("columns %d\n", w.ws_col);
}

void resize_signal(int sig) { get_and_print_screen_size(); }

int main(void) {

  signal(SIGWINCH, resize_signal);
  get_and_print_screen_size();

  sleep(5);
  return 0;
}
