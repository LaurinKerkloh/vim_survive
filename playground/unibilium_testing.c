#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unibilium.h>
#include <unistd.h>

unibi_term *ut;

bool set_unibi(void) {
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

  return true;
}

// alternate character set mode
// used to print special characters like lines/boxes and stuff
// function prints terminals
void list_acs_chars(void) {
  const char *acs_on = unibi_get_str(ut, unibi_enter_alt_charset_mode);
  const char *acs_off = unibi_get_str(ut, unibi_exit_alt_charset_mode);

  const char *acs_chars = unibi_get_str(ut, unibi_acs_chars);

  if (acs_on && acs_off && acs_chars) {
    for (unsigned int i = 0; i < strlen(acs_chars); i = i + 2) {
      printf("%c:", acs_chars[i]);
      printf("%s", acs_on);
      printf("%c", acs_chars[i]);
      printf("%s\n", acs_off);
    }
  } else {
    printf("acs mode not supported, or no list of available chars given");
  }
}

int main(void) {

  if (!set_unibi()) {
    return 1;
  }

  const char *enter_ca_mode = unibi_get_str(ut, unibi_enter_ca_mode);
  fputs(enter_ca_mode, stdout);

  const char *clear_screen = unibi_get_str(ut, unibi_clear_screen);
  fputs(clear_screen, stdout);

  const char *repeat_char = unibi_get_str(ut, unibi_repeat_char);

  fputs(repeat_char, stdout);

  fflush(stdout);
  sleep(10);

  const char *exit_ca_mode = unibi_get_str(ut, unibi_exit_ca_mode);
  fputs(exit_ca_mode, stdout);

  fflush(stdout);
  return 0;
}
