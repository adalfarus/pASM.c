#include <gtk/gtk.h>
#include <stdbool.h>

#include "gtkgui.h"
#include "CTools/treader.h"
#include "pconstants.h" // For version info
#include "putils.h"

static thread_t gui_thread;
static GtkApplication *app = NULL;
static bool running = false;
static bool cleaned_up = false;
Bridge *backend_bridge = NULL;
static GtkWidget *start_stop_button = NULL;
static GtkWidget *single_step_checkbox = NULL;
GtkWidget *left_grid = NULL;
GtkWidget *right_upper_grid = NULL;
GtkWidget *cell_1_entry = NULL;
GtkWidget *cell_2_entry = NULL;
GtkWidget *cell_3_entry = NULL;
GtkWidget *accumulator = NULL;
size_t previous_slot_index = (size_t)-1;

static void on_ia_help(GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    // Parent window from user_data
    GtkWidget *parent_window = GTK_WIDGET(g_object_get_data(G_OBJECT(action), "parent-window"));

    if (!parent_window) {
        g_print("Error: Parent window not set!\n");
        return;
    }

    // Create a new dialog
    GtkWidget *dialog = gtk_dialog_new();
    gtk_window_set_title(GTK_WINDOW(dialog), "Instruction Architecture");
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(parent_window));
    gtk_window_set_default_size(GTK_WINDOW(dialog), 600, 400); // Set a larger default size
    gtk_window_set_resizable(GTK_WINDOW(dialog), TRUE); // Make it resizable

    // Create content area and add a scrolled window
    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget *scrolled_window = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_hexpand(scrolled_window, TRUE);
    gtk_widget_set_vexpand(scrolled_window, TRUE);

    // Create a text view and set the instruction architecture text
    GtkWidget *text_view = gtk_text_view_new();
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text_view), GTK_WRAP_WORD);
    gtk_text_view_set_editable(GTK_TEXT_VIEW(text_view), FALSE); // Make it read-only
    gtk_widget_set_hexpand(text_view, TRUE);
    gtk_widget_set_vexpand(text_view, TRUE);

    // Apply a CSS style for larger font size
    GtkCssProvider *css_provider = gtk_css_provider_new();
    gtk_css_provider_load_from_string(css_provider,
                                     "textview { font-size: 18px; }");
    GtkStyleContext *context = gtk_widget_get_style_context(text_view);
    gtk_style_context_add_provider(context, GTK_STYLE_PROVIDER(css_provider), GTK_STYLE_PROVIDER_PRIORITY_USER);

    const char *instruction_text =
        "Die Befehle\n"
        "\n"
        "LDA #xx   AKKU := xx\n"
        "LDA  xx   AKKU := RAM[xx]\n"
        "LDA (xx)  AKKU := RAM[RAM[xx]]\n"
        "\n"
        "STA  xx   RAM[xx] := AKKU\n"
        "STA (xx)  RAM[RAM[xx]] := AKKU\n"
        "\n"
        "ADD  xx   AKKU := AKKU + RAM[xx]\n"
        "SUB  xx   AKKU := AKKU - RAM[xx]\n"
        "MUL  xx   AKKU := AKKU * RAM[xx]\n"
        "DIV  xx   AKKU := AKKU DIV RAM[xx]\n"
        "\n"
        "JMP  xx   PZ := xx\n"
        "JMP (xx)  PZ := RAM[xx]\n"
        "\n"
        "JNZ  xx   PZ := xx, wenn AKKU <> 0\n"
        "JNZ (xx)  PZ := RAM[xx], wenn AKKU <> 0\n"
        "\n"
        "JZE  xx   PZ := xx, wenn AKKU = 0\n"
        "JZE (xx)  PZ := RAM[xx], wenn AKKU = 0\n"
        "\n"
        "JLE  xx   PZ := xx, wenn AKKU <= 0\n"
        "JLE (xx)  PZ := RAM[xx], wenn AKKU <= 0\n"
        "\n"
        "STP       STOP\n";

    gtk_text_buffer_set_text(gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view)), instruction_text, -1);

    // Add the text view to the scrolled window
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled_window), text_view);

    // Add the scrolled window to the dialog content area
    gtk_box_append(GTK_BOX(content_area), scrolled_window);

    // Add a Close button
    gtk_dialog_add_buttons(GTK_DIALOG(dialog), "_Close", GTK_RESPONSE_CLOSE);

    // Connect the Close button signal
    g_signal_connect(dialog, "response", G_CALLBACK(gtk_window_destroy), dialog);

    // Show the dialog
    gtk_window_present(GTK_WINDOW(dialog));
}

static void on_app_help(GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    // Retrieve the parent window stored in the action
    GtkWindow *parent_window = GTK_WINDOW(g_object_get_data(G_OBJECT(action), "parent-window"));

    if (!parent_window) {
        g_print("Error: Parent window not set!\n");
        return;
    }

    // Create a new dialog
    GtkWidget *dialog = gtk_dialog_new();
    gtk_window_set_title(GTK_WINDOW(dialog), "App Information");
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    gtk_window_set_default_size(GTK_WINDOW(dialog), 400, 200); // Default size
    gtk_window_set_resizable(GTK_WINDOW(dialog), TRUE); // Allow resizing

    // Set transient parent to avoid warnings
    gtk_window_set_transient_for(GTK_WINDOW(dialog), parent_window);

    // Create content area and format the message
    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget *label = gtk_label_new(NULL);
    char message[256];
    snprintf(message, sizeof(message), "Program Name: %s\nVersion: %s\n%s", APP_NAME, VERSION, COPYRIGHT);

    gtk_label_set_text(GTK_LABEL(label), message);
    gtk_label_set_wrap(GTK_LABEL(label), TRUE); // Enable text wrapping
    gtk_widget_set_hexpand(label, TRUE);
    gtk_widget_set_vexpand(label, TRUE);

    // Apply CSS for larger font size
    GtkCssProvider *css_provider = gtk_css_provider_new();
    gtk_css_provider_load_from_string(css_provider,
                                     "label { font-size: 18px; }");
    GtkStyleContext *context = gtk_widget_get_style_context(label);
    gtk_style_context_add_provider(context, GTK_STYLE_PROVIDER(css_provider), GTK_STYLE_PROVIDER_PRIORITY_USER);

    // Add the label to the content area
    gtk_box_append(GTK_BOX(content_area), label);

    // Add a Close button
    gtk_dialog_add_buttons(GTK_DIALOG(dialog), "_Close", GTK_RESPONSE_CLOSE);

    // Connect the Close button signal
    g_signal_connect(dialog, "response", G_CALLBACK(gtk_window_destroy), dialog);

    // Show the dialog
    gtk_window_present(GTK_WINDOW(dialog));
}

// Callback to handle the response from the file chooser dialog
static void on_file_chooser_response(GtkWidget *dialog, int response_id, gpointer user_data) {
    if (response_id == GTK_RESPONSE_ACCEPT) {
        GtkFileChooser *chooser = GTK_FILE_CHOOSER(dialog);

        // Get the selected file
        GFile *file = gtk_file_chooser_get_file(chooser);
        if (file) {
            char *filename = g_file_get_path(file); // Get the file path as a string

            // Process the selected file
            mutex_lock(backend_bridge->mutex);
            if (backend_bridge->backend_interrupt_code == IC_NOTHING) {
                backend_bridge->backend_interrupt_code = BIC_OPEN_FILE;
                backend_bridge->new_file_str = strdup(filename);
            } else {
                g_print("The backend has yet to process the last BIC or is in an error state\n");
            }
            mutex_unlock(backend_bridge->mutex);

            g_print("File selected: %s\n", filename);
            g_free(filename);
            g_object_unref(file); // Free the GFile object
        }
    }

    // Destroy the dialog after use
    gtk_window_destroy(GTK_WINDOW(dialog));
}

static void on_open_file(GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    GtkWidget *dialog;
    // Retrieve the parent window stored in the action
    GtkWindow *parent_window = GTK_WINDOW(g_object_get_data(G_OBJECT(action), "parent-window"));

    if (!parent_window) {
        g_print("Error: Parent window not set!\n");
        return;
    }
    dialog = gtk_file_chooser_dialog_new("Open File", parent_window,
                                         GTK_FILE_CHOOSER_ACTION_OPEN, "_Cancel",
                                         GTK_RESPONSE_CANCEL, "_Open",
                                         GTK_RESPONSE_ACCEPT, NULL);
    gtk_window_set_transient_for(GTK_WINDOW(dialog), parent_window); // Set the parent window to avoid warnings and ensure proper modality
    gtk_widget_show(dialog); // Show the dialog asynchronously
    g_signal_connect(dialog, "response", G_CALLBACK(on_file_chooser_response), parent_window); // Handle the response asynchronously
}

static void on_reload_file(GtkWidget *widget, gpointer user_data) {
    mutex_lock(backend_bridge->mutex);
    if (backend_bridge->backend_interrupt_code == IC_NOTHING) {
        backend_bridge->backend_interrupt_code = BIC_OPEN_FILE;
    } else {
        g_print("Backend has not processed the last BIC or is in an error state.\n");
    }
    mutex_unlock(backend_bridge->mutex);
}

static void on_close_file(GtkWidget *widget, gpointer user_data) {
    mutex_lock(backend_bridge->mutex);
    if (backend_bridge->backend_interrupt_code == IC_NOTHING) {
        backend_bridge->backend_interrupt_code = BIC_CLOSE_FILE;
    } else {
        g_print("Backend has not processed the last BIC or is in an error state.\n");
    }
    mutex_unlock(backend_bridge->mutex);
}

static void on_start_step(GtkWidget *widget, gpointer user_data) {
    mutex_lock(backend_bridge->mutex);
    if (backend_bridge->backend_interrupt_code == IC_NOTHING) {
        if (*backend_bridge->executing && !*backend_bridge->single_step_mode) {
            backend_bridge->backend_interrupt_code = BIC_SINGLE_STEP_MODE_TOGGLE;
            gtk_check_button_set_active(GTK_CHECK_BUTTON(single_step_checkbox), TRUE);
        } else {
            backend_bridge->backend_interrupt_code = BIC_START_STEP_BUTTON;
        }
    } else {
        g_print("Backend has not processed the last BIC or is in an error state.\n");
    }
    mutex_unlock(backend_bridge->mutex);
}

static void on_reset_file(GtkWidget *widget, gpointer user_data) {
    mutex_lock(backend_bridge->mutex);
    if (backend_bridge->backend_interrupt_code == IC_NOTHING) {
        backend_bridge->backend_interrupt_code = BIC_RESET_BUTTON;
    } else {
        g_print("Backend has not processed the last BIC or is in an error state.\n");
    }
    mutex_unlock(backend_bridge->mutex);
}

static void on_toggle_single_step(GtkWidget *widget, gpointer user_data) {
    mutex_lock(backend_bridge->mutex);
    bool old_single_step = *backend_bridge->single_step_mode;
    if (backend_bridge->backend_interrupt_code == IC_NOTHING) {
        backend_bridge->backend_interrupt_code = BIC_SINGLE_STEP_MODE_TOGGLE;
    } else {
        g_print("Backend has not processed the last BIC or is in an error state.\n");
    }
    mutex_unlock(backend_bridge->mutex);
    g_print("Single Step Mode toggled: %s\n", !old_single_step ? "Enabled" : "Disabled");
}

static void on_change_cache_bits(GSimpleAction *simple_action, gpointer user_data) {
    int bits = atoi(strrchr(g_action_get_name(G_ACTION(simple_action)), '_') + 1);

    if (bits >= MIN_CACHE_BITS && bits <= MAX_CACHE_BITS) {
        mutex_lock(backend_bridge->mutex);
        if (backend_bridge->backend_interrupt_code == IC_NOTHING) {
            backend_bridge->backend_interrupt_code = BIC_CHANGE_CACHE_BITS;
            backend_bridge->new_cache_bits = (uint8_t)bits;
        } else {
            g_print("Backend has not processed the last BIC or is in an error state.\n");
        }
        mutex_unlock(backend_bridge->mutex);
    } else {
        g_print("Error: Cache bits must be between %u and %u.\n", MIN_CACHE_BITS, MAX_CACHE_BITS);
    }
}

static gboolean on_key_press_event(GtkEventControllerKey *controller, guint keyval, guint keycode, GdkModifierType state, gpointer user_data) {
    if (state & GDK_CONTROL_MASK) { // Check if Ctrl key is pressed
        switch (keyval) {
            case GDK_KEY_g:
                if (!*backend_bridge->executing) {
                    g_print("Ctrl+g pressed (Start)\n");
                    on_start_step(NULL, user_data);
                    return TRUE;
                }
                break;
            case GDK_KEY_s:
                if (*backend_bridge->executing && *backend_bridge->single_step_mode) {
                    g_print("Ctrl+s pressed (Step)\n");
                    on_start_step(NULL, user_data);
                    return TRUE;
                }
                break;
            case GDK_KEY_e:
                if (*backend_bridge->executing && !*backend_bridge->single_step_mode) {
                    g_print("Ctrl+e pressed (Stop/End)\n");
                    on_start_step(NULL, user_data);
                    return TRUE;
                }
                break;
            case GDK_KEY_n:
                g_print("Ctrl+N pressed (Reset)\n");
                on_reset_file(NULL, user_data);
                return TRUE;
            case GDK_KEY_o:
                g_print("Ctrl+o pressed (open file)\n");
                on_open_file(NULL, NULL, user_data);
                return TRUE;
            case GDK_KEY_r:
                g_print("Ctrl+r pressed (reload file)\n");
                on_reload_file(NULL, user_data);
                return TRUE;
            case GDK_KEY_c:
                g_print("Ctrl+c pressed (close file)\n");
                on_close_file(NULL, user_data);
                return TRUE;
            default:
                break;
        }
    }
    return FALSE;
}

static void clear_grid(GtkWidget *grid) {
    GtkWidget *child = gtk_widget_get_first_child(grid);
    while (child) {
        GtkWidget *next = gtk_widget_get_next_sibling(child);
        gtk_widget_unparent(child);
        child = next;
    }
}

static void populate_grid(GtkWidget *grid, int start, int end) {
    clear_grid(grid); // Clear existing grid content

    for (int i = start; i <= end; i++) {
        GtkWidget *label = gtk_label_new(g_strdup_printf("%d", i));
        gtk_widget_set_name(label, "grid-cell");
        gtk_widget_set_margin_top(label, 5);
        gtk_widget_set_margin_bottom(label, 5);
        gtk_widget_set_margin_start(label, 10);
        gtk_widget_set_margin_end(label, 10);
        gtk_grid_attach(GTK_GRID(grid), label, 0, i - start, 1, 1);
    }
    gtk_widget_queue_draw(grid); // Redraw the grid
}

static void on_activate(GtkApplication *app, gpointer user_data) {
    // Main window
    GtkWidget *window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "pASMc GUI");
    gtk_window_set_default_size(GTK_WINDOW(window), 700, 510);

    // Main horizontal box layout
    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_set_homogeneous(GTK_BOX(main_box), TRUE); // Equal space for both sides
    gtk_widget_set_hexpand(main_box, TRUE);
    gtk_widget_set_vexpand(main_box, TRUE);

    // LEFT PANEL: Large scrollable grid
    GtkWidget *left_scroll = gtk_scrolled_window_new();
    gtk_widget_set_hexpand(left_scroll, TRUE);
    gtk_widget_set_vexpand(left_scroll, TRUE);

    left_grid = gtk_grid_new();
    gtk_widget_set_name(left_grid, "grid");
    gtk_widget_set_hexpand(left_grid, TRUE);
    gtk_widget_set_vexpand(left_grid, TRUE);

    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(left_scroll), left_grid);

    // RIGHT PANEL: Smaller scrollable grid and controls
    GtkWidget *right_panel = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_widget_set_hexpand(right_panel, TRUE);
    gtk_widget_set_vexpand(right_panel, TRUE);

    // Right Upper Half: Smaller scrollable grid (1-255)
    GtkWidget *right_upper_scroll = gtk_scrolled_window_new();
    gtk_widget_set_hexpand(right_upper_scroll, TRUE);
    gtk_widget_set_vexpand(right_upper_scroll, TRUE);

    right_upper_grid = gtk_grid_new();
    GtkWidget *label = gtk_label_new("Cache is not initialized.");
    gtk_grid_attach(GTK_GRID(right_upper_grid), label, 0, 0, 1, 1);
    gtk_widget_set_name(right_upper_grid, "grid");
    gtk_widget_set_hexpand(right_upper_grid, TRUE);
    gtk_widget_set_vexpand(right_upper_grid, TRUE);

    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(right_upper_scroll), right_upper_grid);
    gtk_box_append(GTK_BOX(right_panel), right_upper_scroll);

    // Right Lower Half: Controls
    GtkWidget *right_lower_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_widget_set_hexpand(right_lower_box, TRUE);
    gtk_widget_set_vexpand(right_lower_box, TRUE);

    // Create a grid for the numbered cells
    GtkWidget *right_lower_grid = gtk_grid_new(); // Create a new grid
    gtk_widget_set_name(right_lower_grid, "gridd");

    // Create and configure the entries
    cell_1_entry = gtk_label_new("n ");
    gtk_widget_set_name(cell_1_entry, "sgrid-cell");
    gtk_widget_set_margin_top(cell_1_entry, 5);
    gtk_widget_set_margin_bottom(cell_1_entry, 5);
    gtk_widget_set_margin_start(cell_1_entry, 10);
    gtk_widget_set_margin_end(cell_1_entry, 10);
    cell_2_entry = gtk_label_new("m ");
    gtk_widget_set_name(cell_2_entry, "sgrid-cell");
    gtk_widget_set_margin_top(cell_2_entry, 5);
    gtk_widget_set_margin_bottom(cell_2_entry, 5);
    gtk_widget_set_margin_start(cell_2_entry, 10);
    gtk_widget_set_margin_end(cell_2_entry, 10);
    cell_3_entry = gtk_label_new("s ");
    gtk_widget_set_name(cell_3_entry, "sgrid-cell");
    gtk_widget_set_margin_top(cell_3_entry, 5);
    gtk_widget_set_margin_bottom(cell_3_entry, 5);
    gtk_widget_set_margin_start(cell_3_entry, 10);
    gtk_widget_set_margin_end(cell_3_entry, 10);

    //gtk_label_set_xalign(GTK_LABEL(cell_1_entry), 0.0);
    //gtk_label_set_xalign(GTK_LABEL(cell_2_entry), 0.0);
    //gtk_widget_set_hexpand(cell_1_entry, TRUE);
    //gtk_widget_set_hexpand(cell_2_entry, TRUE);
    gtk_grid_attach(GTK_GRID(right_lower_grid), cell_1_entry, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(right_lower_grid), cell_2_entry, 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(right_lower_grid), cell_3_entry, 0, 2, 1, 1);
    gtk_box_append(GTK_BOX(right_lower_box), right_lower_grid);

    // Accumulator Label
    accumulator = gtk_label_new("Accumulator: 0");
    gtk_box_append(GTK_BOX(right_lower_box), accumulator);

    // Buttons (Start/Step/Stop and Reset)
    GtkWidget *button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    start_stop_button = gtk_button_new_with_label("Start (Ctrl+g)"); // Step (Ctrl+s)
    GtkWidget *reset_button = gtk_button_new_with_label("Reset (Ctrl+n)");
    g_signal_connect(start_stop_button, "clicked", G_CALLBACK(on_start_step), NULL);
    g_signal_connect(reset_button, "clicked", G_CALLBACK(on_reset_file), NULL);
    gtk_box_append(GTK_BOX(button_box), start_stop_button);
    gtk_box_append(GTK_BOX(button_box), reset_button);
    gtk_box_append(GTK_BOX(right_lower_box), button_box);

    // Single Step Checkbox
    single_step_checkbox = gtk_check_button_new_with_label("Single Step Mode");
    gtk_check_button_set_active(GTK_CHECK_BUTTON(single_step_checkbox), *backend_bridge->single_step_mode ? TRUE : FALSE);
    gtk_box_append(GTK_BOX(right_lower_box), single_step_checkbox);
    g_signal_connect(single_step_checkbox, "toggled", G_CALLBACK(on_toggle_single_step), NULL);

    gtk_box_append(GTK_BOX(right_panel), right_lower_box);

    // Combine Panels
    gtk_box_append(GTK_BOX(main_box), left_scroll); // Add large grid on the left
    gtk_box_append(GTK_BOX(main_box), right_panel); // Add controls and smaller grid on the right

    // Add a menu bar to the top
    GMenu *menu_bar = g_menu_new();

    // File Menu
    GMenu *file_menu = g_menu_new();
    g_menu_append_submenu(menu_bar, "File", G_MENU_MODEL(file_menu));
    GSimpleAction *act_open = g_simple_action_new("file_open", NULL);
    GSimpleAction *act_reload = g_simple_action_new("file_reload", NULL);
    GSimpleAction *act_close = g_simple_action_new("file_close", NULL);
    g_action_map_add_action(G_ACTION_MAP(app), G_ACTION(act_open));
    g_action_map_add_action(G_ACTION_MAP(app), G_ACTION(act_reload));
    g_action_map_add_action(G_ACTION_MAP(app), G_ACTION(act_close));
    g_signal_connect(act_open, "activate", G_CALLBACK(on_open_file), NULL);
    g_signal_connect(act_reload, "activate", G_CALLBACK(on_reload_file), NULL);
    g_signal_connect(act_close, "activate", G_CALLBACK(on_close_file), NULL);
    // Store the window as user data in the actions
    g_object_set_data(G_OBJECT(act_open), "parent-window", window);
    GMenuItem *menu_item_open = g_menu_item_new("Open (Ctrl+o)", "app.file_open");
    g_menu_append_item(file_menu, menu_item_open);
    GMenuItem *menu_item_reload = g_menu_item_new("Reload (Ctrl+r)", "app.file_reload");
    g_menu_append_item(file_menu, menu_item_reload);
    GMenuItem *menu_item_close = g_menu_item_new("Close (Ctrl+c)", "app.file_close");
    g_menu_append_item(file_menu, menu_item_close);

    // Settings Menu
    GMenu *settings_menu = g_menu_new();
    g_menu_append_submenu(menu_bar, "Settings", G_MENU_MODEL(settings_menu));
    GMenu *cache_bits_menu = g_menu_new();
    g_menu_append_submenu(settings_menu, "Cache Bits", G_MENU_MODEL(cache_bits_menu));
    for (int i = MIN_CACHE_BITS; i <= MAX_CACHE_BITS; i++) {
        char label[4];
        snprintf(label, sizeof(label), "%d", i);
        char action_name[32];
        snprintf(action_name, sizeof(action_name), "cache_bits_%d", i);

        GSimpleAction *act_tmp = g_simple_action_new(action_name, NULL);
        g_action_map_add_action(G_ACTION_MAP(app), G_ACTION(act_tmp));
        g_signal_connect(act_tmp, "activate", G_CALLBACK(on_change_cache_bits), NULL);
        snprintf(action_name, sizeof(action_name), "app.cache_bits_%d", i);
        GMenuItem *menu_item_tmp = g_menu_item_new(label, action_name);
        g_menu_append_item(cache_bits_menu, menu_item_tmp);
        g_object_unref(act_tmp);
        g_object_unref(menu_item_tmp);
    }

    // Info Menu
    GMenu *info_menu = g_menu_new();
    g_menu_append_submenu(menu_bar, "Info", G_MENU_MODEL(info_menu));
    GSimpleAction *act_ia_help = g_simple_action_new("ia_help", NULL);
    GSimpleAction *act_app_help = g_simple_action_new("app_help", NULL);
    g_action_map_add_action(G_ACTION_MAP(app), G_ACTION(act_ia_help));
    g_action_map_add_action(G_ACTION_MAP(app), G_ACTION(act_app_help));
    // Store the window as user data in the actions
    g_object_set_data(G_OBJECT(act_ia_help), "parent-window", window);
    g_object_set_data(G_OBJECT(act_app_help), "parent-window", window);
    g_signal_connect(act_ia_help, "activate", G_CALLBACK(on_ia_help), NULL);
    g_signal_connect(act_app_help, "activate", G_CALLBACK(on_app_help), NULL);
    GMenuItem *menu_item_ia_help = g_menu_item_new("Instruction Architecture", "app.ia_help");
    g_menu_append_item(info_menu, menu_item_ia_help);
    GMenuItem *menu_item_app_help = g_menu_item_new("Application", "app.app_help");
    g_menu_append_item(info_menu, menu_item_app_help);

    gtk_window_set_child(GTK_WINDOW(window), main_box);
    gtk_application_set_menubar(GTK_APPLICATION(app), G_MENU_MODEL(menu_bar));
    gtk_application_window_set_show_menubar(GTK_APPLICATION_WINDOW(window), TRUE);

    // Add keyboard listener
    GtkEventController *key_controller = gtk_event_controller_key_new();
    g_signal_connect(key_controller, "key-pressed", G_CALLBACK(on_key_press_event), NULL);
    gtk_widget_add_controller(window, GTK_EVENT_CONTROLLER(key_controller));

    // Apply CSS for styling
    GtkCssProvider *css_provider = gtk_css_provider_new();
    gtk_css_provider_load_from_string(css_provider,
                                     "* { font-size: 18px; }"
                                     "#grid-cell { background: #ebe8e6; border: 1px solid #aaa; border-radius: 5px; }" //#f6f5f4 (a bit darker)
                                     "#sgrid-cell { background: #eaeaea; border: 1px solid rgba(0, 255, 255, 0.5); border-radius: 5px; }"
                                     "#grid { border: 2px solid #aaa; border-radius: 5px; }"
                                     "#highlighted-cell { background: #ffdd00; border: 1px solid #ffff00; border-radius: 5px; }");
    // Apply CSS globally to the display
    GdkDisplay *display = gdk_display_get_default();
    gtk_style_context_add_provider_for_display(display, GTK_STYLE_PROVIDER(css_provider), GTK_STYLE_PROVIDER_PRIORITY_USER);

    // Show the window
    gtk_window_present(GTK_WINDOW(window));

    // Cleanup
    g_object_unref(file_menu);
    g_object_unref(act_open);
    g_object_unref(act_reload);
    g_object_unref(act_close);
    g_object_unref(menu_item_open);
    g_object_unref(menu_item_reload);
    g_object_unref(menu_item_close);

    g_object_unref(settings_menu);
    g_object_unref(cache_bits_menu);

    g_object_unref(info_menu);
    g_object_unref(act_ia_help);
    g_object_unref(act_app_help);
    g_object_unref(menu_item_ia_help);
    g_object_unref(menu_item_app_help);
}

void highlight_cell(GtkWidget *grid, size_t new_slot_index, size_t *previous_slot_index) {
    GtkWidget *current_child = gtk_widget_get_first_child(grid);
    GtkWidget *new_target_label = NULL;
    GtkWidget *previous_target_label = NULL;

    size_t current_index = 0;

    // Iterate through grid children to find the new and previous slots
    while (current_child != NULL) {
        if (current_index == new_slot_index) {
            new_target_label = current_child;
        }
        if (previous_slot_index && current_index == *previous_slot_index) {
            previous_target_label = current_child;
        }

        current_child = gtk_widget_get_next_sibling(current_child);
        current_index++;
    }

    if (previous_target_label) {
        gtk_widget_set_name(previous_target_label, "grid-cell");
    }
    if (new_target_label) {
        gtk_widget_set_name(new_target_label, "highlighted-cell");
    }
    if (previous_slot_index) {
        *previous_slot_index = new_slot_index;
    }
}

static gboolean main_loop_update(gpointer user_data) {
    if (!backend_bridge) return G_SOURCE_REMOVE;

    GtkButton *button = GTK_BUTTON(start_stop_button); // Cast to GtkButton
    const char *current_label = gtk_button_get_label(button);

    mutex_lock(backend_bridge->mutex);

    if (backend_bridge->gui_interrupt_code != IC_NOTHING) {
        if (backend_bridge->gui_interrupt_code == GIC_RESET) {
            clear_grid(GTK_WIDGET(left_grid));
            uint8_t *sram = backend_bridge->sram;
            uint64_t sram_size = backend_bridge->sram_size;
            uint8_t instruction_size = *backend_bridge->instruction_size;
            uint8_t operand_size = instruction_size - 1;

            size_t ram_index = 0;
            size_t slot_index = 0;

            while (ram_index < sram_size && sram != NULL) {
                // Ensure there is enough space for a full instruction
                if (ram_index + instruction_size > sram_size) {
                    fprintf(stderr, "Incomplete instruction at offset %zu. Skipping.\n", ram_index);
                    break;
                }

                // Read opcode and operand
                uint8_t opcode = sram[ram_index];
                uint32_t operand = 0;
                int32_t signed_operand = 0;

                if (opcode == 0) {
                    // Opcode 0: Use sign-extended operand
                    signed_operand = sign_extend_i32((uint32_t)sram[ram_index + 1], operand_size);
                } else {
                    // Normal unsigned operand
                    memcpy(&operand, sram + ram_index + 1, operand_size);
                }

                // Get instruction name
                const char *instruction_name = (opcode <= 99) ? INSTRUCTION_SET[opcode] : "UNKNOWN";

                // Format the instruction string
                char instruction_str[64];
                if (opcode == 0) {
                    snprintf(instruction_str, sizeof(instruction_str), "[%zu] %s %d", slot_index, instruction_name, signed_operand);
                } else {
                    snprintf(instruction_str, sizeof(instruction_str), "[%zu] %s %u", slot_index, instruction_name, operand);
                }

                // Create a new label for the grid slot
                GtkWidget *label = gtk_label_new(instruction_str);
                gtk_widget_set_name(label, "grid-cell");
                gtk_widget_set_margin_top(label, 5);
                gtk_widget_set_margin_bottom(label, 5);
                gtk_widget_set_margin_start(label, 10);
                gtk_widget_set_margin_end(label, 10);
                gtk_label_set_xalign(GTK_LABEL(label), 0.0); // Align text to the left
                gtk_grid_attach(GTK_GRID(left_grid), label, 0, slot_index, 1, 1);

                // Move to the next instruction
                ram_index += instruction_size;
                slot_index++;
            }
            
            gtk_widget_queue_draw(GTK_WIDGET(left_grid));
            clear_grid(GTK_WIDGET(right_upper_grid));
            Cache *cache = backend_bridge->sdata_cell_cache;
            if (!cache || !cache->entries) {
                GtkWidget *label = gtk_label_new("Cache is not initialized.");
                gtk_grid_attach(GTK_GRID(right_upper_grid), label, 0, 0, 1, 1);
            } else {
                uint8_t cache_size = cache->size;

                for (uint8_t i = 0; i < cache_size; i++) {
                    uint64_t entry = cache->entries[i];

                    // Extract cache information
                    bool is_dirty = entry & (1ULL << 32);
                    uint32_t stored_address = (uint32_t)((entry >> (32 + cache->cache_bits)) << cache->cache_bits);
                    int32_t operand = (int32_t)(entry & 0xFFFFFFFF);

                    // Format cache entry string
                    char cache_entry_str[64];
                    snprintf(cache_entry_str, sizeof(cache_entry_str), "{%d|%s}: [%u] %d", 
                            i, is_dirty ? "D" : "C", stored_address | i, operand);

                    // Create a new label for the cache entry
                    GtkWidget *label = gtk_label_new(cache_entry_str);
                    gtk_widget_set_name(label, "grid-cell");
                    gtk_widget_set_margin_top(label, 5);
                    gtk_widget_set_margin_bottom(label, 5);
                    gtk_widget_set_margin_start(label, 10);
                    gtk_widget_set_margin_end(label, 10);

                    // Determine the row and column for this cache entry
                    int row = i / 2;
                    int col = i % 2;

                    // Attach the label to the grid
                    gtk_grid_attach(GTK_GRID(right_upper_grid), label, col, row, 1, 1);
                }
            }
            gtk_widget_queue_draw(GTK_WIDGET(right_upper_grid));
        }
        backend_bridge->gui_interrupt_code = IC_NOTHING;
    }

    highlight_cell(left_grid, *backend_bridge->instruction_counter, &previous_slot_index);

    gtk_label_set_text(GTK_LABEL(cell_1_entry), strdup(get_non_empty_string(backend_bridge->instruction, "[n] instruction")));
    gtk_label_set_xalign(GTK_LABEL(cell_1_entry), 0.0);
    gtk_label_set_text(GTK_LABEL(cell_2_entry), strdup(get_non_empty_string(backend_bridge->coinstruction, "[m] co-instruction")));
    gtk_label_set_xalign(GTK_LABEL(cell_2_entry), 0.0);
    gtk_label_set_text(GTK_LABEL(cell_3_entry), strdup(get_non_empty_string(backend_bridge->cocoinstruction, "[s] co-co-instruction")));
    gtk_label_set_xalign(GTK_LABEL(cell_3_entry), 0.0);
    char accumulator_str[12];
    snprintf(accumulator_str, sizeof(accumulator_str), "ACCU: %d", *backend_bridge->accumulator);
    gtk_label_set_text(GTK_LABEL(accumulator), accumulator_str);

    // Update from queue
    //
    while (!is_empty(backend_bridge->change_queue)) {
        uint64_t value_gotten;
        bool is_writeback = false;
        dequeue_with_bit(backend_bridge->change_queue, &value_gotten, &is_writeback);
        if (is_writeback) {
            uint32_t address = (uint32_t)(value_gotten >> 32);
            int32_t operand = (int32_t)value_gotten;

            printf("Addrs %u", address);
            printf("Op %i", operand);

            // Format and update the label for the writeback
            char writeback_str[64];
            snprintf(writeback_str, sizeof(writeback_str), "[%u] %d", address, operand);

            GtkWidget *label = gtk_grid_get_child_at(GTK_GRID(left_grid), 0, address);
            if (label) {
                gtk_label_set_text(GTK_LABEL(label), writeback_str);
            } else {
                g_print("Warning: Writeback address %u not found in left grid.\n", address);
            }
        } else {
            uint8_t magic = 32 - backend_bridge->sdata_cell_cache->cache_bits;
            uint8_t cache_idx = (uint8_t)((value_gotten << magic) >> magic + 32);
            // printf("Idx %u\n", value_gotten);
            // printf("Idx %u\n", cache_idx);
            int32_t operand = (int32_t)value_gotten;

            // Format and update the label for the cache update
            char cache_update_str[64];
            snprintf(cache_update_str, sizeof(cache_update_str), "{%d|%s}: [%u] %d", cache_idx, "U", cache_idx, operand);

            int row = cache_idx / 2;
            int col = cache_idx % 2;

            GtkWidget *label = gtk_grid_get_child_at(GTK_GRID(right_upper_grid), col, row);
            if (label) {
                gtk_label_set_text(GTK_LABEL(label), cache_update_str);
            } else {
                g_print("Warning: Cache index %u not found in right grid.\n", cache_idx);
            }
        }
    }

    if (!*backend_bridge->executing) {
        if (g_strcmp0(current_label, "Start (Ctrl+g)") != 0) {
            gtk_button_set_label(button, "Start (Ctrl+g)");
        }
    } else {
        if (*backend_bridge->single_step_mode) {
            if (g_strcmp0(current_label, "Step (Ctrl+s)") != 0) {
                gtk_button_set_label(button, "Step (Ctrl+s)");
            }
        } else {
            if (g_strcmp0(current_label, "Stop (Ctrl+e)") != 0) {
                gtk_button_set_label(button, "Stop (Ctrl+e)");
            }
        }
    }

    mutex_unlock(backend_bridge->mutex);

    return G_SOURCE_CONTINUE; // Continue running in the main loop
}

static void *gui_main_loop(void *arg) {
    app = gtk_application_new("com.example.pASMc", G_APPLICATION_DEFAULT_FLAGS);

    // Connect the activation signal to the application
    g_signal_connect(app, "activate", G_CALLBACK(on_activate), NULL);

    /*/ Add actions for menu items
    for (int i = 1; i <= 8; i++) {
        char action_name[32];
        snprintf(action_name, sizeof(action_name), "cache_bits_%d", i);
        GSimpleAction *action = g_simple_action_new(action_name, NULL);
        g_signal_connect(action, "activate", G_CALLBACK(action_clbk), NULL);
        g_action_map_add_action(G_ACTION_MAP(app), G_ACTION(action));
    }

    // Add additional menu actions
    g_action_map_add_action(G_ACTION_MAP(app), G_ACTION(g_simple_action_new("file_open", NULL)));
    g_action_map_add_action(G_ACTION_MAP(app), G_ACTION(g_simple_action_new("file_close", NULL)));
    g_action_map_add_action(G_ACTION_MAP(app), G_ACTION(g_simple_action_new("file_reload", NULL)));
    g_action_map_add_action(G_ACTION_MAP(app), G_ACTION(g_simple_action_new("help_instruction_architecture", NULL)));
    g_action_map_add_action(G_ACTION_MAP(app), G_ACTION(g_simple_action_new("help_version", NULL)));
    */

    // Run the application
    printf("Run app\n");
    // Schedule periodic updates to run in the main GTK loop
    g_timeout_add(100, main_loop_update, NULL); // Run every 100ms
    g_application_run(G_APPLICATION(app), 0, NULL); // int status = 

    running = false;
    return NULL;
}

bool gtkgui_running() {
    return running;
}

bool gtkgui_start(Bridge *bridge) {
    if (running) {
        g_print("GUI already running.\n");
        return false;
    }

    running = true;
    backend_bridge = bridge;

    // Create the GUI thread using CTools
    if (thread_create(&gui_thread, gui_main_loop, NULL) != 0) {
        g_print("Failed to create GUI thread.\n");
        running = false;
        return false;
    }

    return true;
}

void gtkgui_stop() {
    if (!running && cleaned_up) return;

    // Request GTK to quit the application
    if (app) {
        g_idle_remove_by_data(NULL); // Remove all idle handlers
        g_application_quit(G_APPLICATION(app));
        g_object_unref(app);
        app = NULL;
    }

    // Join the thread
    thread_join(gui_thread);
    running = false;
    return;
}
