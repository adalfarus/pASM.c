#ifndef GTKGUI_H
#define GTKGUI_H

#include <stdbool.h>
#include "putils.h"

bool gtkgui_running();

// Initialize and run the GUI in a separate thread
bool gtkgui_start(Bridge *backend_bridge);

// Stop the GUI and clean up resources
void gtkgui_stop();

#endif // GTKGUI_H
