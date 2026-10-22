// Ventanas estilo Mac OS 9: Finder (Macintosh HD), About y el panel Appearance > Desktop
#include "os.h"

static UBYTE tipo = 0;
static UBYTE vx, vy, vw, vh;       // posición y tamaño de la ventana abierta, en tiles
static BYTE fila_sel = -1;         // fila seleccionada en el Finder
static UBYTE sel_panel = 0;        // fondo marcado en la lista del panel (se aplica con "Set Desktop")

static const char *titulo_de(void) {
    return tipo == VENT_FINDER ? "Macintosh HD" : (tipo == VENT_PANEL ? "Appearance" : "About");
}

// Dibuja el marco de la ventana (barra de título con botón de cierre, bordes y fondo)
static void marco(UBYTE x, UBYTE y, UBYTE w, UBYTE h) {
    UBYTE i, j, a = ATTR_BANCO1 | PAL_VENTANA;
    ui_tile(x, y, UI_WIN_CLOSE, a);
    for (i = 1; i < w - 1; i++) ui_tile(x + i, y, UI_WIN_TITLE, a);
    ui_tile(x + w - 1, y, UI_WIN_TITLE_R, a);
    for (j = 1; j < h - 1; j++) {
        ui_tile(x, y + j, UI_WIN_L, a);
        for (i = 1; i < w - 1; i++) ui_tile(x + i, y + j, UI_WIN_BODY, a);
        ui_tile(x + w - 1, y + j, UI_WIN_R, a);
    }
    ui_tile(x, y + h - 1, UI_WIN_BOT_L, a);
    for (i = 1; i < w - 1; i++) ui_tile(x + i, y + h - 1, UI_WIN_BOT, a);
    ui_tile(x + w - 1, y + h - 1, UI_WIN_BOT_R, a);
}

// Panel Desktop: pestaña, miniatura del fondo elegido, lista de fondos y botón "Set Desktop"
static void contenido_panel(void) {
    static const char *nombres[NUM_FONDOS] = { "Lavender", "Blue Face" };
    UBYTE i, j, pal;
    ui_titulo(vx + 1, vy + 1, "Desktop", PAL_VENTANA, vw - 2);         // pestaña activa
    // Miniatura (5x4 tiles). La del fondo activo usa la paleta 0 y la otra la 7
    pal = sel_panel == config_fondo() ? 0 : 7;
    for (j = 0; j < 4; j++)
        for (i = 0; i < 5; i++)
            ui_tile(vx + 1 + i, vy + 3 + j, (sel_panel ? UI_THUMB1_0 : UI_THUMB0_0) + j * 5 + i, ATTR_BANCO1 | pal);
    ui_texto(vx + 7, vy + 3, "Patterns:", UI_FONT, PAL_VENTANA, 10);
    for (i = 0; i < NUM_FONDOS; i++) {
        ui_texto(vx + 7, vy + 4 + i, nombres[i], UI_FONT, PAL_VENTANA, 10);
        if (i == sel_panel) ui_paleta_rect(vx + 7, vy + 4 + i, 10, 1, PAL_SEL);
    }
    ui_texto(vx + 5, vy + 8, " Set Desktop", UI_FONT, PAL_SEL, 12);   // botón oscuro
}

static void contenido(void) {
    // Título centrado, con un espacio a cada lado para cortar las rayas de la barra
    char buf[16] = " ";
    UBYTE n;
    strcat(buf, titulo_de());
    strcat(buf, " ");
    n = strlen(buf);
    ui_titulo(vx + (vw - n) / 2, vy, buf, PAL_VENTANA, n);
    if (tipo == VENT_FINDER) {
        static const char *nombres[3] = { "System Folder", "Applications", "Documents" };
        ui_texto(vx + 1, vy + 1, "3 items", UI_FONT, PAL_VENTANA, vw - 2);
        for (n = 0; n < 3; n++) {
            ui_tile(vx + 1, vy + 2 + n, UI_FOLDER, ATTR_BANCO1 | PAL_VENTANA);
            ui_texto(vx + 2, vy + 2 + n, nombres[n], UI_FONT, PAL_VENTANA, vw - 3);
        }
        if (fila_sel >= 0) ui_paleta_rect(vx + 1, vy + 2 + fila_sel, vw - 2, 1, PAL_SEL);
    } else if (tipo == VENT_PANEL) {
        contenido_panel();
    } else {
        ui_texto(vx + 1, vy + 2, "Mac OS 9 GB", UI_FONT, PAL_VENTANA, vw - 2);
        ui_texto(vx + 1, vy + 3, "Game Boy Color", UI_FONT, PAL_VENTANA, vw - 2);
        ui_texto(vx + 1, vy + 5, "Version 0.1", UI_FONT, PAL_VENTANA, vw - 2);
    }
}

void ventana_dibujar(void) {
    marco(vx, vy, vw, vh);
    contenido();
}

// Animación sencilla de "zoom": el marco crece (o se encoge) desde el centro
static void marco_zoom(UBYTE paso) {
    UBYTE w = vw * paso / 4, h = vh * paso / 4;
    if (w < 3) w = 3;
    if (h < 3) h = 3;
    marco(vx + (vw - w) / 2, vy + (vh - h) / 2, w, h);
}

void ventana_abrir(UBYTE t) {
    UBYTE paso;
    tipo = t;
    fila_sel = -1;
    sel_panel = config_fondo();
    vy = 3;
    if (t == VENT_PANEL) { vx = 1; vw = 18; vh = 11; }
    else { vx = 2; vw = 16; vh = t == VENT_FINDER ? 10 : 8; }
    for (paso = 1; paso < 4; paso++) { marco_zoom(paso); ui_esperar(3); }
    ventana_dibujar();
}

void ventana_cerrar(void) {
    UBYTE paso;
    for (paso = 3; paso > 0; paso--) {
        ui_restaurar_escritorio(vx, vy, vw, vh);
        marco_zoom(paso);
        ui_esperar(3);
    }
    ui_restaurar_escritorio(vx, vy, vw, vh);
    tipo = 0;
}

UBYTE ventana_abierta(void) { return tipo != 0; }

UBYTE ventana_contiene(UBYTE x, UBYTE y) {
    return tipo && x >= vx * 8 && x < (vx + vw) * 8 && y >= vy * 8 && y < (vy + vh) * 8;
}

void ventana_clic(UBYTE x, UBYTE y) {
    UBYTE fx = x >> 3, fy = y >> 3;
    if (fy == vy && fx == vx) { ventana_cerrar(); return; }          // botón de cierre
    if (tipo == VENT_PANEL) {
        if (fy >= vy + 4 && fy < vy + 4 + NUM_FONDOS && fx >= vx + 7 && fx < vx + 17) {
            sel_panel = fy - vy - 4;                                  // elegir en la lista
            contenido_panel();
        } else if (fy == vy + 8 && fx >= vx + 5 && fx < vx + 17) {
            desktop_set_fondo(sel_panel);                             // "Set Desktop": aplica y guarda
        }
        return;
    }
    if (tipo == VENT_FINDER && fy >= vy + 2 && fy < vy + 5 && fx > vx && fx < vx + vw - 1) {
        if (fila_sel >= 0) ui_paleta_restaurar(vx + 1, vy + 2 + fila_sel, vw - 2, 1);
        fila_sel = fy - vy - 2;
        ui_paleta_rect(vx + 1, fy, vw - 2, 1, PAL_SEL);
    }
}
