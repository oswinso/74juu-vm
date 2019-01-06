#include <stdlib.h>
#include <stdbool.h>

#ifndef VM_PRINTING_H
#define VM_PRINTING_H

void init_screen(void);

void destroy_screen(void);

struct input_line;

void error(char *msg);

void info(char *msg);

void print_registers(__uint16_t registers[], __uint16_t segments[], __uint16_t bus, bool bus_floating,
                     __uint8_t u_program_counter, __uint16_t alu_a,
                     __uint16_t alu_b, __uint64_t control_bits, __uint16_t mar, __uint16_t instruction_register,
                     __uint8_t u_instruction_register);

int get_key(struct input_line *buf, char *target, int max_len);

int handle_keyboard(void);

int handle_input(struct input_line *buf, char *target, int max_len, int key);

#endif //VM_PRINTING_H
