#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include "printing.h"
#include <unistd.h>
#include <math.h>
#include <curses.h>
#include <string.h>
#include "main.h"

#define MEMORY_SIZE 1 << 16
#define EEPROM_SIZE 1 << 13

typedef uint8_t u_char;

_Bool running = true;

double clock_rate = 1;
#define clock_period 1/clock_rate * 1E6
#define print_state() print_registers(registers, segments, bus, bus_floating, u_program_counter, alu_a, alu_b,\
current_uInstruction, mar, instruction_register, u_instruction_register)

const char EEPROM_HEADER[] = {0x37, 0x34, 0x6A, 0x75, 0x75, 0x78, 0x78, 0x78};
const char EEPROM_FOOTER[] = {0x6C, 0x6D, 0x61, 0x6F, 0x66, 0x74, 0x65, 0x72};

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
#define current_uInstruction eeprom[eeprom_index]
#define ir_src_reg ((instruction_register & 0x00E0) >> 5)
#define ir_dst_reg ((instruction_register & 0x0700) >> 8)

#define neg_uIP_W ((__uint64_t) 1 << 0)
#define neg_out_W ((__uint64_t) 1 << 1)
#define neg_mem_R ((__uint64_t) 1 << 2)
#define neg_mem_W ((__uint64_t) 1 << 3)
#define neg_mar_W ((__uint64_t) 1 << 4)
// 5: Empty
#define neg_seg_en ((__uint64_t) 1 << 6)
#define seg_W ((__uint64_t) 1 << 7)
#define neg_reg_en ((__uint64_t) 1 << 8)
#define reg_W ((__uint64_t) 1 << 9)
#define neg_flag_W ((__uint64_t) 1 << 10)
#define flag_sel_bus ((__uint64_t) 1 << 11)
#define neg_flag_R ((__uint64_t) 1 << 12)

#define neg_alu_b_W ((__uint64_t) 1 << 13)
#define neg_alu_a_W ((__uint64_t) 1 << 14)
#define shift_right ((__uint64_t) 1 << 15)
#define shift_left ((__uint64_t) 1 << 16)
#define alu_s ((__uint64_t) (1 << 17 | 1 << 18 | 1 << 19))
#define neg_alu_re ((__uint64_t) 1 << 20)
#define cin ((__uint64_t) 1 << 21)
#define addr_mode ((__uint64_t) (1 << 22 | 1 << 23))
#define reg_sel ((__uint64_t) 1 << 24)
#define neg_jmp_re ((__uint64_t) 1 << 25)
#define inta ((__uint64_t) 1 << 26)
#define neg_int_re ((__uint64_t) 1 << 27)
#define halt ((__uint64_t) 1 << 28)
#define neg_decode_R (1 << 29)
// 30: Empty
#define neg_uPC_clear ((__uint64_t) 1 << 31)
#define jsel ((__uint64_t) 1 << 32 | (__uint64_t) 1 << 33 | (__uint64_t) 1 << 34 | (__uint64_t) 1 << 35)
#define seg_sel ((__uint64_t) 1 << 36 | (__uint64_t) 1 << 37)
#define ir_W ((__uint64_t) 1 << 38)

const __uint64_t do_nothing_bits =
    neg_uIP_W + neg_out_W + neg_mem_R + neg_mem_W + neg_mar_W + neg_seg_en + neg_reg_en + neg_flag_W + neg_alu_b_W +
    neg_alu_a_W + neg_alu_re + neg_jmp_re + neg_int_re + neg_decode_R + neg_uPC_clear;

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

__uint16_t get_alu_result(u_char operation, bool shl, bool shr, bool carry) {
  __uint16_t result = 0;
  switch (operation) {
    case 0:
      result = 0;
      break;
    case 1:
      result = alu_a + !alu_b;
      break;
    case 2:
      result = !alu_a + alu_b;
      break;
    case 3:
      result = alu_a + alu_b;
      break;
    case 4:
      result = alu_a ^ alu_b;
      break;
    case 5:
      result = alu_a | alu_b;
      break;
    case 6:
      result = alu_a & alu_b;
      break;
    case 7:
      result = 0xFFFF;
      break;
    default:
      error("Expected operation to be between 0 and 7");
      break;
  }
  if (carry) {
    result += 1;
  }
  if (shl) {
    result = result << 1;
  }
  if (shr) {
    result = result >> 1;
  }
  if (shl && shr) {
    error("Both shift_left and shift_right set");
  }
  return result;
}

void tick() {
  char buf[256] = "Tick: ";
  if ((neg_uIP_W & current_uInstruction) == 0) {
    strcat(buf, "[write uIP]");
    u_instruction_register = (__uint8_t) (instruction_register >> 10);
  }
  if ((neg_out_W & current_uInstruction) == 0) {
  }
  if ((neg_mem_W & current_uInstruction) == 0) {
    // Something
  }
  if ((neg_mar_W & current_uInstruction) == 0) {
    strcat(buf, "[mar W]");
    mar = read_bus();
  }
  if ((neg_seg_en & current_uInstruction) == 0) {
    int segment_index = (int) ((seg_sel & current_uInstruction) >> 36);
    if (seg_W & current_uInstruction) {
      segments[segment_index] = read_bus();
    } else {
      write_bus(segments[segment_index]);
    }
  }
  // TODO: Check order of reg_sel
  if ((neg_reg_en & current_uInstruction) == 0) {
    if (reg_W & current_uInstruction) {
      if ((reg_sel & current_uInstruction) != 0) {
        registers[ir_src_reg] = read_bus();
      } else {
        registers[ir_dst_reg] = read_bus();
      }
    }
  }
  if ((neg_flag_W & current_uInstruction) == 0) {
    // TODO: implement
  }
  if ((neg_alu_a_W & current_uInstruction) == 0) {
    strcat(buf, "[ALU A W]");
    alu_a = read_bus();
  }
  if ((neg_alu_b_W & current_uInstruction) == 0) {
    strcat(buf, "[ALU B W]");
    alu_b = read_bus();
  }
  if ((neg_uPC_clear & current_uInstruction) == 0) {
    strcat(buf, "[PC CLR]");
    u_program_counter = 0;
  }
  if (ir_W & current_uInstruction) {
    strcat(buf, "[IR W]");
    instruction_register = read_bus();
  }
  if (halt & current_uInstruction) {
    strcat(buf, "[Halt]");
    running = false;
  }
  info(buf);
}

void non_tick() {
  char buf[256] = "Non Clocked: ";
  if ((neg_mem_R & current_uInstruction) == 0) {
    // TODO: fill in
  }
  if ((neg_seg_en & current_uInstruction) == 0) {
    int segment_index = (int) ((seg_sel & current_uInstruction) >> 36);
    if (!(seg_W & current_uInstruction)) {
      write_bus(segments[segment_index]);
    }
  }
  // TODO: Check order of reg_sel
  if ((neg_reg_en & current_uInstruction) == 0) {
    if (!(reg_W & current_uInstruction)) {
      if ((reg_sel & current_uInstruction) != 0) {
        write_bus(registers[ir_src_reg]);
      } else {
        write_bus(registers[ir_dst_reg]);
      }
    }
  }
  if ((neg_flag_R & current_uInstruction) == 0) {
    // TODO: Check order of flags
  }
  if ((neg_alu_re & current_uInstruction) == 0) {
    u_char alu_operation = (u_char) ((alu_s & current_uInstruction) >> 17);
    bool shr = (bool) ((shift_left & current_uInstruction) >> 15);
    bool shl = (bool) ((shift_left & current_uInstruction) >> 16);
    bool carry_in = (bool) ((cin & current_uInstruction) >> 21);
    write_bus(get_alu_result(alu_operation, shl, shr, carry_in));
  }
  if ((neg_jmp_re & current_uInstruction) == 0) {
    // TODO: implement
  }
  if ((neg_int_re & current_uInstruction) == 0) {
    // TODO: implement
  }
  if ((neg_decode_R & current_uInstruction) == 0) {
    // TODO: add in interrupts
    u_instruction_register = (__uint8_t) (instruction_register >> 11);
  }
  if (inta & current_uInstruction) {
    // TODO: implement
  }
  info(buf);
}

void inverted_tick() {
  u_program_counter++;
}

void randomize_registers() {
  char buf[50];
  srandom((unsigned int) time(NULL));
  for (int i = 0; i < 8; i++) {
    registers[i] = (__uint16_t) random();
    sprintf(buf, "reg %d: %x", i, registers[i]);
    info(buf);
  }
  for (int i = 0; i < 4; i++) {
    segments[i] = (__uint16_t) random();
    sprintf(buf, "seg %d: %x", i, segments[i]);
    info(buf);
  }
  alu_a = (__uint16_t) random();
  sprintf(buf, "a: %x", alu_a);
  info(buf);
  alu_b = (__uint16_t) random();
  sprintf(buf, "b: %x", alu_b);
  info(buf);
  mar = (__uint16_t) random();

//  instruction_register = (__uint16_t) random();
//  u_instruction_register = (__uint8_t) random();
}


int parseEEPROM(unsigned char *eeprom_buffer, size_t buffer_size, __uint64_t eeprom[]) {
  printf("%c %c %c %c %c %c %c %c %c %c\n", eeprom_buffer[0], eeprom_buffer[1], eeprom_buffer[2], eeprom_buffer[3],
         eeprom_buffer[4], eeprom_buffer[5], eeprom_buffer[6], eeprom_buffer[7], eeprom_buffer[8], eeprom_buffer[9]);
  if (memcmp(eeprom_buffer, EEPROM_HEADER, sizeof(EEPROM_HEADER)) != EXIT_SUCCESS) {
    printf("Failed 1");
    return EXIT_FAILURE;
  }
  size_t footer_start = buffer_size - 8;
  if (memcmp(eeprom_buffer + footer_start, EEPROM_FOOTER, sizeof(EEPROM_FOOTER)) != EXIT_SUCCESS) {
    printf("Failed 2");
    return EXIT_FAILURE;
  }
  size_t total_instructions = (buffer_size - 16) / (40 / 8);
  printf("eeprom size: %ld\n", total_instructions);
  if (total_instructions != EEPROM_SIZE) {
    printf("wrong size");
    return EXIT_FAILURE;
  }
//  for (int i = 0; i < EEPROM_SIZE; ++i) {
  for (int i = 0; i < 5; ++i) {
    __uint64_t acc = 0;
    for (int j = 0; j < 5; ++j) {
      acc += ((__uint64_t) eeprom_buffer[sizeof(EEPROM_HEADER) + 5 * i + j]) << (j * 8);
//      printf("%lx\n", ((__uint64_t)eeprom_buffer[sizeof(EEPROM_HEADER) + 5*i + j]) << (j*8));
//      printf("%x\n", eeprom_buffer[sizeof(EEPROM_HEADER) + 5 * i + j]);
    }
//    printf("%lu\n", acc);
//    printf("%lx\n", acc);
    eeprom[i] = acc;
//    printf("%04x: %lx\n", i, acc);
  }
  printf("Succeeded!\n");
  return EXIT_SUCCESS;
}

int main(int argc, char *argv[]) {
  if (argc != 3) {
    printf("Usage: ./vm EEPROM_file Memory_file\n");
    return EXIT_FAILURE;
  }
  FILE *eeprom_file;
  unsigned char *eeprom_buffer;
  eeprom_file = fopen(argv[1], "rb");
  fseek(eeprom_file, 0, SEEK_END);
  size_t eeprom_len = (size_t) ftell(eeprom_file);
  rewind(eeprom_file);

  eeprom_buffer = (unsigned char *) malloc((eeprom_len + 1) * sizeof(char));
  fread(eeprom_buffer, eeprom_len, 1, eeprom_file);
  fclose(eeprom_file);

  if (parseEEPROM(eeprom_buffer, eeprom_len, eeprom) == EXIT_FAILURE) {
    printf("EEPROM file provided was invalid.");
    return EXIT_FAILURE;
  }
//  return EXIT_SUCCESS;

  init_screen();

//  error(eeprom_filename);
  randomize_registers();
//  for (int i = 0; i < EEPROM_SIZE; ++i) {
////    sprintf(eeprom_filename, "At: %d", i);
////    error(eeprom_filename);
//    eeprom[i] = do_nothing_bits;
//  }
  double last_time;
  int mode = PAUSED;
  while (running) {
    last_time = (double) clock() / CLOCKS_PER_SEC;

    // Input
    int code = handle_keyboard();
    if (code == VM_EXIT) {
      break;
    }
    if (code == VM_RUN) {
      mode = CONTINUOUS;
    }
    if (code == VM_PAUSE) {
      mode = PAUSED;
    }

    if (code == VM_STEP || mode == CONTINUOUS) {
      print_state();
      // Debug

      //
      empty_bus();

      non_tick();
      tick();
      inverted_tick();
      double current_time = (double) clock() / CLOCKS_PER_SEC;
      double elapsed_time = current_time - last_time;
//      __useconds_t sleep_time = (__useconds_t)(clock_period - elapsed_time);
//      sprintf(buf, "%f, %f, %f", current_time, elapsed_time, last_time);
//      usleep(sleep_time);
    }
  }
  print_state();
  if (!running) { // If halted, and not exit
    info("HALTED MUDAFUCKA (press any key to exit)");
    while (1) {
      int key = getch();
      if (key != ERR) {
        break;
      }
    }
  }
  free(eeprom_buffer);
  destroy_screen();
  return 0;
}
