// Definiciones comunes de OS/GB (Mac OS 9 para Game Boy Color)
#ifndef OS_H
#define OS_H

#include <gb/gb.h>
#include <gb/cgb.h>
#include <string.h>
#include "gen_graficos.h"

#define PAL_SEL      6      // paleta de "selección" (texto blanco sobre azul)
#define ATTR_BANCO1  0x08   // atributo CGB: el tile está en el banco 1 de VRAM (fuente y piezas de UI)
#define PAL_VENTANA  2
#define PAL_MENU     1

// Acciones que pueden devolver menús, launcher e iconos
enum { ACC_NINGUNA, ACC_ABOUT, ACC_ABRIR_HD, ACC_PANEL, ACC_REINICIAR, ACC_APAGAR };

// ---- ui.c: utilidades de pantalla (tiles, texto, paletas, fundidos)
void ui_cargar_tiles_ui(void);
void ui_cargar_pantalla(const UBYTE *mapa, const UBYTE *attr);
void ui_tile(UBYTE x, UBYTE y, UBYTE tile, UBYTE attr);
void ui_texto(UBYTE x, UBYTE y, const char *s, UBYTE base, UBYTE attr, UBYTE ancho);
void ui_elegir_fondo(const UBYTE *mapa, const UBYTE *attr);   // escritorio que usa ui_restaurar_escritorio
void ui_titulo(UBYTE x, UBYTE y, const char *s, UBYTE attr, UBYTE ancho);
void ui_restaurar_escritorio(UBYTE x, UBYTE y, UBYTE w, UBYTE h);
void ui_paleta_rect(UBYTE x, UBYTE y, UBYTE w, UBYTE h, UBYTE pal);
void ui_paleta_restaurar(UBYTE x, UBYTE y, UBYTE w, UBYTE h);
void ui_fundido(const unsigned int *pal, UBYTE paso, UBYTE a_blanco);
void ui_esperar(UBYTE frames);

// ---- config.c: ajustes guardados en SRAM (batería)
#define NUM_FONDOS 2     // 0 = patrón morado, 1 = Blue Finder
void  config_cargar(void);
UBYTE config_fondo(void);
void  config_guardar_fondo(UBYTE n);

// ---- cursor.c
void cursor_iniciar(void);
void cursor_mover(UBYTE x, UBYTE y);
void cursor_mostrar(UBYTE si);

// ---- boot.c
void boot_ejecutar(void);

// ---- menu.c
BYTE  menu_titulo_en(UBYTE x, UBYTE y);   // 0 = Apple, 1 = Special, -1 = ninguno
UBYTE menu_abierto(void);
void  menu_abrir(UBYTE n);
void  menu_cerrar(void);
void  menu_hover(UBYTE x, UBYTE y);
UBYTE menu_clic(UBYTE x, UBYTE y);        // devuelve la acción elegida (y no cierra el menú)

// ---- window.c
void  ventana_abrir(UBYTE tipo);
void  ventana_cerrar(void);
void  ventana_dibujar(void);
UBYTE ventana_abierta(void);
UBYTE ventana_contiene(UBYTE x, UBYTE y);
void  ventana_clic(UBYTE x, UBYTE y);     // cierra con el botón de cierre o selecciona una fila
#define VENT_FINDER 1
#define VENT_ABOUT  2
#define VENT_PANEL  3     // Control Panels > Appearance > Desktop

// ---- desktop.c
void  desktop_cargar(void);
void  desktop_aplicar_seleccion(void);
UBYTE desktop_clic(UBYTE x, UBYTE y);     // iconos y launcher; devuelve una acción
void  desktop_deseleccionar(void);
void  desktop_set_fondo(UBYTE n);          // cambia el fondo al instante y lo guarda en SRAM
UBYTE desktop_ejecutar(void);

#endif
