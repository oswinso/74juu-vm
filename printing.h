#include <stdlib.h>

#ifndef VM_PRINTING_H
#define VM_PRINTING_H

void init_screen(void);

void prompt(char* msg, char* buf);

void error(char *msg);

void print_registers(__uint16_t registers[], __uint16_t segments[]);

#endif //VM_PRINTING_H
