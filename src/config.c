// Ajustes guardados en la SRAM del cartucho (sobreviven al apagado)
#include "os.h"

#define SRAM ((volatile UBYTE *)0xA000)

static UBYTE fondo = 0;

// Formato: 'O' 'S' fondo (fondo ^ 0xA5). Si no cuadra (SRAM nueva o corrupta) se usa el fondo por defecto
void config_cargar(void) {
    ENABLE_RAM;
    if (SRAM[0] == 'O' && SRAM[1] == 'S' && SRAM[2] < NUM_FONDOS && SRAM[3] == (UBYTE)(SRAM[2] ^ 0xA5))
        fondo = SRAM[2];
    else
        fondo = 0;
    DISABLE_RAM;
}

UBYTE config_fondo(void) { return fondo; }

void config_guardar_fondo(UBYTE n) {
    fondo = n;
    ENABLE_RAM;
    SRAM[0] = 'O';
    SRAM[1] = 'S';
    SRAM[2] = n;
    SRAM[3] = n ^ 0xA5;
    DISABLE_RAM;
}
