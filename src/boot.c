// Arranque de Mac OS 9: pantalla de bienvenida y, al instante, "Starting Up..." con barra de progreso.
// Los gráficos salen del diseño (assets/source/boot_*.png) y usan el banco 0 de VRAM.
#include "os.h"

// Dibuja el paso k de la barra: solo cambian unos pocos tiles (BOOT_BARRA_W x BOOT_BARRA_H)
static void dibujar_barra(UBYTE k) {
    UBYTE x, y;
    unsigned int i = (unsigned int)k * BOOT_BARRA_W * BOOT_BARRA_H;
    for (y = 0; y < BOOT_BARRA_H; y++)
        for (x = 0; x < BOOT_BARRA_W; x++, i++)
            ui_tile(BOOT_BARRA_X + x, BOOT_BARRA_Y + y, boot_barra_map[i], boot_barra_attr[i]);
}

void boot_ejecutar(void) {
    UBYTE p;
    cursor_mostrar(0);
    ui_fundido(boot_pal, 4, 1);                      // todo blanco mientras se prepara
    set_bkg_data(0, BOOT_TILE_COUNT, boot_tiles);
    ui_cargar_pantalla(boot_map_bienvenida, boot_attr_bienvenida);
    for (p = 4; p < 5; p--) { ui_fundido(boot_pal, p, 1); ui_esperar(5); }   // aparece (p baja de 4 a 0)
    ui_esperar(70);                                  // "Welcome to Mac OS"
    ui_cargar_pantalla(boot_map_carga, boot_attr_carga);   // cambia al instante a "Starting Up..."
    for (p = 1; p <= BOOT_PASOS; p++) { dibujar_barra(p); ui_esperar(9); }
    ui_esperar(20);
    for (p = 0; p <= 4; p++) { ui_fundido(boot_pal, p, 1); ui_esperar(5); }  // sale a blanco hacia el escritorio
}
