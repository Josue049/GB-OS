// OS/GB - Mac OS 9 para Game Boy Color: punto de entrada
#include "os.h"

void main(void) {
    DISPLAY_OFF;
    config_cargar();           // lee el fondo elegido de la SRAM
    ui_cargar_tiles_ui();      // fuente y piezas de interfaz: banco 1, permanentes
    cursor_iniciar();
    SHOW_BKG;
    DISPLAY_ON;
    for (;;) {
        boot_ejecutar();       // pantalla de inicio
        desktop_ejecutar();    // escritorio hasta Restart / Shut Down
    }
}
