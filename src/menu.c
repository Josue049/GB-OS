// Menús desplegables de la barra superior (Apple y Special)
#include "os.h"

typedef struct {
    UBYTE x, ancho;            // posición y ancho del desplegable en tiles
    UBYTE tit_x, tit_ancho;    // tiles de la barra que se resaltan al abrirlo
    UBYTE px0, px1;            // zona del título en píxeles (para el clic)
    UBYTE n;
    const char *texto[4];
    UBYTE accion[4];
} Menu;

static const Menu menus[2] = {
    { 0, 16, 0, 2, 0, 20, 4, { "About This Mac", "Control Panels", "Macintosh HD", "Restart" },
      { ACC_ABOUT, ACC_PANEL, ACC_ABRIR_HD, ACC_REINICIAR } },
    { 9, 11, 9, 4, 79, 106, 2, { "Restart", "Shut Down", "", "" },
      { ACC_REINICIAR, ACC_APAGAR, ACC_NINGUNA, ACC_NINGUNA } },
};

static BYTE abierto = -1;
static BYTE resaltado = -1;

BYTE menu_titulo_en(UBYTE x, UBYTE y) {
    UBYTE i;
    if (y >= 10) return -1;
    for (i = 0; i < 2; i++)
        if (x >= menus[i].px0 && x < menus[i].px1) return i;
    return -1;
}

UBYTE menu_abierto(void) { return abierto >= 0; }

void menu_abrir(UBYTE n) {
    const Menu *m = &menus[n];
    UBYTE i, j, fila;
    abierto = n;
    resaltado = -1;
    ui_paleta_rect(m->tit_x, 0, m->tit_ancho, 1, PAL_SEL);     // título invertido
    for (j = 0; j < m->n + 2; j++) {
        fila = j == 0 ? UI_MENU_TOP_L : (j == m->n + 1 ? UI_MENU_BOT_L : UI_MENU_MID_L);
        // Las tres piezas de cada fila son consecutivas: izquierda, centro, derecha
        ui_tile(m->x, 1 + j, fila, ATTR_BANCO1 | PAL_MENU);
        for (i = 1; i < m->ancho - 1; i++) ui_tile(m->x + i, 1 + j, fila + 1, ATTR_BANCO1 | PAL_MENU);
        ui_tile(m->x + m->ancho - 1, 1 + j, fila + 2, ATTR_BANCO1 | PAL_MENU);
        if (j >= 1 && j <= m->n) ui_texto(m->x + 1, 1 + j, m->texto[j - 1], UI_FONT, PAL_MENU, m->ancho - 2);
    }
}

void menu_cerrar(void) {
    const Menu *m;
    if (abierto < 0) return;
    m = &menus[abierto];
    ui_restaurar_escritorio(m->x, 1, m->ancho, m->n + 2);
    ui_paleta_restaurar(m->tit_x, 0, m->tit_ancho, 1);
    abierto = resaltado = -1;
}

// Devuelve el índice de opción bajo el cursor, o -1
static BYTE opcion_en(UBYTE x, UBYTE y) {
    const Menu *m = &menus[abierto];
    UBYTE fx = x >> 3, fy = y >> 3;
    if (fx > m->x && fx < m->x + m->ancho - 1 && fy >= 2 && fy < 2 + m->n) return fy - 2;
    return -1;
}

void menu_hover(UBYTE x, UBYTE y) {
    BYTE o = opcion_en(x, y);
    const Menu *m = &menus[abierto];
    if (o == resaltado) return;
    if (resaltado >= 0) ui_paleta_restaurar(m->x + 1, 2 + resaltado, m->ancho - 2, 1);
    if (o >= 0) ui_paleta_rect(m->x + 1, 2 + o, m->ancho - 2, 1, PAL_SEL);
    resaltado = o;
}

UBYTE menu_clic(UBYTE x, UBYTE y) {
    BYTE o = opcion_en(x, y);
    return o < 0 ? ACC_NINGUNA : menus[abierto].accion[o];
}
