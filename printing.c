#include <stdio.h>
#include <stdlib.h>
#include "printing.h"
#include <curses.h>

#define registers_height 20
#define input_height 5

#define ERROR_PAIR 1

int cur_line = 0;
WINDOW *register_window;
WINDOW *output_window;
WINDOW *input_window;

void error(char *msg) {
  wattron(output_window, A_BOLD | COLOR_PAIR(ERROR_PAIR));
  mvwprintw(output_window, cur_line, 0, "Error: %s", msg);
  ++cur_line;
  wattroff(output_window, A_BOLD | COLOR_PAIR(ERROR_PAIR));
  wrefresh(output_window);
}

void prompt(char *msg, char *buf) {
  mvwprintw(input_window, 0, 0, msg);
  wmove(input_window, 1, 0);

  wgetnstr(input_window, buf, 50);
}

int maxlines, maxcols;

void init_screen(void) {
  initscr();
  cbreak();
  keypad(stdscr, TRUE);

  maxlines = LINES - 1;
  maxcols = COLS - 1;

  if (has_colors() == FALSE) {
    endwin();
    printf("Your terminal doesn't support colors");
    exit(1);
  }
  start_color();
  use_default_colors();
  init_pair(ERROR_PAIR, COLOR_RED, -1);

  clear();
  refresh();
  register_window = newwin(registers_height, COLS, 0, 0);
  wborder(register_window, ' ', ' ', ' ', ACS_HLINE, ' ', ' ', ' ', ' ');
  output_window = newwin(LINES - registers_height - input_height, COLS, registers_height, 0);
  wborder(output_window, ' ', ' ', ' ', ACS_HLINE, ' ', ' ', ' ', ' ');
  input_window = newwin(input_height, COLS, LINES-input_height, 0);

  mvwprintw(register_window, 0, 0, "EAX: 0");
  wrefresh(register_window);
  wmove(input_window, 0, 0);
  wrefresh(output_window);
}

void print_registers(__uint16_t registers[], __uint16_t segments[]) {

}
