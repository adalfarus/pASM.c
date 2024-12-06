#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <windows.h>
#include <inttypes.h> // For PRIu32, etc.

#include "putils.h"
#include "gtkgui.h"
#include "pconstants.h"

// Dynamically attach to the parent console or suppress output
static void configure_console_output(uint64_t path_length) {
    // Attempt to attach to the parent process's console
    if (AttachConsole(ATTACH_PARENT_PROCESS)) {
        freopen("CONOUT$", "w", stdout); // Redirect stdout to parent console
        freopen("CONOUT$", "w", stderr); // Redirect stderr to parent console
        freopen("CONIN$", "r", stdin);  // Redirect stdin to parent console
    } else {
        // If no parent console exists, redirect output to NUL
        freopen("NUL", "w", stdout);
        freopen("NUL", "w", stderr);
    }
    
    // Here we do some tombfoolery to get the terminal to behave
    printf("\033[%uD", path_length);
    for (int i = 0; i < path_length; i++) {
        printf(" ");
    }
    printf("\033[%uD", path_length);
}

void on_close() {
    #ifdef ENABLE_DYNAMIC_CONSOLE
    char path[MAX_PATH] = {0}; // Buffer to store the current directory
    GetCurrentDirectory(MAX_PATH - 1, path);
    path[MAX_PATH - 1] = '\0'; // Explicitly null-terminate
    printf("%s>", path);
    fflush(stdout);
    fflush(stderr);
    #endif
}

int run_gui() {
    puts("Starting pASMc GUI...\n");

    // Start the GUI
    if (!gtkgui_start()) {
        fprintf(stderr, "Failed to start the GUI.\n");
        return EXIT_FAILURE;
    }

    puts("GUI is running. Press Ctrl+C to terminate.\n");

    // Wait indefinitely until the GUI stops
    while (true) {
        if (!gtkgui_running()) { 
            break;
        }
    }
    
    gtkgui_stop();

    puts("GUI has stopped. Exiting application.\n");
    return EXIT_SUCCESS;
}

int p_program(char *script_path, bool disable_gui, bool single_step_mode, 
              uint32_t overwrite_memory_size, uint8_t overwrite_operand_size, 
              char *input_file, uint8_t cache_bits) 
{
    // printf("disable_gui: %s\n", disable_gui ? "true" : "false");
    // printf("single_step_mode: %s\n", single_step_mode ? "true" : "false");
    // printf("overwrite_memory_size: %u\n", overwrite_memory_size);
    // printf("overwrite_operand_size: %u\n", overwrite_operand_size);
    // printf("cache_bits: %u\n", cache_bits);
    // printf("input_file: %s\n\n", input_file ? input_file : "(none)");

    if (!ends_with(input_file, ".p")) {
        fprintf(stderr, "Usage: %s [arguments] <file>.p\n", script_path);
        return EXIT_FAILURE;
    }
    char absolute_path[PATH_MAX];
    if (realpath(input_file, absolute_path) == NULL) {
        perror("realpath");
        return EXIT_FAILURE;
    }
    printf("Running: %s\n", absolute_path);

    /*
    uint32_t instruction_counter = 0;
    uint64_t program_counter = 0;
    int32_t accumulator = 0;
    uint32_t temp_u32;
    uint64_t temp_u64;
    int32_t temp_i32;
    uint8_t temp_u8;
    uint8_t op_code;
    uint32_t operand;
    bool running = true;
    bool is_valid_result;

    Bridge gui_bridge; // Will be here even without gui for easier integration
    init_bridge(&gui_bridge, );
    */

    Cache *data_cell_cache = create_cache(cache_bits);
    uint64_t file_size; // Make a unified memory_size that actually represent the size of ram and sram
    uint32_t memory_size;
    uint8_t operand_size;
    uint8_t *ram = read_file(absolute_path, data_cell_cache, &file_size, &memory_size, &operand_size);
    /*
    if (!disable_gui) { // Move all this logic into the event loop? 
        uint8_t *sram = malloc(file_size);
        memcpy(sram, ram, file_size);
        Bridge gui_bridge;
        init_bridge(&gui_bridge, 0, 0, 0, 0, data_cell_cache, ram, sram, file_size);
    }*/
    uint8_t instruction_size = 1 + operand_size;

    if (overwrite_memory_size > 0) {
        if (overwrite_memory_size > MAX_MEMORY_SIZE || overwrite_memory_size < MIN_MEMORY_SIZE) {
            printf("The memory size %u is not in range (%u:%u).", overwrite_memory_size, MIN_MEMORY_SIZE, MAX_MEMORY_SIZE);
            free_cache(data_cell_cache);
            exit(EXIT_FAILURE);
        }
        memory_size = overwrite_memory_size;
        file_size = memory_size * instruction_size;
        ram = realloc(ram, file_size);
    }
    if (overwrite_operand_size > 0) {
        if (overwrite_operand_size > MAX_OPERAND_SIZE || overwrite_operand_size < MIN_OPERAND_SIZE) {
            printf("The operant size %u is not in range (%u:%u).", overwrite_operand_size, MIN_OPERAND_SIZE, MAX_OPERAND_SIZE);
            free_cache(data_cell_cache);
            exit(EXIT_FAILURE);
        }
        operand_size = overwrite_operand_size;
    }
    print_buffer_in_hex(ram, file_size);
   
    uint32_t instruction_counter = 0;
    uint64_t program_counter = 0;
    int32_t accumulator = 0;
    uint32_t temp_u32;
    uint64_t temp_u64;
    int32_t temp_i32;
    uint8_t temp_u8;
    uint8_t op_code;
    uint32_t operand;
    bool running = true;
    bool is_valid_result;

    while (program_counter < file_size && running) {
        if (single_step_mode) {
            printf("\n");
            print_cache(data_cell_cache);
            print_buffer_in_hex(ram, file_size);
            printf("PC: %u\n", instruction_counter);
            printf("AKKU: %i\n", accumulator);
        }
        op_code = ram[program_counter++];
        instruction_counter++;
        if (op_code >= 10 && op_code <= 99) {
            if (program_counter + operand_size <= file_size) {
                operand = 0;
                memcpy(&operand, ram + program_counter, operand_size);
                program_counter += operand_size;
                // printf("%u u%d i%d\n", op_code, address_op, data_op);
            } else {
                fprintf(stderr, "Reached end of file during execution at %u.\n", instruction_counter);
                free(ram);
                free_cache(data_cell_cache);
                return EXIT_FAILURE;
            }
            switch (op_code) {
                case LDA_IMM:
                    accumulator = sign_extend_i32(operand, operand_size);
                    printf("[%u] LDA_IMM #%i\n", instruction_counter - 1, accumulator);
                    break;
                case LDA_DIR:
                    temp_i32 = (int32_t)get_u32_from_cache_or_ram(data_cell_cache, ram, operand, instruction_size);
                    accumulator = sign_extend_i32(temp_i32, operand_size);
                    printf("[%u] LDA_DIR %u (%i)\n", instruction_counter - 1, operand, accumulator);
                    break;
                case LDA_IND:
                    printf("[%u] LDA_IND %u", instruction_counter - 1, operand);
                    temp_u32 = get_u32_from_cache_or_ram(data_cell_cache, ram, operand, instruction_size); // First level: Load the indirect address
                    temp_i32 = (int32_t)get_u32_from_cache_or_ram(data_cell_cache, ram, temp_u32, instruction_size); // Second level: Load the value at the indirect address
                    accumulator = sign_extend_i32(temp_i32, operand_size); // Store the final value in the accumulator
                    printf(" (%i)\n", accumulator);
                    break;
                case STA_DIR:
                    // if address 0 writes back 0 we have a lot of trouble
                    temp_u64 = add_to_cache(data_cell_cache, operand, (uint32_t)accumulator, true, &is_valid_result);
                    if (is_valid_result) {
                        writeback_cache_entry(data_cell_cache, ram, temp_u64, instruction_size);
                    }
                    printf("[%u] STA_DIR %u\n", instruction_counter - 1, operand);
                    break;
                case STA_IND:
                    printf("[%u] STA_IND %u", instruction_counter - 1, operand);
                    temp_u32 = get_u32_from_cache_or_ram(data_cell_cache, ram, operand, instruction_size); // First level: Load the indirect address+
                    printf(" (%u)\n", temp_u32);
                    // if address 0 writes back 0 we have a lot of trouble
                    temp_u64 = add_to_cache(data_cell_cache, temp_u32, (uint32_t)accumulator, true, &is_valid_result);
                    if (is_valid_result) {
                        writeback_cache_entry(data_cell_cache, ram, temp_u64, instruction_size);
                    }
                    break;
                case ADD_DIR:
                    temp_i32 = sign_extend_i32((int32_t)get_u32_from_cache_or_ram(data_cell_cache, ram, operand, instruction_size), operand_size);
                    printf("[%u] ADD_DIR %u (%i)\n", instruction_counter - 1, operand, temp_i32);
                    accumulator += temp_i32, operand_size;
                    break;
                case SUB_DIR:
                    temp_i32 = sign_extend_i32((int32_t)get_u32_from_cache_or_ram(data_cell_cache, ram, operand, instruction_size), operand_size);
                    printf("[%u] SUB_DIR %u (%i)\n", instruction_counter - 1, operand, temp_i32);
                    accumulator -= temp_i32;
                    break;
                case MUL_DIR:
                    temp_i32 = sign_extend_i32((int32_t)get_u32_from_cache_or_ram(data_cell_cache, ram, operand, instruction_size), operand_size);
                    printf("[%u] MUL_DIR %u (%i)\n", instruction_counter - 1, operand, temp_i32);
                    accumulator *= temp_i32;
                    break;
                case DIV_DIR:
                    temp_i32 = sign_extend_i32((int32_t)get_u32_from_cache_or_ram(data_cell_cache, ram, operand, instruction_size), operand_size);
                    printf("[%u] DIV_DIR %u (%i)\n", instruction_counter - 1, operand, temp_i32);
                    accumulator /= temp_i32;
                    break;
                case JMP_DIR:
                    instruction_counter = operand;
                    program_counter = instruction_counter * instruction_size;
                    printf("[%u] JMP_DIR %u\n", instruction_counter - 1, operand);
                    break;
                case JMP_IND:
                    printf("[%u] JMP_IND %u", instruction_counter - 1, operand);
                    temp_u32 = get_u32_from_cache_or_ram(data_cell_cache, ram, operand, instruction_size);
                    printf(" (%i)\n", temp_u32);
                    instruction_counter = temp_u32;
                    program_counter = instruction_counter * instruction_size;
                    break;
                case JNZ_DIR:
                    if (accumulator != 0) {
                        instruction_counter = operand;
                        program_counter = instruction_counter * instruction_size;
                    }
                    printf("[%u] JNZ_DIR %u\n", instruction_counter - 1, operand);
                    break;
                case JNZ_IND:
                    printf("[%u] JNZ_IND %u", instruction_counter - 1, operand);
                    temp_u32 = get_u32_from_cache_or_ram(data_cell_cache, ram, operand, instruction_size);
                    printf(" (%i)\n", temp_u32);
                    if (accumulator != 0) {
                        instruction_counter = temp_u32;
                        program_counter = instruction_counter * instruction_size;
                    }
                    break;
                case JZE_DIR:
                    if (accumulator == 0) {
                        instruction_counter = operand;
                        program_counter = instruction_counter * instruction_size;
                    }
                    printf("[%u] JZE_DIR %u\n", instruction_counter - 1, operand);
                    break;
                case JZE_IND:
                    printf("[%u] JZE_IND %u", instruction_counter - 1, operand);
                    temp_u32 = get_u32_from_cache_or_ram(data_cell_cache, ram, operand, instruction_size);
                    printf(" (%i)\n", temp_u32);
                    if (accumulator == 0) {
                        instruction_counter = temp_u32;
                        program_counter = instruction_counter * instruction_size;
                    }
                    break;
                case JLE_DIR:
                    if (accumulator <= 0) {
                        instruction_counter = operand;
                        program_counter = instruction_counter * instruction_size;
                    }
                    printf("[%u] JLE_DIR %u\n", instruction_counter - 1, operand);
                    break;
                case JLE_IND:
                    printf("[%u] JLE_IND %u", instruction_counter - 1, operand);
                    temp_u32 = get_u32_from_cache_or_ram(data_cell_cache, ram, operand, instruction_size);
                    printf(" (%i)\n", temp_u32);
                    if (accumulator <= 0) {
                        instruction_counter = temp_u32;
                        program_counter = instruction_counter * instruction_size;
                    }
                    break;
                case STP:
                    running = false;
                    printf("[%u] STP\n", instruction_counter - 1);
                    break;
                default:
                    break;
            }
        } else {
            if (program_counter + operand_size <= file_size) {
                // program_counter += operand_size;
                // continue;
                fprintf(stderr, "Tried to execute unknown opcode (%u) at %u.\n", op_code, instruction_counter);
                free(ram);
                free_cache(data_cell_cache);
                return EXIT_FAILURE;
            } else {
                fprintf(stderr, "Reached end of file during execution at %u.\n", instruction_counter);
                free(ram);
                free_cache(data_cell_cache);
                return EXIT_FAILURE;
            }
        }
        if (single_step_mode) {
            getchar();
        }
    }

    print_cache(data_cell_cache);
    for (uint32_t index = 0; index < data_cell_cache->size; index++) {
        uint64_t entry = data_cell_cache->entries[index];

        // Extract the stored address
        uint32_t stored_address = (uint32_t)(entry >> (32 + data_cell_cache->cache_bits));
        uint32_t stored_operand = (uint32_t)entry;

        uint64_t actual_entry = ((uint64_t)((stored_address << data_cell_cache->cache_bits) | index) << 32) | stored_operand;
        writeback_cache_entry(data_cell_cache, ram, actual_entry, instruction_size);
    }
    reset_cache(data_cell_cache);

    // print_cache(data_cell_cache);
    // print_buffer_in_hex(ram, file_size);

    size_t ram_index = 0;
    while (ram_index < file_size) {
        // Ensure there is enough space for a full instruction
        if (ram_index + instruction_size > file_size) {
            fprintf(stderr, "Incomplete instruction at offset %zu. Skipping.\n", ram_index);
            break;
        }

        uint8_t opcode = ram[ram_index];
        uint32_t operand = 0;
        int32_t signed_operand = 0;

        if (opcode == 0) {
            // Opcode 0: Use sign-extended operand
            signed_operand = sign_extend_i32((uint32_t)ram[ram_index + 1], operand_size);
        } else {
            // Normal unsigned operand
            memcpy(&operand, ram + ram_index + 1, operand_size);
        }

        // Get instruction name
        const char *instruction_name = (opcode < 99) ? INSTRUCTION_SET[opcode] : "UNKNOWN";

        // Print instruction
        if (opcode == 0) {
            printf("Instruction: %-7s Operand: %i (Signed)\n", instruction_name, signed_operand);
        } else {
            printf("Instruction: %-7s Operand: %u (Unsigned)\n", instruction_name, operand);
        }

        // Move to the next instruction
        ram_index += instruction_size;
    }

    free(ram);
    free_cache(data_cell_cache);
    return EXIT_SUCCESS;
}

int main(int argc, char *argv[]) {
    #ifdef ENABLE_DYNAMIC_CONSOLE
    char path[MAX_PATH] = {0}; // Buffer to store the current directory
    GetCurrentDirectory(MAX_PATH - 1, path);
    path[MAX_PATH - 1] = '\0'; // Explicitly null-terminate
    configure_console_output((uint64_t)strlen(path) + 1);
    #endif

    if (atexit(on_close) != 0) {
        fprintf(stderr, "Failed to register on_close handler.\n");
        return EXIT_FAILURE;
    }

    bool disable_gui = false;
    bool single_step_mode = false;
    // bool debug = false;
    uint32_t overwrite_memory_size = 0;
    uint8_t overwrite_operand_size = 0;
    char input_file[MAX_PATH] = {0};
    uint8_t cache_bits = 4;

    bool lt_run_gui = false; // Temp argument
    ParseableArgument arguments[] = {
        {"lt-run-gui", "ltrg", &lt_run_gui, strtobool, false}, // Temp argument
        {"disable-gui", "ng", &disable_gui, strtobool, false}, 
        {"singlestep", "s", &single_step_mode, strtobool, false},
        {"overwrite-memory-size=", "m=", &overwrite_memory_size, strtou32, false},
        {"overwrite-operand-size=", "o=", &overwrite_operand_size, strtou8, false},
        {"cache-bits=", "c=", &cache_bits, strtou8, false},
        // {"debug", "d", &debug}
        {"", "", &input_file, strtostr, false}, // Positional argument
    };
    int num_arguments = sizeof(arguments) / sizeof(ParseableArgument);
    int exit_code = parse_arguments(argc, argv, arguments, num_arguments);

    if (exit_code == EXIT_FAILURE) {
        exit(EXIT_FAILURE);
    }

    if (lt_run_gui) {
        exit_code = run_gui();
    } else {
        exit_code = p_program(argv[0], disable_gui, single_step_mode, overwrite_memory_size, overwrite_operand_size, input_file, cache_bits);
    }
    return exit_code;
}
