#include <curses.h>
#include <stdlib.h>

int main(void)
{
  initscr();
  cbreak();
  noecho();

  clear();
  int maxlines = LINES - 1;
  int maxcols = COLS  - 1;
  mvprintw(maxlines, 0, "Init, maxlines: %d, maxcols: %d", maxlines, maxcols);
  refresh();
  getch();

  clear();
  mvaddstr(0,0, "0,0");
  mvaddstr(2,2, "2,2");
  mvaddstr(4,4, "4,4");
  mvaddstr(8,8, "8,8");
  mvaddstr(16,16, "16,16");
  refresh();
  getch();
  endwin();
}