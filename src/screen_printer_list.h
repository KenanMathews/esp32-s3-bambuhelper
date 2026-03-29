#ifndef SCREEN_PRINTER_LIST_H
#define SCREEN_PRINTER_LIST_H

#include <stdint.h>

// Printer-selection screen — shows all active printer slots as tappable buttons.
// Call printerListEnter() to open, then printerListUpdate() each loop iteration.
// Returns: -1 = still open, -2 = timed out / dismissed, 0..N-1 = printer selected

void printerListEnter();
int8_t printerListUpdate();

#endif // SCREEN_PRINTER_LIST_H
