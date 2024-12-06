#ifndef GTKGUI_H
#define GTKGUI_H

#include <stdbool.h>

bool gtkgui_running();

// Initialize and run the GUI in a separate thread
bool gtkgui_start();

// Stop the GUI and clean up resources
void gtkgui_stop();

#endif // GTKGUI_H
