#include <stdio.h>
#include <stdlib.h>
#include "printing.h"
#include <curses.h>
#include <ctype.h>
#include <string.h>
#include "main.h"

#define registers_height 20
#define input_height 10

#define ERROR_PAIR 1

#define ctrl(x) ((x) & 0x1f)

const char register_names[8][4] = {"EAX", "EBX", "ECX", "EDX", "ESI", "EDI", "EBP", "ESP"};
const char segment_names[4][4] = {"IP", "DS", "SS", "CS"};

int cur_line = 0;
WINDOW *register_window;
WINDOW *output_window;
WINDOW *input_window;

int maxlines, maxcols;
struct input_line lnbuffer;
int last_command = 0;

struct input_line {
  char *ln;
  int length;
  int capacity;
  int cursor;
  int last_rendered;
};

int lines_read = 0;

void make_buffer(struct input_line *buf) {
  buf->ln = NULL;
  buf->length = 0;
  buf->capacity = 0;
  buf->cursor = 0;
  buf->last_rendered = 0;
}

void destroy_buffer(struct input_line *buf) {
  free(buf->ln);
  make_buffer(buf);
}

void render_line(struct input_line *buf) {
  int i = 0;
  for (; i < buf->length; i++) {
    chtype c = buf->ln[i];
    if (i == buf->cursor) {
      c |= A_REVERSE;
    }
    waddch(input_window, c);
  }
  if (buf->cursor == buf->length) {
    waddch(input_window, ' ' | A_REVERSE);
    ++i;
  }
  int rendered = i;
  // Erase previously rendered characters
  for (; i < buf->last_rendered; ++i) {
    waddch(input_window, ' ');
  }
  buf->last_rendered = rendered;
  wrefresh(input_window);
}

int retrieve_content(struct input_line *buf, char *target, int max_len) {
  int len = buf->length < (max_len - 1) ? buf->length : (max_len - 1);
  memcpy(target, buf->ln, (size_t) len);
  target[len] = '\0';
  buf->cursor = 0;
  buf->length = 0;
  return len + 1;
}

void add_char(struct input_line *buf, char ch) {
  // Ensure enough space for new char
  if (buf->length == buf->capacity) {
    int ncap = buf->capacity + 128;
    char *newln = (char *) realloc(buf->ln, ncap);
    if (!newln) {
      return;
    }
    buf->ln = newln;
    buf->capacity = ncap;
  }

  // Add new character
  memmove(&buf->ln[buf->cursor + 1], &buf->ln[buf->cursor], buf->length - buf->cursor);
  buf->ln[buf->cursor] = ch;
  ++buf->cursor;
  ++buf->length;
}


void error(char *msg) {
  wattron(output_window, A_BOLD | COLOR_PAIR(ERROR_PAIR));
  wmove(output_window, cur_line, 0);
  wprintw(output_window, "%s \n", msg);
  ++cur_line;
  wattroff(output_window, A_BOLD | COLOR_PAIR(ERROR_PAIR));
  mvwhline(output_window, LINES - registers_height - input_height-1, 0, ACS_HLINE, COLS);
  wrefresh(output_window);
}

void info(char *msg) {
  wmove(output_window, cur_line, 0);
  wprintw(output_window, "%s \n", msg);
  ++cur_line;
  mvwhline(output_window, LINES - registers_height - input_height-1, 0, ACS_HLINE, COLS);
  wrefresh(output_window);
}

void init_screen(void) {
  initscr();

  cbreak();             // Immediate key input
  nonl();               // Get return key
  timeout(0);           // Non-blocking input
  keypad(stdscr, TRUE); // Fix keypad
  noecho();             // No automatic printing
  curs_set(0);          // Hide cursor
  intrflush(stdscr, 0);
  intrflush(input_window, 0);
  leaveok(stdscr, 0);
  leaveok(input_window, 0);

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
  mvwhline(register_window, registers_height-1, 0, ACS_HLINE, COLS);
  output_window = newwin(LINES - registers_height - input_height, COLS, registers_height, 0);
  mvwhline(output_window, LINES - registers_height - input_height-1, 0, ACS_HLINE, COLS);
  input_window = newwin(input_height, COLS, LINES - input_height, 0);

//  mvwprintw(register_window, 0, 0, "EAX: 0");
  wrefresh(register_window);
  wmove(input_window, 0, 0);
  wrefresh(output_window);
  scrollok(output_window, true);

  // Setup input buffer
  make_buffer(&lnbuffer);
}

void destroy_screen(void) {
  destroy_buffer(&lnbuffer);
  delwin(register_window);
  delwin(output_window);
  delwin(input_window);
  delwin(stdscr);
  endwin();
  refresh();
}

int handle_keyboard(void) {
  char ln[1024];
  int code = 0;
  int len = get_key(&lnbuffer, ln, sizeof(ln));
  if (len != 0) { // If gotten a complete string
    if (strcmp(ln, "exit") == 0 || strcmp(ln, "quit") == 0) {
      code = VM_EXIT;
    }
    if (strcmp(ln, "r") == 0 || strcmp(ln, "run") == 0) {
      code = VM_RUN;
    }
    if (strcmp(ln, "p") == 0 || strcmp(ln, "pause") == 0) {
      code = VM_PAUSE;
    }
    if (strcmp(ln, "s") == 0 || strcmp(ln, "step") == 0) {
      code = VM_STEP;
    }
    if (len == 1) { // Zero length string
      code = last_command;
    }
    wmove(input_window, lines_read, 0);
    wclrtoeol(input_window);
    waddstr(input_window, ln);
    lines_read++;
    if (len == -1 || strcmp(ln, "clear") == 0 || strcmp(ln, "clr") == 0) {
      lines_read = 0;
      wclear(input_window);
      wrefresh(input_window);
      destroy_buffer(&lnbuffer);
    }
    last_command = code;
  }
  wmove(input_window, lines_read, 0);
  render_line(&lnbuffer);
  return code;
}

int get_key(struct input_line *buf, char *target, int max_len) {
  while (1) {
    int key = getch();
    if (key == ERR) {
      return 0;
    }
    int n = handle_input(buf, target, max_len, key);
    if (n != 0) {
      return n;
    }
  }
}

int handle_input(struct input_line *buf, char *target, int max_len, int key) {
  if (!(key & KEY_CODE_YES) && isprint(key)) {
    add_char(buf, key);
    return 0;
  }

  switch (key) {
    case ERR:
      break; // Nothing pressed
    case KEY_LEFT:
      if (buf->cursor > 0) { --buf->cursor; }
      break;
    case KEY_RIGHT:
      if (buf->cursor < buf->length) { ++buf->cursor; }
      break;
    case KEY_BACKSPACE:
    case 127:
    case 8:
      if (buf->cursor <= 0) {
        break;
      }
      buf->cursor--;
    case KEY_DC: // Fallthrough
      if (buf->cursor < buf->length) {
        memmove(&buf->ln[buf->cursor], &buf->ln[buf->cursor + 1], buf->length - buf->cursor - 1);
        buf->length--;
      }
      break;
    case ctrl('L'):
      return -1;
    case KEY_ENTER:
    case '\r':
    case '\n':
      return retrieve_content(buf, target, max_len);
    default:
      return 0;
  }
  return 0;
}

void control_bits_to_binary(__uint64_t control_bits, char *buf) {
  printf("Starting: ");
  for (int i = 40; i >= 0; i--) {
    char buf2[8];
    sprintf(buf2, "%c", '0' + (int)((control_bits >> i) & 1));
//    printf("(%2d) %lx - %s\n", i, control_bits, buf2);
    strcat(buf, buf2);
  }
//  printf("%s", buf);
//  printf("Done \n");
}

void print_registers(__uint16_t registers[], __uint16_t segments[], __uint16_t bus, bool bus_floating,
                     __uint8_t u_program_counter, __uint16_t alu_a,
                     __uint16_t alu_b, __uint64_t control_bits, __uint16_t mar, __uint16_t instruction_register,
                     __uint8_t u_instruction_register) {
  werase(register_window);
  for (int i = 0; i < 8; i++) {
    mvwprintw(register_window, i, 0, "%s: %x", register_names[i], registers[i]);
  }
  for (int i = 0; i < 4; i++) {
    mvwprintw(register_window, i, 12, "%s: %x", segment_names[i], segments[i]);
  }
  // Bus
  char *bus_text = bus_floating ? "Bus: %x (Floating)" : "Bus: %x";
  mvwprintw(register_window, 9, 0, bus_text, bus);

  // ALU
  mvwprintw(register_window, 11, 0, "A: %x    B: %x", alu_a, alu_b);

  // Control Logic
  int src_reg = ((instruction_register & 0x00E0) >> 5);
  int dst_reg = ((instruction_register & 0x0700) >> 8);
  int offset = instruction_register & 0x001F;
  int val8 = instruction_register & 0x00FF;
  int val11 = instruction_register & 0x03FF;
  mvwprintw(register_window, 13, 0, "uIR: %x    uPC: %d", u_instruction_register, u_program_counter);
  mvwprintw(register_window, 14, 0, "IR: %x    dst: %x    src: %x    off: %x   8: %x    11: %x",
      instruction_register, dst_reg, src_reg, offset, val8, val11);
  mvwprintw(register_window, 16, 0, "Current uInstruction: %lx", control_bits);

  char buf[64] = "";
  control_bits_to_binary(control_bits, buf);
  mvwprintw(register_window, 17, 0, buf);

  mvwhline(register_window, registers_height-1, 0, ACS_HLINE, COLS);
  wrefresh(register_window);
}
