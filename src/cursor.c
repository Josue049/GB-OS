// Cursor: flecha clásica de 8x10 px en un solo sprite 8x16
#include "os.h"

void cursor_iniciar(void) {
    SPRITES_8x16;
    set_sprite_data(0, 2, cursor_tiles);
    set_sprite_tile(0, 0);
    set_sprite_prop(0, 0);
    set_sprite_palette(0, 1, cursor_pal);
}

void cursor_mover(UBYTE x, UBYTE y) {
    move_sprite(0, x + 8, y + 16);    // el hardware desplaza los sprites (8,16)
}

void cursor_mostrar(UBYTE si) {
    if (si) SHOW_SPRITES; else HIDE_SPRITES;
}
