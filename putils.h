#ifndef UTILS_H
#define UTILS_H

#include <stdbool.h>
#include <inttypes.h>
#include "CTools/treader.h"
// We only declare what is used outside

// *************************************************
// Cache
// *************************************************
typedef struct {
    uint64_t *entries; // Cache array, each entry holds an address and operand
    uint8_t size; // Number of entries in the cache
    uint8_t cache_bits;
} Cache;

Cache *create_cache(uint8_t cache_bits);
void reset_cache(Cache *cache);
void free_cache(Cache *cache);
bool will_overwrite_entry(Cache *cache, uint32_t address);
uint64_t find_in_cache(Cache *cache, uint32_t address);
uint64_t add_to_cache(Cache *cache, uint32_t address, uint32_t operand, bool as_dirty, bool *is_valid_result);
void writeback_cache_entry(Cache *cache, uint8_t *ram, uint64_t cache_entry, uint8_t instruction_size);
uint32_t get_u32_from_cache_or_ram(Cache *cache, uint8_t *ram, uint32_t address, uint8_t instruction_size);
void print_cache(Cache *cache);
Cache *duplicate_cache(const Cache *original);

// *************************************************
// Queue64
// *************************************************
typedef struct {
    uint64_t *data;  // Pointer to the queue data
    size_t size;     // Maximum size of the queue
    size_t front;    // Index of the front element
    size_t rear;     // Index of the rear element
    size_t count;    // Number of elements in the queue
} Queue64;

bool enqueue_with_bit(Queue64 *queue, uint64_t value, bool is_writeback);
bool dequeue_with_bit(Queue64 *queue, uint64_t *value, bool *is_writeback);
void init_queue(Queue64 *queue, size_t size);
bool enqueue(Queue64 *queue, uint64_t value);
bool dequeue(Queue64 *queue, uint64_t *value);
bool is_empty(const Queue64 *queue);
bool is_full(const Queue64 *queue);
void free_queue(Queue64 *queue);
void reset_queue(Queue64 *queue);

// *************************************************
// Specialized
// *************************************************
int32_t sign_extend_i32(int32_t num, uint8_t operand_size);
uint8_t *read_file(char *absolute_path, Cache *cache, uint64_t *outer_file_size, uint32_t *outer_memory_size, uint8_t *outer_operand_size);

// *************************************************
// Argument parsing & Interrupts
// *************************************************
typedef struct {
    const char *name;
    const char *shorthand;
    void *value;
    void (*converter)(const char *, void *);
    bool used;
} ParseableArgument;

void strtou32(const char *s, void *output);
void strtou8(const char *s, void *output);
void strtobool(const char *s, void *output);
void strtostr(const char *s, void *output);
int parse_arguments(int argc, char *argv[], ParseableArgument *arguments, int num_arguments);

/* Bridge Documentation
The upper 4 bits of the interrupt codes are not considered.
There can only ever be one element in the interrupt.
If the iterrupt was fully processed the code is cleared to 0 to signal that it can process more now.
backend_interrupt_code:
- XXXX 0000 => Nothing
- XXXX 0001 => open_file
- XXXX 0010 => reload_file
- XXXX 0011 => close_file
- XXXX 0100 => change_cache_bits
- XXXX 0101 => Start/Step button
- XXXX 0110 => Reset button
- XXXX 0111 => single_step_mode_toggle
- XXXX 1000 => reset_acknowledge
gui_interrupt_code:
- XXXX 0000 => Nothing
- XXXX 0001 => Reset (Load cache fully, backend will stay still, afterwards set the reset_acknowledge interrupt code)
- XXXX 0010 => 
- XXXX 0011 => 
- XXXX 0100 => 
- XXXX 0101 => 
- XXXX 0110 => 
- XXXX 0111 => 
 */
typedef struct { // 
    uint8_t backend_interrupt_code; // Gui->Backend
    uint8_t gui_interrupt_code; // Backend->Gui
    char *new_file_str; // Accompanies the open_file backend_interrupt_code
    uint8_t new_cache_bits; // Accompanies the change_cache_bits backend_interrupt_code
    // Used for per instruction updates
    int32_t *accumulator; // Only view
    uint8_t *instruction_size;
    uint32_t *instruction_counter;
    char *instruction; // Pointer to memory managed by backend & reused each iteration
    char *coinstruction; // Like in lda_dir
    char *cocoinstruction; // Like in lda_ind
    Queue64 *change_queue; // All changes queued
    bool *executing; // Backend currently executing
    bool *single_step_mode;
    // Used for reset updates
    Cache *sdata_cell_cache;
    uint8_t *sram; // View into the static ram copy (Not getting modified)
    uint32_t sram_size;

    mutex_t *mutex; // Who is allowed to modify it, read is always allowed
} Bridge;

void init_bridge(Bridge *gui_bridge, int32_t *accumulator, uint8_t *instruction_size, uint32_t *instruction_counter, char *instruction, char *coinstruction, char *cocoinstruction, bool *executing, 
                 bool *single_step_mode, Queue64 *change_queue, Cache *data_cell_cache, Cache *sdata_cell_cache, uint8_t *ram, uint8_t *sram, uint32_t sram_size);

// *************************************************
// Other
// *************************************************
void print_bits(uint64_t num, int bit_count);
bool ends_with(const char *str, const char *suffix);
double elapsed_time(struct timespec start, struct timespec end);
void print_buffer_in_hex(const void *buffer, size_t size);
void print_buffer(const void *buffer, size_t size);
bool is_in_set(char c, const char *set);
char *lstrip(char *str, const char *chars_to_strip);
const char *get_non_empty_string(const char *str, const char *default_text);

#endif // UTILS_H
