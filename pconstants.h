#ifndef CONSTANTS_H
#define CONSTANTS_H

#ifdef _WIN32
    #include <direct.h>
    #define realpath(rel, abs) _fullpath(abs, rel, _MAX_PATH)
    // #define PATH_MAX _MAX_PATH
    #include <windows.h>
#else
    #include <limits.h>
    #include <unistd.h>
#endif
#include <stdint.h>

#define APP_NAME "pASM.c"
#define VERSION "1.0.0"
#define COPYRIGHT "© 2024 BeyerCorp"

#define MIN_MEMORY_SIZE ((uint32_t)1)
#define MAX_MEMORY_SIZE ((uint32_t)4294967295)  // 4mb * instruction_size
#define MIN_OPERAND_SIZE 1
#define MAX_OPERAND_SIZE 4 // max_instruction_size = 5
#define MIN_CACHE_BITS 1
#define MAX_CACHE_BITS 6 // Number of bits to use for the index (e.g., 4 bits for 16 entries)
#define MAX_CACHE_SIZE (1 << MAX_CACHE_BITS) // Total cache size based on MAX_CACHE_BITS
#define MAX_PROGRAM_SIZE ((uint64_t)21474836484) // 2GB or 2,048mb * instruction_size

#define MIN_READ_BUFFER_SIZE 512       // Minimum read buffer size (512 bytes)
#define MAX_READ_BUFFER_SIZE (4 * 1024 * 1024) // Maximum read buffer size (4 MB)

#define NOP 0x00
#define LDA_IMM 0x0A
#define LDA_DIR 0x0B
#define LDA_IND 0x0C
#define STA_DIR 0x14
#define STA_IND 0x15
#define ADD_DIR 0x1E
#define SUB_DIR 0x28
#define MUL_DIR 0x32
#define DIV_DIR 0x3C
#define JMP_DIR 0x46
#define JMP_IND 0x47
#define JNZ_DIR 0x50
#define JNZ_IND 0x51
#define JZE_DIR 0x5A
#define JZE_IND 0x5B
#define JLE_DIR 0x5C
#define JLE_IND 0x5D
#define STP 0x63

// Instruction set mapping
extern const char *INSTRUCTION_SET[];

// Bridge codes
#define IC_NOTHING 0
// Backend interrupt codes (Gui->Backend)
#define BIC_OPEN_FILE 1
// #define BIC_RELOAD_FILE 2
#define BIC_CLOSE_FILE 3
#define BIC_CHANGE_CACHE_BITS 4
#define BIC_START_STEP_BUTTON 5
#define BIC_RESET_BUTTON 6
#define BIC_SINGLE_STEP_MODE_TOGGLE 7
// #define BIC_RESET_ACKNOWLEDGE 8
// Gui interrupt codes (Backend->Gui)
#define GIC_RESET 1

#endif // CONSTANTS_H
