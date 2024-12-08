#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>
#include <time.h>

#include "putils.h"
#include "pconstants.h"
#include "CTools/treader.h"

// *************************************************
// Cache
// *************************************************

Cache *create_cache(uint8_t cache_bits) {
    if (cache_bits < MIN_CACHE_BITS || cache_bits > MAX_CACHE_BITS) {
        fprintf(stderr, "Passed cache bits is not in range (%u:%u) %u.\n", MIN_CACHE_BITS, MAX_CACHE_BITS, cache_bits);
        exit(EXIT_FAILURE);
    }
    Cache *cache = malloc(sizeof(Cache));
    if (!cache) {
        perror("Failed to allocate memory for Cache");
        exit(EXIT_FAILURE);
    }
    cache->size = 1 << cache_bits;
    cache->cache_bits = cache_bits;
    cache->entries = malloc(cache->size * sizeof(uint64_t));
    if (!cache->entries) {
        perror("Failed to allocate memory for Cache entries");
        free(cache);
        exit(EXIT_FAILURE);
    }
    memset(cache->entries, 0, cache->size * sizeof(uint64_t)); // Initialize cache to zero
    return cache;
}

void reset_cache(Cache *cache) {
    memset(cache->entries, 0, cache->size * sizeof(uint64_t)); // Reset cache to zero
}

void free_cache(Cache *cache) {
    if (cache) {
        free(cache->entries);
        cache->entries = NULL; // Prevent double-free
        free(cache);
        cache = NULL; // Prevent further access
    }
}

uint8_t get_cache_index(uint8_t cache_bits, uint32_t address) {
    return (address & ((1 << cache_bits) - 1)); // Mask the lower cache_bits bits
}

bool will_overwrite_entry(Cache *cache, uint32_t address) {
    uint8_t index = get_cache_index(cache->cache_bits, address);
    uint32_t stored_address = (uint32_t)((cache->entries[index] >> 32 + cache->cache_bits) << cache->cache_bits); // Extract the stored address
    return cache->entries[index] != 0 && (stored_address | index) != address; // True if non-zero and different address
}

uint64_t find_in_cache(Cache *cache, uint32_t address) {
    uint8_t index = get_cache_index(cache->cache_bits, address);
    uint64_t entry = cache->entries[index];
    uint32_t stored_address = (uint32_t)(entry >> 32 + cache->cache_bits) << cache->cache_bits; // Extract the stored address
    if ((stored_address | index) == address) {
        return (uint64_t)(uint32_t)entry; // Cache hit (discard the address)
    }
    return UINT32_MAX + 1; // Cache miss
}

uint64_t add_to_cache(Cache *cache, uint32_t address, uint32_t operand, bool as_dirty, bool *is_valid_result) {
    uint8_t index = get_cache_index(cache->cache_bits, address);

    uint64_t old_entry = cache->entries[index];
    uint32_t remaining_address = (uint32_t)(old_entry >> 32 + cache->cache_bits);
    uint32_t stored_address = (remaining_address << cache->cache_bits) | index;
    uint32_t stored_operand = (uint32_t)(old_entry & 0xFFFFFFFF);
    bool is_dirty = old_entry & (1ULL << 32);

    if (stored_address == address && stored_operand == operand) {
        return 0; // No change, no overwrite
    }

    // If the addresses are different or the operand has changed, create a new entry
    uint64_t new_entry = (((uint64_t)address >> 1) << 33) | operand;
    if (stored_address == address) {
        // If the address matches but the operand changed, mark as dirty
        new_entry |= (1ULL << 32);
        is_dirty = false; // We don't want to write back yet
    } else if (as_dirty) {
        new_entry |= (1ULL << 32);
    }

    cache->entries[index] = new_entry;
    *is_valid_result = is_dirty;
    return is_dirty ? ((uint64_t)stored_address << 32) | stored_operand : 0;
}

void writeback_cache_entry(Cache *cache, uint8_t *ram, uint64_t cache_entry, uint8_t instruction_size) {
    if (!cache || !ram) {
        fprintf(stderr, "Error: Null cache or RAM pointer.\n");
        return;
    } else if (instruction_size < 2) {
        fprintf(stderr, "Error: Instruction size must be at least 2 (1 byte for opcode + operand).\n");
        return;
    }
    uint32_t address = (uint32_t)(cache_entry >> 32);
    uint32_t operand = (uint32_t)(cache_entry & 0xFFFFFFFF);

    uint64_t ram_index = address * instruction_size;
    // printf("Writing %u back to %u at idx %u\n", operand, address, ram_index);
    // int32_t temp = sign_extend_i32((uint32_t)ram[ram_index + 1], instruction_size - 1);
    // printf("Before: %i\n", temp);
    memcpy(ram + ram_index + 1, &operand, instruction_size - 1);
    // temp = sign_extend_i32((uint32_t)ram[ram_index + 1], instruction_size - 1);
    // printf("After: %i\n", temp);
}

uint32_t get_u32_from_cache_or_ram(Cache *cache, uint8_t *ram, uint32_t address, uint8_t instruction_size) {
    uint64_t cache_result = find_in_cache(cache, address);
    if (cache_result == UINT32_MAX + 1) {
        uint8_t opcode = (uint8_t)ram[address * instruction_size];
        if (opcode != 0) {
            fprintf(stderr, "\nTried to load non-data address at %u.\n", address);
            free_cache(cache);
            free(ram);
            exit(EXIT_FAILURE);
        }
        uint32_t operant = (uint32_t)ram[(address * instruction_size) + 1];
        // if address 0 writes back 0 we have a lot of trouble
        bool is_valid_result = false;
        cache_result = add_to_cache(cache, address, operant, false, &is_valid_result);
        if (is_valid_result) {
            writeback_cache_entry(cache, ram, cache_result, instruction_size);
        }
        return operant;
    } else {
        return (uint32_t)cache_result;
    }
}

void print_cache(Cache *cache) {
    if (!cache || !cache->entries) {
        printf("Cache is not initialized.\n");
        return;
    }
    printf("Cache Contents:\n{");
    for (uint8_t i = 0; i < cache->size; i++) {
        uint64_t entry = cache->entries[i];

        bool is_dirty = entry & (1ULL << 32);
        uint32_t stored_address = (uint32_t)(entry >> 32 + cache->cache_bits) << cache->cache_bits; // Extract the address
        int32_t operand = (int32_t)(entry & 0xFFFFFFFF); // Extract the operand
        printf("%d/%s: (%u, %d), ", i, is_dirty ? "1" : "0", stored_address | i, operand);
        if (i % 2 == 1) {
            printf("\n");
        }
    }
    printf("}\n");
}

Cache *duplicate_cache(const Cache *original) {
    if (!original) return NULL; // Handle NULL input

    // Allocate memory for the new Cache structure
    Cache *copy = malloc(sizeof(Cache));
    if (!copy) {
        perror("Failed to allocate memory for cache copy");
        return NULL;
    }

    // Copy primitive fields
    copy->size = original->size;
    copy->cache_bits = original->cache_bits;

    // Allocate memory for the entries array
    copy->entries = malloc(copy->size * sizeof(uint64_t));
    if (!copy->entries) {
        perror("Failed to allocate memory for cache entries");
        free(copy); // Free the structure itself before returning
        return NULL;
    }

    // Copy the entries array
    memcpy(copy->entries, original->entries, copy->size * sizeof(uint64_t));

    return copy;
}

// *************************************************
// Queue64
// *************************************************
void init_queue(Queue64 *queue, size_t size) {
    queue->data = malloc(size * sizeof(uint64_t));
    if (!queue->data) {
        perror("Failed to allocate memory for queue");
        exit(EXIT_FAILURE);
    }
    queue->size = size;
    queue->front = 0;
    queue->rear = 0;
    queue->count = 0;
}

bool enqueue(Queue64 *queue, uint64_t value) {
    if (queue->count == queue->size) {
        return false; // Queue is full
    }
    queue->data[queue->rear] = value;
    queue->rear = (queue->rear + 1) % queue->size;
    queue->count++;
    return true;
}

bool dequeue(Queue64 *queue, uint64_t *value) {
    if (queue->count == 0) {
        return false; // Queue is empty
    }
    *value = queue->data[queue->front];
    queue->front = (queue->front + 1) % queue->size;
    queue->count--;
    return true;
}

bool is_empty(const Queue64 *queue) {
    return queue->count == 0;
}

bool is_full(const Queue64 *queue) {
    return queue->count == queue->size;
}

void free_queue(Queue64 *queue) {
    free(queue->data);
    queue->data = NULL;
    queue->size = 0;
    queue->front = 0;
    queue->rear = 0;
    queue->count = 0;
}

void reset_queue(Queue64 *queue) {
    if (!queue || !queue->data) {
        perror("Queue is not initialized or has been freed.");
        return;
    }
    queue->front = 0;
    queue->rear = 0;
    queue->count = 0;
}

bool enqueue_with_bit(Queue64 *queue, uint64_t value, bool is_writeback) {
    if (queue->count == queue->size) {
        return false; // Queue is full
    }

    // Encode the extra bit into the value
    if (is_writeback) {
        value &= ~(1ULL << 63); // Clear MSB for writeback
    } else {
        value |= (1ULL << 63); // Set MSB for cache
    }

    queue->data[queue->rear] = value;
    queue->rear = (queue->rear + 1) % queue->size;
    queue->count++;
    return true;
}

bool dequeue_with_bit(Queue64 *queue, uint64_t *value, bool *is_writeback) {
    if (queue->count == 0) {
        return false; // Queue is empty
    }

    uint64_t encoded_value = queue->data[queue->front];

    // Decode the extra bit
    *is_writeback = ~(encoded_value >> 63) & 1;
    *value = encoded_value & ~(1ULL << 63); // Clear MSB to extract the value

    queue->front = (queue->front + 1) % queue->size;
    queue->count--;
    return true;
}

// *************************************************
// Other
// *************************************************
void print_bits(uint64_t num, int bit_count) {
    for (int i = bit_count - 1; i >= 0; i--) {
        printf("%d", (num >> i) & 1); // Extract and print the i-th bit
        if (i % 8 == 0 && i != 0) {
            puts(" ");
        }
    }
    puts("\n");
}

bool ends_with(const char *str, const char *suffix) {
    if (!str || !suffix) {
        return false; // Null strings can't match
    }

    size_t str_len = strlen(str);
    size_t suffix_len = strlen(suffix);

    if (suffix_len > str_len) {
        return false;
    }

    return strcmp(str + str_len - suffix_len, suffix) == 0;
}

double elapsed_time(struct timespec start, struct timespec end) {
    return (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
}

void print_buffer_in_hex(const void *buffer, size_t size) {
    const uint8_t *buf = (const uint8_t *)buffer; // Cast to byte pointer
    size_t i;

    for (i = 0; i < size; i++) {
        // Print each byte in hexadecimal format
        printf("%02X ", buf[i]);

        // Add a new line every 16 bytes for better readability
        if ((i + 1) % 16 == 0) {
            printf("\n");
        }
    }

    // Ensure the output ends with a newline
    if (i % 16 != 0) {
        printf("\n");
    }
}

void print_buffer(const void *buffer, size_t size) {
    const uint8_t *buf = (const uint8_t *)buffer; // Cast to byte pointer
    size_t i;

    for (i = 0; i < size; i++) {
        printf("%i ", buf[i]);

        // Add a new line every 16 bytes for better readability
        if ((i + 1) % 16 == 0) {
            printf("\n");
        }
    }

    // Ensure the output ends with a newline
    if (i % 16 != 0) {
        printf("\n");
    }
}

uint64_t max_u64(uint64_t num1, uint64_t num2) {
    if (num1 > num2) {return num1;} else {return num2;}
}

bool is_in_set(char c, const char *set) {
    while (*set) {
        if (c == *set) {
            return true;
        }
        set++;
    }
    return false;
}

char *lstrip(char *str, const char *chars_to_strip) {
    if (!str || !chars_to_strip) {return "";}

    char *start = str;
    while (*start && is_in_set(*start, chars_to_strip)) {
        start++;
    }

    char *result = malloc(strlen(start) + 1);
    if (!result) {
        perror("Failed to allocate memory");
        return NULL;
    }
    strcpy(result, start);
    return result;
}

const char *get_non_empty_string(const char *str, const char *default_text) {
    return (str && str[0] != '\0') ? str : default_text;
}

// *************************************************
// Specialized
// *************************************************
int32_t sign_extend_i32(int32_t num, uint8_t operand_size) {
    // If the operand is signed and operand_size < sizeof(int32_t),
    // sign-extend the value manually.
    if (operand_size < sizeof(int32_t) && (num & (1 << (8 * operand_size - 1)))) {
        num |= ~((1 << (8 * operand_size)) - 1); // Sign-extend the value
    }
    return num;
}

uint8_t *read_file(char *absolute_path, Cache *cache, uint64_t *outer_file_size, uint32_t *outer_memory_size, uint8_t *outer_operand_size) {
    FILE *p_file = fopen(absolute_path, "rb");
    if (!p_file) {
        fprintf(stderr, "Error opening file: %s\n", absolute_path);
        free_cache(cache);
        exit(EXIT_FAILURE);
    }

    // Read and validate the header
    char magic[5] = {0};
    fread(magic, 1, 4, p_file);
    if (strcmp(magic, "EMUL") != 0) {
        fprintf(stderr, "Invalid magic number: %s\n", magic);
        fclose(p_file);
        free_cache(cache);
        exit(EXIT_FAILURE);
    }
    uint8_t operand_size;
    uint32_t memory_size;
    fread(&operand_size, sizeof(uint8_t), 1, p_file);
    fread(&memory_size, sizeof(uint32_t), 1, p_file);
    if (operand_size > MAX_OPERAND_SIZE || operand_size < MIN_OPERAND_SIZE) {
        printf("The operant size %u is not in range (%u:%u).", operand_size, MIN_OPERAND_SIZE, MAX_OPERAND_SIZE);
        fclose(p_file);
        free_cache(cache);
        exit(EXIT_FAILURE);
    } else if (memory_size < MIN_MEMORY_SIZE || memory_size > MAX_MEMORY_SIZE) {
        printf("The memory size %u is not in range (%u:%u).", memory_size, MIN_MEMORY_SIZE, MAX_MEMORY_SIZE);
        fclose(p_file);
        free_cache(cache);
        exit(EXIT_FAILURE);
    }
    *outer_memory_size = memory_size;
    *outer_operand_size = operand_size;

    memory_size += 1;  // We want the actual size, not the last idx
    uint8_t instruction_size = 1 + operand_size;
    printf("\nValidatedHeader: Magic=%s, Operand Size=%uB, Memory Size=%uB\n", magic, operand_size, memory_size * instruction_size);
    uint8_t header_size = ftell(p_file);
    fseek(p_file, 0, SEEK_END);
    size_t file_size = ftell(p_file) - header_size;
    fseek(p_file, header_size, SEEK_SET);

    if (file_size > MAX_PROGRAM_SIZE * instruction_size) {
        fprintf(stderr, "File size exceeds maximum program size of %uB.\n", MAX_PROGRAM_SIZE * instruction_size);
        fclose(p_file);
        free_cache(cache);
        exit(EXIT_FAILURE);
    } else if (file_size > memory_size * instruction_size) {
        fprintf(stderr, "File size exceeds specified memory size of %uB.\n", memory_size * instruction_size);
        fclose(p_file);
        free_cache(cache);
        exit(EXIT_FAILURE);
    }

    uint8_t *ram = calloc(max_u64((uint64_t)file_size, (uint64_t)(memory_size * instruction_size)), 1);
    if (!ram) {
        perror("Failed to allocate RAM");
        fclose(p_file);
        free_cache(cache);
        exit(EXIT_FAILURE);
    }

    // Buffered reading of file
    uint32_t instruction_counter = 0;
    uint32_t temp_u32;
    int32_t temp_i32;
    uint8_t op_code;
    size_t bytes_read, remaining_buffer_size, bytes_to_read;
    size_t total_size = 0, buffer_pointer = 0;
    size_t buffer_size = 4096; // Start with a 4 KB buffer
    uint8_t *buffer = malloc(buffer_size);
    uint8_t *new_buffer;
    if (!buffer) {
        perror("Failed to allocate buffer");
        fclose(p_file);
        free_cache(cache);
        exit(EXIT_FAILURE);
    }
    struct timespec start, end;
    double io_time = 0, processing_time = 0;

    while (!feof(p_file)) {
        clock_gettime(CLOCK_MONOTONIC, &start);
        if (buffer_pointer != total_size) {
            printf("%u != %u\n", buffer_pointer, total_size);
            remaining_buffer_size = total_size - buffer_pointer;
            memmove(buffer, buffer + buffer_pointer, remaining_buffer_size);
        } else {remaining_buffer_size = 0;}
        bytes_to_read = buffer_size - remaining_buffer_size;
        bytes_read = fread(buffer + remaining_buffer_size, 1, bytes_to_read, p_file);
        clock_gettime(CLOCK_MONOTONIC, &end);
        io_time = elapsed_time(start, end);
        // printf("%f\n", io_time);

        if (bytes_read == 0) {
            if (ferror(p_file)) {
                perror("Error reading file");
                break;
            }
            // EOF reached
            break;
        }

        clock_gettime(CLOCK_MONOTONIC, &start);
        total_size = remaining_buffer_size + bytes_read;
        buffer_pointer = 0;
        while (buffer_pointer < total_size) {
            if (buffer_pointer + 1 > total_size) break; // Ensure opcode fits
            op_code = buffer[buffer_pointer++];
            if (buffer_pointer + operand_size > total_size) {
                buffer_pointer--;
                break;
            } // Ensure operand fits
            temp_u32 = 0;
            memcpy(&temp_u32, &buffer[buffer_pointer], operand_size);
            buffer_pointer += operand_size;
            // printf("u32>%u %d\n", op_code, temp_u32);

            if (op_code == 0) {
                temp_i32 = temp_u32;
                // If the operand is signed and operand_size < sizeof(int32_t),
                // sign-extend the value manually.
                if (operand_size < sizeof(int32_t) && (temp_i32 & (1 << (8 * operand_size - 1)))) {
                    temp_i32 |= ~((1 << (8 * operand_size)) - 1); // Sign-extend the value
                }
                // printf("i32>%u %d\n", op_code, temp_i32);
                if ((temp_i32 == 0 && !(will_overwrite_entry(cache, instruction_counter))) 
                    || temp_i32 != 0) {
                    bool _;
                    add_to_cache(cache, instruction_counter, temp_i32, false, &_);
                }
            }
            // Write instruction to RAM
            // printf("Writing %u %d\n", op_code, temp_u32);
            memcpy(ram + (instruction_counter * instruction_size), &op_code, 1);
            memcpy(ram + (instruction_counter * instruction_size) + 1, &temp_u32, operand_size);
            instruction_counter++;
            // print_file_in_hex(ram, file_size);
        }

        clock_gettime(CLOCK_MONOTONIC, &end);
        processing_time = elapsed_time(start, end);
        // printf("%f\n", processing_time);

        if (io_time > processing_time && buffer_size < MAX_READ_BUFFER_SIZE) {
            buffer_size = buffer_size * 2 > MAX_READ_BUFFER_SIZE ? MAX_READ_BUFFER_SIZE : buffer_size * 2;
            printf("Increasing buffer size to %zu bytes\n", buffer_size);
            uint8_t *new_buffer = realloc(buffer, buffer_size);
            if (!new_buffer) {
                perror("Failed to resize buffer");
                break;
            }
            buffer = new_buffer;
        } else if (processing_time > io_time && buffer_size > MIN_READ_BUFFER_SIZE) {
            buffer_size = buffer_size / 2 < MIN_READ_BUFFER_SIZE ? MIN_READ_BUFFER_SIZE : buffer_size / 2;
            printf("Decreasing buffer size to %zu bytes\n", buffer_size);
            uint8_t *new_buffer = realloc(buffer, buffer_size);
            if (!new_buffer) {
                perror("Failed to resize buffer");
                break;
            }
            buffer = new_buffer;
        }
    }

    if (buffer_pointer != total_size) {
        fprintf(stderr, "Reached end of file during loading at %u.\n", instruction_counter);
        free(buffer);
        fclose(p_file);
        free(ram);
        free_cache(cache);
        exit(EXIT_FAILURE);
    }
    free(buffer);
    fclose(p_file);
    printf("File loaded into RAM (%zu bytes).\n", file_size);
    *outer_file_size = file_size;
    return ram;
}

// *************************************************
// Argument parsing & Interrupts
// *************************************************

void strtou32(const char *s, void *output) {
    char *endptr;
    errno = 0;
    uintmax_t num = strtoumax(s, &endptr, 10);

    if (errno == ERANGE || num > UINT32_MAX) {
        fprintf(stderr, "Error: Couldn't parse '%s' to uint32_t (out of range).\n", s);
        exit(EXIT_FAILURE);
    } else if (*endptr != '\0') {
        fprintf(stderr, "Error: Invalid characters in '%s'.\n", s);
        exit(EXIT_FAILURE);
    }
    *(uint32_t *)output = (uint32_t)num; // Store the result
}

void strtou8(const char *s, void *output) {
    char *endptr;
    errno = 0;
    uintmax_t num = strtoumax(s, &endptr, 10);

    if (errno == ERANGE || num > UINT8_MAX) {
        fprintf(stderr, "Error: Couldn't parse '%s' to uint8_t (out of range).\n", s);
        exit(EXIT_FAILURE);
    } else if (*endptr != '\0') {
        fprintf(stderr, "Error: Invalid characters in '%s'.\n", s);
        exit(EXIT_FAILURE);
    }
    *(uint8_t *)output = (uint8_t)num; // Store the result
}

void strtobool(const char *str, void *output) {
    *(bool *)output = strcmp(str, "true") == 0 || strcmp(str, "1") == 0;
}

void strtostr(const char *s, void *output) {
    if (!s) {
        return; // Handle null input safely
    }
    memcpy((char *)output, s, strlen(s)); // Duplicate the string
}

int parse_arguments(int argc, char *argv[], ParseableArgument *arguments, int num_arguments) {
    for (int i = 1; i < argc; i++) {
        char *arg = argv[i];
        char *stripped_arg = lstrip(argv[i], "-");
        bool matched = false;

        for (int j = 0; j < num_arguments; j++) {
            const char *long_name = arguments[j].name;
            const char *short_name = arguments[j].shorthand;

            // Check if the argument matches the long or short name
            if ((long_name && strncmp(stripped_arg, long_name, strlen(long_name)) == 0) ||
                (short_name && strncmp(stripped_arg, short_name, strlen(short_name)) == 0)) {
                if (arguments[j].used) {
                    continue;}
                matched = true;
                arguments[j].used = true; // Mark the argument as used

                // Check if the argument ends with '='
                char *tmp_value = strchr(long_name, '=');
                char *real_value = strchr(stripped_arg, '=');
                char value_buffer[256] = {0}; // Adjust size as needed
                char *value = value_buffer;
                if (tmp_value) {
                    real_value++; // Skip the '=' character
                    value = real_value;
                } else if (long_name && strchr(long_name, '=') != NULL) {
                    // Argument requires a value but none is provided
                } else if (strchr(long_name, '=') == NULL && !real_value && strlen(long_name) != 0) {
                    // Argument does not require a value
                    char *loc = "true";
                    strcpy(value, loc);
                } else {
                    strcpy(value, arg);
                }

                // Call the conversion function and store the result
                if (arguments[j].converter) {
                    arguments[j].converter(value, arguments[j].value);
                }

                break; // Stop checking other arguments for this input
            }
        }

        if (!matched) {
            fprintf(stderr, "Unknown argument: %s\n", argv[i]);
            return EXIT_FAILURE;
        }
    }
    return EXIT_SUCCESS;
}

void init_bridge(Bridge *gui_bridge, int32_t *accumulator, uint8_t *instruction_size, uint32_t *instruction_counter, char *instruction, char *coinstruction, char *cocoinstruction, bool *executing, 
                 bool *single_step_mode, Queue64 *change_queue, Cache *data_cell_cache, Cache *sdata_cell_cache, uint8_t *ram, uint8_t *sram, uint32_t sram_size)
{
    gui_bridge->backend_interrupt_code = IC_NOTHING;
    gui_bridge->gui_interrupt_code = IC_NOTHING;
    gui_bridge->new_file_str = NULL;
    gui_bridge->new_cache_bits = 0;
    gui_bridge->accumulator = accumulator;
    gui_bridge->instruction_size = instruction_size;
    gui_bridge->instruction_counter = instruction_counter;
    gui_bridge->instruction = instruction;
    gui_bridge->coinstruction = coinstruction;
    gui_bridge->cocoinstruction = cocoinstruction;
    gui_bridge->executing = executing;
    gui_bridge->single_step_mode = single_step_mode;
    gui_bridge->change_queue = change_queue;
    gui_bridge->sdata_cell_cache = sdata_cell_cache;
    gui_bridge->sram = sram;
    gui_bridge->sram_size = sram_size;
    gui_bridge->mutex = malloc(sizeof(mutex_t));
    if (!gui_bridge->mutex) {
        fprintf(stderr, "Failed to allocate memory for gui_bridge mutex\n");
        free_cache(sdata_cell_cache);
        free_cache(data_cell_cache);
        free_queue(change_queue);
        free(sram);
        free(ram);
        exit(EXIT_FAILURE);
    }
    int result = mutex_init(gui_bridge->mutex);
    if (result != 0) {
        fprintf(stderr, "Creation of mutex for gui_bridge failed\n");
        free(gui_bridge->mutex);
        free_cache(sdata_cell_cache);
        free_cache(data_cell_cache);
        free_queue(change_queue);
        free(sram);
        free(ram);
        exit(EXIT_FAILURE);
    }
}
