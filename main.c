#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include "printing.h"
#include <unistd.h>

#define MEMORY_SIZE 2 ^ 16
#define EEPROM_SIZE 2 ^ 13

_Bool running;

double clock_rate = 1;
#define clock_period 1/clock_rate * 1E6

// -------------------
// Official Registers:
// ------------------

// AX, BX, CX, DX, SP, BP, SI, BI
__uint16_t registers[8];

// CS, IP, SS, DS
__uint16_t segments[4];

// -------------------
// Hidden Registers:
// ------------------

// ALU registers
__uint16_t alu_a;
__uint16_t alu_b;

__uint16_t mar;

// Instruction Registers
__uint16_t instruction_register;
__uint8_t u_instruction_register;
__uint8_t u_program_counter;
//
//// -------------------
//// ALU Control Bits:
//// ------------------
//
//__u_char alu_select;
//_Bool shift_left;
//_Bool shift_right;
//
//// -------------------
//// Official Register Control Bits:
//// ------------------
//
//__u_char register_select;
//_Bool register_enable;
//_Bool register_write;
//
//__u_char segment_select;
//_Bool segment_enable;
//_Bool segment_write;

__uint16_t memory[MEMORY_SIZE];
#define eeprom_index u_instruction_register << 7 | u_program_counter
__uint64_t eeprom[EEPROM_SIZE];

// -------------------
// Bus
// ------------------
_Bool bus_floating;
__uint16_t bus;


// -------------------
// Control Signal Masks
// ------------------
#define neg_uIP_W 1 << 0
#define neg_out_W 1 << 1
#define neg_mem_R 1 << 2
#define neg_mem_W 1 << 3
#define neg_mar_W 1 << 4
// 5: Empty
#define neg_seg_en 1 << 6
#define seg_W 1 << 7
#define neg_reg_en 1 << 8
#define reg_W 1 << 9
#define neg_flag_W 1 << 10
#define flag_sel_bus 1 << 11
#define neg_flag_R 1 << 12

#define neg_alu_b_W 1 << 13
#define neg_alu_a_W 1 << 14
#define shift_right 1 << 15
#define shift_left 1 << 16
#define alu_s 1 << 17 | 1 << 18 | 1 << 19
#define neg_alu_re 1 << 20
#define cin 1 << 21
#define addr_mode 1 << 22 | 1 << 23
#define reg_sel 1 << 24
#define neg_jmp_re 1 << 25
#define inta 1 << 26
#define neg_int_re 1 << 27
#define halt 1 << 28
// 29: Empty
// 30: Empty
#define neg_uIR_clear (__uint64_t)1 << 31
#define jsel (__uint64_t)1 << 32 | (__uint64_t)1 << 33 | (__uint64_t)1 << 34 | (__uint64_t)1 << 35
#define seg_sel (__uint64_t)1 << 36 | (__uint64_t)1 << 37
#define ir_W (__uint64_t)1 << 38

void empty_bus() {
  bus_floating = true;
  bus = 0;
}

void write_bus(__uint16_t val) {
  if (bus_floating) {
    bus = val;
    bus_floating = false;
  } else {
    error("Bus written twice in same tick!");
  }
}

__uint16_t read_bus() {
  if (bus_floating) {
    return 0;
  } else {
    return bus;
  }
}

void tick() {
  if (neg_uIP_W & eeprom[eeprom_index] == 0) {
    u_instruction_register = (__uint8_t )(instruction_register >> 10);
  }
  if (neg_out_W & eeprom[eeprom_index] == 0) {

  }
  if (neg_mem_R & eeprom[eeprom_index] == 0) {
    // something
  }
  if (neg_mem_W & eeprom[eeprom_index] == 0) {
    // Something
  }
  if (neg_mar_W & eeprom[eeprom_index] == 0) {
    mar = read_bus();
  }
  if (neg_seg_en & eeprom[eeprom_index] == 0) {
    int segment_index = (int)((seg_sel & eeprom[eeprom_index]) >> 36);
    if (seg_W & eeprom[eeprom_index]) {
      segments[segment_index] = read_bus();
    } else {
      write_bus(segments[segment_index]);
    }
  }
}

void inverted_tick() {

}

void randomize_registers() {
  for (int i = 0; i < 8; i++) {
    registers[i] = (__uint16_t) random();
  }
  for (int i = 0; i < 4; i++) {
    segments[i] = (__uint16_t) random();
  }
  alu_a = (__uint16_t) random();
  alu_b = (__uint16_t) random();
  mar = (__uint16_t) random();

  instruction_register = (__uint16_t) random();
  u_instruction_register = (__uint8_t) random();
}

int main() {
  init_screen();

  // Read EEPROM file
  char eeprom_filename[50];
  prompt("Hello", eeprom_filename);
  error(eeprom_filename);
  randomize_registers();
  double last_time;
  while (running) {
    last_time = (double) clock() / CLOCKS_PER_SEC;
    empty_bus();
    tick();
    inverted_tick();
    double current_time = (double) clock() / CLOCKS_PER_SEC;
    double elapsed_time = (current_time - last_time) / CLOCKS_PER_SEC;
    usleep((__useconds_t )(clock_period - elapsed_time * 1E6));
  }
  return 0;
}