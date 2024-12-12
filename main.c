#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#define _WIN32_WINNT 0x0500
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
    printf("Starting pASMc GUI...\n");
    if (!gtkgui_start(NULL)) {
        fprintf(stderr, "Failed to start the GUI.\n");
        return EXIT_FAILURE;
    }
    printf("GUI is running. Press Ctrl+C to terminate.\n");
    while (true) { // Wait indefinitely until the GUI stops
        if (!gtkgui_running()) { 
            break;
        }
    }
    gtkgui_stop();
    printf("GUI has stopped. Exiting application.\n");
    return EXIT_SUCCESS;
}

int p_program(char *script_path, bool disable_gui, bool single_step_mode, 
              uint32_t overwrite_memory_size, uint8_t overwrite_operand_size, 
              char *input_file, uint8_t cache_bits, uint8_t queue_size, 
              bool immidiate_start, bool single_loop) 
{
    // printf("disable_gui: %s\n", disable_gui ? "true" : "false");
    // printf("single_step_mode: %s\n", single_step_mode ? "true" : "false");
    // printf("overwrite_memory_size: %u\n", overwrite_memory_size);
    // printf("overwrite_operand_size: %u\n", overwrite_operand_size);
    // printf("cache_bits: %u\n", cache_bits);
    // printf("input_file: %s\n\n", input_file ? input_file : "(none)");

    char instruction[20] = {0};
    char coinstruction[20] = {0};
    char cocoinstruction[20] = {0};
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
    bool executing = false;
    bool peek = false;
    bool is_valid_result;

    // Uninitialized vars
    uint64_t file_size;
    uint64_t ram_size;
    uint32_t memory_size;
    uint8_t operand_size;
    uint8_t instruction_size;
    uint8_t *ram = NULL, *sram = NULL, *temp_ram = NULL;

    Cache *data_cell_cache = create_cache(cache_bits);
    Cache *sdata_cell_cache = NULL;
    Queue64 change_queue;
    if (!disable_gui) {
        init_queue(&change_queue, queue_size);
    }

    Bridge gui_bridge; // Will be here even without gui for easier integration
    init_bridge(&gui_bridge, &accumulator, &instruction_size, &instruction_counter, instruction, coinstruction, cocoinstruction, 
                &executing, &single_step_mode, &change_queue, data_cell_cache, sdata_cell_cache, NULL, NULL, 0);
    
    // Set bridge code to open a file
    if (input_file[0] != '\0') {
        gui_bridge.backend_interrupt_code = BIC_OPEN_FILE;
        gui_bridge.new_file_str = input_file;
        if (immidiate_start) {
            executing = true;
        }
    }

    if (!disable_gui) {
        printf("Starting pASMc GUI...\n");
        if (!gtkgui_start(&gui_bridge)) {
            fprintf(stderr, "Failed to start the GUI.\n");
            return EXIT_FAILURE;
        }
        printf("GUI is running. Press Ctrl+C to terminate.\n");
    }

    // We run until we encounter the STP instruction
    // print_buffer_in_hex(ram, file_size);

    while (running) {
        if ((!disable_gui && !gtkgui_running()) || (!executing && single_loop)) { 
            break;
        }
        switch (gui_bridge.backend_interrupt_code) {
            case IC_NOTHING:
                break;
            case BIC_OPEN_FILE: // Same as BIC_RELOAD_FILE, but the gui doesn't reset the file to load
                if (data_cell_cache != NULL) {
                    reset_cache(data_cell_cache);
                } else {
                    data_cell_cache = create_cache(cache_bits);
                }
                if (sdata_cell_cache != NULL) {
                    free_cache(sdata_cell_cache);
                }
                if (ram != NULL) free(ram);
                if (sram != NULL) free(sram);
                
                if (!ends_with(gui_bridge.new_file_str, ".p")) {
                    fprintf(stderr, "Usage: %s [arguments] <file>.p\n", script_path);
                    mutex_lock(gui_bridge.mutex);
                    gui_bridge.backend_interrupt_code = IC_NOTHING;
                    mutex_unlock(gui_bridge.mutex);
                    break;
                }
                char absolute_path[PATH_MAX];
                if (realpath(gui_bridge.new_file_str, absolute_path) == NULL) {
                    perror("realpath");
                    return EXIT_FAILURE;
                }
                printf("Running: %s\n", absolute_path);
                ram = read_file(absolute_path, data_cell_cache, &file_size, &memory_size, &operand_size);
                ram_size = file_size;
                instruction_size = 1 + operand_size;
                instruction[0] = '\0';
                coinstruction[0] = '\0';
                cocoinstruction[0] = '\0';
                instruction_counter = 0;
                program_counter = 0;
                accumulator = 0;

                if (overwrite_memory_size > 0) {
                    if (overwrite_memory_size > MAX_MEMORY_SIZE || overwrite_memory_size < MIN_MEMORY_SIZE) {
                        printf("The memory size %u is not in range (%u:%u).", overwrite_memory_size, MIN_MEMORY_SIZE, MAX_MEMORY_SIZE);
                        free_cache(data_cell_cache);
                        // free(gui_bridge);
                        gtkgui_stop();
                        exit(EXIT_FAILURE);
                    }
                    memory_size = overwrite_memory_size;
                    ram_size = memory_size * instruction_size;
                    temp_ram = realloc(ram, file_size);
                    if (!temp_ram) {
                        perror("Failed to allocate ram");
                        free(ram);
                        free_cache(data_cell_cache);
                        // free(gui_bridge);
                        gtkgui_stop();
                        exit(EXIT_FAILURE);
                    }
                    ram = temp_ram;
                    temp_ram = NULL;
                    if (ram_size > file_size) {
                        memset(ram + file_size, 0, ram_size - file_size);
                    }
                }
                if (overwrite_operand_size > 0) {
                    if (overwrite_operand_size > MAX_OPERAND_SIZE || overwrite_operand_size < MIN_OPERAND_SIZE) {
                        printf("The operant size %u is not in range (%u:%u).", overwrite_operand_size, MIN_OPERAND_SIZE, MAX_OPERAND_SIZE);
                        free(ram);
                        free_cache(data_cell_cache);
                        // free(gui_bridge);
                        gtkgui_stop();
                        exit(EXIT_FAILURE);
                    }
                    operand_size = overwrite_operand_size;
                }

                mutex_lock(gui_bridge.mutex);
                sram = malloc(ram_size);
                if (!sram) {
                    perror("Failed to allocate sram");
                    free(ram);
                    free_cache(data_cell_cache);
                    exit(EXIT_FAILURE);
                }
                memcpy(sram, ram, ram_size);
                sdata_cell_cache = duplicate_cache(data_cell_cache);
                if (!sdata_cell_cache) {
                    perror("Failed to allocate sdata_cell_cache");
                    free(ram);
                    free(sram);
                    free_cache(data_cell_cache);
                    exit(EXIT_FAILURE);
                }

                gui_bridge.sdata_cell_cache = sdata_cell_cache;
                gui_bridge.sram = sram;
                gui_bridge.sram_size = ram_size;

                gui_bridge.backend_interrupt_code = IC_NOTHING;
                // gui_bridge.new_file_str = NULL;
                // Empty queue
                reset_queue(&change_queue);
                if (gui_bridge.gui_interrupt_code == IC_NOTHING) {
                    gui_bridge.gui_interrupt_code = GIC_RESET;
                } else if (!disable_gui) {
                    fprintf(stderr, "The gui has stopped execution or is in an error state\n");
                    exit(EXIT_FAILURE);
                }
                mutex_unlock(gui_bridge.mutex);
                break;
            case BIC_CLOSE_FILE:
                printf("Closing file\n");
                mutex_lock(gui_bridge.mutex);
                gui_bridge.new_file_str = NULL;
                file_size = 0;
                ram_size = 0;
                memory_size = 0;
                operand_size = 0;
                instruction_size = 0;
                if (ram != NULL) {
                    free(ram);
                    ram = NULL;
                }
                if (sram != NULL) {
                    free(sram);
                    sram = NULL;
                }
                temp_ram = NULL;
                instruction[0] = '\0';
                coinstruction[0] = '\0';
                cocoinstruction[0] = '\0';

                free_cache(data_cell_cache);
                if (sdata_cell_cache) {
                    free_cache(sdata_cell_cache);
                }

                data_cell_cache = create_cache(cache_bits);
                sdata_cell_cache = NULL;

                gui_bridge.sdata_cell_cache = sdata_cell_cache;
                gui_bridge.sram = sram;
                gui_bridge.sram_size = ram_size;

                // Empty queue
                reset_queue(&change_queue);
                gui_bridge.backend_interrupt_code = IC_NOTHING;
                if (gui_bridge.gui_interrupt_code == IC_NOTHING) {
                    gui_bridge.gui_interrupt_code = GIC_RESET;
                } else if (!disable_gui) {
                    fprintf(stderr, "The gui has stopped execution or is in an error state\n");
                    exit(EXIT_FAILURE);
                }
                mutex_unlock(gui_bridge.mutex);
                break;
            case BIC_CHANGE_CACHE_BITS:
                if (data_cell_cache != NULL) {
                    free(data_cell_cache);
                }
                mutex_lock(gui_bridge.mutex);
                if (sdata_cell_cache != NULL) {
                    free_cache(sdata_cell_cache);
                }
                cache_bits = gui_bridge.new_cache_bits;
                if (cache_bits > MAX_CACHE_BITS || cache_bits < MIN_CACHE_BITS) {
                    printf("The cache bits %u is not in range (%u:%u).\n", operand_size, MIN_CACHE_BITS, MAX_CACHE_BITS);
                    free(ram);
                    if (sram) {
                        free(sram);
                    }
                    exit(EXIT_FAILURE);
                }
                executing = false;
                data_cell_cache = create_cache(cache_bits);
                // sdata_cell_cache = duplicate_cache(data_cell_cache);
                gui_bridge.backend_interrupt_code = BIC_OPEN_FILE;
                printf("Changed cache bits to %u.\nReloading file from disk ...\n", cache_bits);
                continue;
                mutex_unlock(gui_bridge.mutex);
                break;
            case BIC_START_STEP_BUTTON:
                if (ram == NULL || sram == NULL || data_cell_cache == NULL || sdata_cell_cache == NULL) {
                    fprintf(stderr, "You can't start a file without loading it first.");
                    mutex_lock(gui_bridge.mutex);
                    gui_bridge.backend_interrupt_code = IC_NOTHING;
                    mutex_unlock(gui_bridge.mutex);
                    // exit(EXIT_FAILURE);
                } else if (!executing) {
                    program_counter = 0;
                    mutex_lock(gui_bridge.mutex);
                    executing = true;
                    instruction_counter = 0;
                    reset_queue(&change_queue);
                    accumulator = 0;
                    gui_bridge.backend_interrupt_code = IC_NOTHING;
                    mutex_unlock(gui_bridge.mutex);
                }
                break;
            case BIC_RESET_BUTTON:
                // We need to clean the queue here
                printf("Resetting state from loaded file ...\n");
                mutex_lock(gui_bridge.mutex);
                memcpy(ram, sram, ram_size);
                if (sdata_cell_cache != NULL) {
                    free_cache(data_cell_cache);
                    data_cell_cache = duplicate_cache(sdata_cell_cache);
                } else if (data_cell_cache != NULL) reset_cache(data_cell_cache);
                instruction[0] = '\0';
                coinstruction[0] = '\0';
                cocoinstruction[0] = '\0';
                instruction_counter = 0;
                program_counter = 0;
                executing = false;

                gui_bridge.sdata_cell_cache = sdata_cell_cache;
                gui_bridge.sram = sram;
                gui_bridge.sram_size = ram_size;
                reset_queue(&change_queue);

                gui_bridge.backend_interrupt_code = IC_NOTHING;
                if (gui_bridge.gui_interrupt_code == IC_NOTHING) {
                    gui_bridge.gui_interrupt_code = GIC_RESET;
                } else if (!disable_gui) {
                    fprintf(stderr, "The gui has stopped execution or is in an error state\n");
                    exit(EXIT_FAILURE);
                }
                mutex_unlock(gui_bridge.mutex);
                break;
            case BIC_SINGLE_STEP_MODE_TOGGLE:
                mutex_lock(gui_bridge.mutex);
                single_step_mode = !single_step_mode;
                gui_bridge.backend_interrupt_code = IC_NOTHING;
                mutex_unlock(gui_bridge.mutex);
                break;
            default:
                fprintf(stderr, "Unexpected BIC %u", gui_bridge.backend_interrupt_code);
                mutex_lock(gui_bridge.mutex);
                gui_bridge.backend_interrupt_code = IC_NOTHING;
                mutex_unlock(gui_bridge.mutex);
                break;
        }
        if (executing && !peek) {
            if (single_step_mode && disable_gui) {
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
                        snprintf(instruction, sizeof(instruction), "[%u] LDA_IMM #%i", instruction_counter - 1, accumulator);
                        coinstruction[0] = '\0';
                        cocoinstruction[0] = '\0';
                        break;
                    case LDA_DIR:
                        temp_i32 = (int32_t)get_u32_from_cache_or_ram(data_cell_cache, ram, operand, instruction_size);
                        accumulator = sign_extend_i32(temp_i32, operand_size);
                        snprintf(instruction, sizeof(instruction), "[%u] LDA_DIR %u", instruction_counter - 1, operand);
                        snprintf(coinstruction, sizeof(coinstruction), "[%u] %i", operand, accumulator);
                        cocoinstruction[0] = '\0';
                        break;
                    case LDA_IND:
                        snprintf(instruction, sizeof(instruction), "[%u] LDA_IND %u", instruction_counter - 1, operand);
                        temp_u32 = get_u32_from_cache_or_ram(data_cell_cache, ram, operand, instruction_size); // First level: Load the indirect address
                        snprintf(coinstruction, sizeof(coinstruction), "[%u] %u", operand, temp_u32);
                        temp_i32 = (int32_t)get_u32_from_cache_or_ram(data_cell_cache, ram, temp_u32, instruction_size); // Second level: Load the value at the indirect address
                        accumulator = sign_extend_i32(temp_i32, operand_size); // Store the final value in the accumulator
                        snprintf(cocoinstruction, sizeof(cocoinstruction), "[%u] %i", temp_u32, accumulator);
                        break;
                    case STA_DIR:
                        // if address 0 writes back 0 we have a lot of trouble
                        temp_u64 = add_to_cache(data_cell_cache, operand, (uint32_t)accumulator, true, &is_valid_result);
                        if (is_full(&change_queue)) {
                            printf("QUEUE FULL\n");
                            exit(1);
                        }
                        if (is_valid_result) {
                            writeback_cache_entry(data_cell_cache, ram, temp_u64, instruction_size);
                            enqueue_with_bit(&change_queue, temp_u64, true);
                        }
                        // printf("Queuing1 %u\n", operand);
                        // printf("Making1 %u\n", (uint64_t)operand << 32 | accumulator);
                        enqueue_with_bit(&change_queue, (uint64_t)operand << 32 | accumulator, false);
                        snprintf(instruction, sizeof(instruction), "[%u] STA_DIR %u", instruction_counter - 1, operand);
                        snprintf(coinstruction, sizeof(coinstruction), "[%u] %i", operand, accumulator);
                        cocoinstruction[0] = '\0';
                        break;
                    case STA_IND:
                        snprintf(instruction, sizeof(instruction), "[%u] STA_IND %u", instruction_counter - 1, operand);
                        temp_u32 = get_u32_from_cache_or_ram(data_cell_cache, ram, operand, instruction_size); // First level: Load the indirect address
                        snprintf(coinstruction, sizeof(coinstruction), "[%u] %u", operand, temp_u32);
                        // if address 0 writes back 0 we have a lot of trouble
                        temp_u64 = add_to_cache(data_cell_cache, temp_u32, (uint32_t)accumulator, true, &is_valid_result);
                        if (is_full(&change_queue)) {
                            printf("QUEUE FULL\n");
                            exit(1);
                        }
                        if (is_valid_result) {
                            writeback_cache_entry(data_cell_cache, ram, temp_u64, instruction_size);
                            enqueue_with_bit(&change_queue, temp_u64, true);
                        }
                        // printf("Queuing2 %u\n", temp_u32);
                        enqueue_with_bit(&change_queue, (uint64_t)temp_u32 << 32 | accumulator, false);
                        snprintf(cocoinstruction, sizeof(cocoinstruction), "[%u] %i", temp_u32, accumulator);
                        break;
                    case ADD_DIR:
                        temp_i32 = sign_extend_i32((int32_t)get_u32_from_cache_or_ram(data_cell_cache, ram, operand, instruction_size), operand_size);
                        snprintf(instruction, sizeof(instruction), "[%u] ADD_DIR %u", instruction_counter - 1, operand);
                        snprintf(coinstruction, sizeof(coinstruction), "[%u] %i", operand, temp_i32);
                        cocoinstruction[0] = '\0';
                        accumulator += temp_i32, operand_size;
                        break;
                    case SUB_DIR:
                        temp_i32 = sign_extend_i32((int32_t)get_u32_from_cache_or_ram(data_cell_cache, ram, operand, instruction_size), operand_size);
                        snprintf(instruction, sizeof(instruction), "[%u] SUB_DIR %u", instruction_counter - 1, operand);
                        snprintf(coinstruction, sizeof(coinstruction), "[%u] %i", operand, temp_i32);
                        cocoinstruction[0] = '\0';
                        accumulator -= temp_i32;
                        break;
                    case MUL_DIR:
                        temp_i32 = sign_extend_i32((int32_t)get_u32_from_cache_or_ram(data_cell_cache, ram, operand, instruction_size), operand_size);
                        snprintf(instruction, sizeof(instruction), "[%u] MUL_DIR %u", instruction_counter - 1, operand);
                        snprintf(coinstruction, sizeof(coinstruction), "[%u] %i", operand, temp_i32);
                        cocoinstruction[0] = '\0';
                        accumulator *= temp_i32;
                        break;
                    case DIV_DIR:
                        temp_i32 = sign_extend_i32((int32_t)get_u32_from_cache_or_ram(data_cell_cache, ram, operand, instruction_size), operand_size);
                        snprintf(instruction, sizeof(instruction), "[%u] DIV_DIR %u", instruction_counter - 1, operand);
                        snprintf(coinstruction, sizeof(coinstruction), "[%u] %i", operand, temp_i32);
                        cocoinstruction[0] = '\0';
                        accumulator /= temp_i32;
                        break;
                    case JMP_DIR:
                        instruction_counter = operand;
                        program_counter = instruction_counter * instruction_size;
                        snprintf(instruction, sizeof(instruction), "[%u] JMP_DIR %u", instruction_counter - 1, operand);
                        coinstruction[0] = '\0';
                        cocoinstruction[0] = '\0';
                        break;
                    case JMP_IND:
                        snprintf(instruction, sizeof(instruction), "[%u] JMP_IND %u", instruction_counter - 1, operand);
                        temp_u32 = get_u32_from_cache_or_ram(data_cell_cache, ram, operand, instruction_size);
                        snprintf(coinstruction, sizeof(coinstruction), "[%u] %u", operand, temp_u32);
                        cocoinstruction[0] = '\0';
                        instruction_counter = temp_u32;
                        program_counter = instruction_counter * instruction_size;
                        break;
                    case JNZ_DIR:
                        if (accumulator != 0) {
                            instruction_counter = operand;
                            program_counter = instruction_counter * instruction_size;
                        }
                        snprintf(instruction, sizeof(instruction), "[%u] JNZ_DIR %u", instruction_counter - 1, operand);
                        coinstruction[0] = '\0';
                        cocoinstruction[0] = '\0';
                        break;
                    case JNZ_IND:
                        snprintf(instruction, sizeof(instruction), "[%u] JNZ_IND %u", instruction_counter - 1, operand);
                        temp_u32 = get_u32_from_cache_or_ram(data_cell_cache, ram, operand, instruction_size);
                        snprintf(coinstruction, sizeof(coinstruction), "[%u] %u", operand, temp_u32);
                        cocoinstruction[0] = '\0';
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
                        snprintf(instruction, sizeof(instruction), "[%u] JZE_DIR %u", instruction_counter - 1, operand);
                        coinstruction[0] = '\0';
                        cocoinstruction[0] = '\0';
                        break;
                    case JZE_IND:
                        snprintf(instruction, sizeof(instruction), "[%u] JZE_IND %u", instruction_counter - 1, operand);
                        temp_u32 = get_u32_from_cache_or_ram(data_cell_cache, ram, operand, instruction_size);
                        snprintf(coinstruction, sizeof(coinstruction), "[%u] %u", operand, temp_u32);
                        cocoinstruction[0] = '\0';
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
                        snprintf(instruction, sizeof(instruction), "[%u] JLE_DIR %u", instruction_counter - 1, operand);
                        coinstruction[0] = '\0';
                        cocoinstruction[0] = '\0';
                        break;
                    case JLE_IND:
                        snprintf(instruction, sizeof(instruction), "[%u] JLE_IND %u", instruction_counter - 1, operand);
                        temp_u32 = get_u32_from_cache_or_ram(data_cell_cache, ram, operand, instruction_size);
                        snprintf(coinstruction, sizeof(coinstruction), "[%u] %u", operand, temp_u32);
                        cocoinstruction[0] = '\0';
                        if (accumulator <= 0) {
                            instruction_counter = temp_u32;
                            program_counter = instruction_counter * instruction_size;
                        }
                        break;
                    case STP:
                        executing = false;
                        snprintf(instruction, sizeof(instruction), "[%u] STP", instruction_counter - 1);
                        instruction_counter--;
                        coinstruction[0] = '\0';
                        cocoinstruction[0] = '\0';
                        print_cache(data_cell_cache);
                        for (uint32_t index = 0; index < data_cell_cache->size; index++) {
                            uint64_t entry = data_cell_cache->entries[index];

                            // Extract the stored address
                            uint32_t stored_address = (uint32_t)(entry >> (32 + data_cell_cache->cache_bits));
                            uint32_t stored_operand = (uint32_t)entry;

                            uint64_t actual_entry = ((uint64_t)((stored_address << data_cell_cache->cache_bits) | index) << 32) | stored_operand;
                            writeback_cache_entry(data_cell_cache, ram, actual_entry, instruction_size);
                            enqueue_with_bit(&change_queue, actual_entry, true);
                        }
                        reset_cache(data_cell_cache);
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
                        break;
                    default:
                        break;
                }
                printf("%s (%s;%s)\n", instruction, coinstruction, cocoinstruction);
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
        }
        if (peek) peek = false;
        if (executing && single_step_mode) {
            peek = false;
            if (disable_gui) {
                getchar();
            } else {
                while (gtkgui_running() && single_step_mode) {
                    mutex_lock(gui_bridge.mutex);
                    if (gui_bridge.backend_interrupt_code != IC_NOTHING) {
                        if (gui_bridge.backend_interrupt_code == BIC_START_STEP_BUTTON) {
                            gui_bridge.backend_interrupt_code = IC_NOTHING;
                            mutex_unlock(gui_bridge.mutex);
                            break;
                        } else { // Other codes
                            peek = true;
                            mutex_unlock(gui_bridge.mutex);
                            break;
                        }
                    }
                    mutex_unlock(gui_bridge.mutex);
                }
            }
        }
        if (!executing && disable_gui) {
            char command[256];
            printf("\nSimple CLI for pASM.c\n");
            while (1) {
                printf("\nCommands:\n");
                printf("  open [filename]: Opens a file.\n");
                printf("  reset          : Resets the system state to the currently loaded file.\n");
                printf("  reload         : Reloads the currently opened file from disk.\n");
                printf("  close          : Closes the currently loaded file\n");
                printf("  start          : Starts execution of currently opened file.\n");
                printf("  toggle         : Toggles single-step mode.\n");
                printf("  cache [%u-%u]    : Changes the cache bits.\n", MIN_CACHE_BITS, MAX_CACHE_BITS);
                printf("  exit           : Exits the program.\n");
                printf("\n> ");
                if (!fgets(command, sizeof(command), stdin)) {
                    perror("Failed to read input");
                    continue;
                }
                // Remove newline character
                command[strcspn(command, "\n")] = 0;
                if (strncmp(command, "open ", 5) == 0) {
                    const char *filename = command + 5;
                    mutex_lock(gui_bridge.mutex);
                    if (gui_bridge.backend_interrupt_code == IC_NOTHING) {
                        gui_bridge.backend_interrupt_code = BIC_OPEN_FILE;
                    } else {
                        fprintf(stderr, "The backend has yet to process the last BIC or is in an error state\n");
                        break;
                    }
                    gui_bridge.new_file_str = strdup(filename);
                    mutex_unlock(gui_bridge.mutex);
                    break;
                } else if (strcmp(command, "reload") == 0) {
                    mutex_lock(gui_bridge.mutex);
                    if (gui_bridge.backend_interrupt_code == IC_NOTHING) {
                        gui_bridge.backend_interrupt_code = BIC_OPEN_FILE;
                    } else {
                        fprintf(stderr, "The backend has yet to process the last BIC or is in an error state\n");
                        break;
                    }
                    mutex_unlock(gui_bridge.mutex);
                    break;
                } else if (strcmp(command, "start") == 0) {
                    mutex_lock(gui_bridge.mutex);
                    if (gui_bridge.backend_interrupt_code == IC_NOTHING) {
                        gui_bridge.backend_interrupt_code = BIC_START_STEP_BUTTON;
                    } else {
                        fprintf(stderr, "The backend has yet to process the last BIC or is in an error state\n");
                        break;
                    }
                    mutex_unlock(gui_bridge.mutex);
                    break;
                } else if (strcmp(command, "reset") == 0) {
                    mutex_lock(gui_bridge.mutex);
                    if (gui_bridge.backend_interrupt_code == IC_NOTHING) {
                        gui_bridge.backend_interrupt_code = BIC_RESET_BUTTON;
                    } else {
                        fprintf(stderr, "The backend has yet to process the last BIC or is in an error state\n");
                        break;
                    }
                    mutex_unlock(gui_bridge.mutex);
                    break;
                } else if (strcmp(command, "toggle") == 0) {
                    single_step_mode = !single_step_mode;
                } else if (strncmp(command, "cache ", 6) == 0) {
                    int bits = atoi(command + 6);
                    if (bits >= MIN_CACHE_BITS && bits <= MAX_CACHE_BITS) {
                        mutex_lock(gui_bridge.mutex);
                        if (gui_bridge.backend_interrupt_code == IC_NOTHING) {
                            gui_bridge.backend_interrupt_code = BIC_CHANGE_CACHE_BITS;
                        } else {
                            fprintf(stderr, "The backend has yet to process the last BIC or is in an error state\n");
                            break;
                        }
                        gui_bridge.new_cache_bits = (uint8_t)bits;
                        mutex_unlock(gui_bridge.mutex);
                        break;
                    } else {
                        printf("Error: Cache bits must be between %u and %u.\n", MIN_CACHE_BITS, MAX_CACHE_BITS);
                    }
                } else if (strcmp(command, "exit") == 0) {
                    printf("Exiting program.\n");
                    running = false;
                    break;
                } else if (strcmp(command, "close") == 0) {
                    mutex_lock(gui_bridge.mutex);
                    if (gui_bridge.backend_interrupt_code == IC_NOTHING) {
                        gui_bridge.backend_interrupt_code = BIC_CLOSE_FILE;
                    } else {
                        fprintf(stderr, "The backend has yet to process the last BIC or is in an error state\n");
                        break;
                    }
                    mutex_unlock(gui_bridge.mutex);
                    break;
                } else {
                    printf("Unknown command: '%s'. Please try again.\n", command);
                }
            }
        }
    }

    // print_cache(data_cell_cache);
    // print_buffer_in_hex(ram, file_size);

    if (!disable_gui) gtkgui_stop();
    free(ram);
    free(sram);
    free_cache(data_cell_cache);
    free_cache(sdata_cell_cache);
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
    char input_file[MAX_PATH] = "";
    uint8_t cache_bits = 4;
    uint8_t queue_size = 100;

    bool run_only_gui = false;
    bool immidiate_start = false;
    bool single_loop = false;
    bool help = false;
    ParseableArgument arguments[] = {
        {"help", "h", &help, strtobool, false},
        {"hilfe", "?", &help, strtobool, false},
        {"run-only-gui", "rog", &run_only_gui, strtobool, false},
        {"disable-gui", "ng", &disable_gui, strtobool, false}, 
        {"singlestep", "ss", &single_step_mode, strtobool, false},
        {"overwrite-memory-size=", "ms=", &overwrite_memory_size, strtou32, false},
        {"overwrite-operand-size=", "os=", &overwrite_operand_size, strtou8, false},
        {"cache-bits=", "cb=", &cache_bits, strtou8, false},
        {"queue-size=", "qs=", &queue_size, strtou8, false},
        {"immidiate-start", "is", &immidiate_start, strtobool, false},
        {"single-loop", "sl", &single_loop, strtobool, false},
        // {"debug", "d", &debug}
        {"", "", &input_file, strtostr, false}, // Positional argument
    };
    int num_arguments = sizeof(arguments) / sizeof(ParseableArgument);
    int exit_code = parse_arguments(argc, argv, arguments, num_arguments);

    if (exit_code == EXIT_FAILURE) {
        exit(EXIT_FAILURE);
    }

    if (help) {
        printf("pASM.c Help Menu ~Flags~:\n");
        printf("  help [hilfe; h; ?]                 : Opens this menu.\n");
        printf("  run-only-gui [rog]                 : Runs only the gui, no backend.\n");
        printf("  disable-gui [ng]                   : Runs only the backend, no gui.\n");
        printf("  singlestep [ss]                    : Enables the single-step mode.\n");
        printf("  overwrite-memory-size [ms]={%u-%u}    : Overwrites the memory size for all loaded files.\n", MIN_MEMORY_SIZE, MAX_MEMORY_SIZE);
        printf("  overwrite-operand-size [os]={%u-%u}  : Overwrites the operand size for all loaded files.\n", MIN_OPERAND_SIZE, MAX_OPERAND_SIZE);
        printf("  cache-bits [cb]={%u-%u}              : Sets the cache bits for the program, the default is 4.\n", MIN_CACHE_BITS, MAX_CACHE_BITS);
        printf("  queue-size [qs]={>0}               : Sets the queue size for the program, the default is 100.\n");
        printf("  immidiate-start [is]               : Immidiately starts the program, can only be used if you also specify a file.\n");
        printf("  single-loop [sl]                   : Makes the program exit after one loop (1 file execution).\n");
        printf("  {positional_arg}.p                 : The input file, has to end in '.p'.\n");
        exit(EXIT_SUCCESS);
    }

    if (!disable_gui) {
        HWND hWnd = GetConsoleWindow();
        ShowWindow( hWnd, SW_MINIMIZE );  //won't hide the window without SW_MINIMIZE
        ShowWindow( hWnd, SW_HIDE );
    }

    if (run_only_gui) {
        exit_code = run_gui();
    } else {
        exit_code = p_program(argv[0], disable_gui, single_step_mode, overwrite_memory_size, overwrite_operand_size, input_file, cache_bits, queue_size, immidiate_start, single_loop);
    }
    return exit_code;
}
