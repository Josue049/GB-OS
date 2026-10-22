// Escritorio: carga del diseño, icono Macintosh HD, launcher y bucle principal
#include "os.h"

static UBYTE hd_seleccionado = 0;

// Botones del launcher: zona en píxeles, tiles que se iluminan al pulsar y acción
typedef struct { UBYTE x0, x1, col, ncols, accion; } BotonLauncher;
static const BotonLauncher launcher[] = {
    { 18, 37, 2, 3, ACC_ABRIR_HD },    // icono de ventana: abre Macintosh HD
    { 37, 56, 5, 2, ACC_NINGUNA },
    { 56, 76, 7, 2, ACC_ABOUT },       // monitor: About
    { 76, 96, 10, 2, ACC_NINGUNA },
};

// Los dos escritorios disponibles (generados por tools/convert_assets.py)
static const unsigned char *const fondo_tiles[NUM_FONDOS] = { desktop0_tiles, desktop1_tiles };
static const unsigned char *const fondo_map[NUM_FONDOS] = { desktop0_map, desktop1_map };
static const unsigned char *const fondo_attr[NUM_FONDOS] = { desktop0_attr, desktop1_attr };
static const unsigned int *const fondo_pal[NUM_FONDOS] = { desktop0_pal, desktop1_pal };
static const unsigned char fondo_ntiles[NUM_FONDOS] = { DESKTOP0_TILE_COUNT, DESKTOP1_TILE_COUNT };

static const unsigned int *pal_actual;      // paletas del escritorio activo (para los fundidos)

// Carga tiles y mapa del fondo elegido (la pantalla se ve blanca o apagada mientras tanto)
static void cargar_fondo(UBYTE n) {
    pal_actual = fondo_pal[n];
    set_bkg_data(0, fondo_ntiles[n], fondo_tiles[n]);
    ui_elegir_fondo(fondo_map[n], fondo_attr[n]);
    ui_cargar_pantalla(fondo_map[n], fondo_attr[n]);
}

void desktop_cargar(void) {
    pal_actual = fondo_pal[config_fondo()];
    ui_fundido(pal_actual, 4, 1);                   // blanco mientras se carga
    cargar_fondo(config_fondo());
    hd_seleccionado = 0;
}

// Cambio de fondo desde Control Panels: se aplica al instante y se guarda
void desktop_set_fondo(UBYTE n) {
    if (n == config_fondo()) return;
    config_guardar_fondo(n);
    DISPLAY_OFF;
    cargar_fondo(n);
    set_bkg_palette(0, 8, pal_actual);
    desktop_aplicar_seleccion();
    if (ventana_abierta()) ventana_dibujar();       // la ventana del panel se repinta encima
    DISPLAY_ON;
}

// Icono (2x2 tiles) y rótulo bajo él
static void pintar_hd(UBYTE sel) {
    if (sel) {
        ui_paleta_rect(HD_ICON_X, HD_ICON_Y, 2, 2, PAL_SEL);
        ui_paleta_rect(HD_LABEL_X0, HD_LABEL_Y, HD_LABEL_X1 - HD_LABEL_X0 + 1, 1, PAL_SEL);
    } else {
        ui_paleta_restaurar(HD_ICON_X, HD_ICON_Y, 2, 2);
        ui_paleta_restaurar(HD_LABEL_X0, HD_LABEL_Y, HD_LABEL_X1 - HD_LABEL_X0 + 1, 1);
    }
}

void desktop_aplicar_seleccion(void) { if (hd_seleccionado) pintar_hd(1); }

void desktop_deseleccionar(void) {
    if (hd_seleccionado) { hd_seleccionado = 0; pintar_hd(0); }
}

UBYTE desktop_clic(UBYTE x, UBYTE y) {
    UBYTE i, tx = x >> 3, ty = y >> 3;
    // Icono Macintosh HD: un clic selecciona, el segundo lo abre
    if ((tx >= HD_ICON_X && tx < HD_ICON_X + 2 && ty >= HD_ICON_Y && ty < HD_ICON_Y + 2) ||
        (tx >= HD_LABEL_X0 && tx <= HD_LABEL_X1 && ty == HD_LABEL_Y)) {
        if (hd_seleccionado) return ACC_ABRIR_HD;
        hd_seleccionado = 1;
        pintar_hd(1);
        return ACC_NINGUNA;
    }
    // Launcher inferior
    if (y >= 129 && x < 96)
        for (i = 0; i < 4; i++)
            if (x >= launcher[i].x0 && x < launcher[i].x1) {
                ui_paleta_rect(launcher[i].col, 16, launcher[i].ncols, 2, PAL_SEL);   // pulso visual
                ui_esperar(8);
                ui_paleta_restaurar(launcher[i].col, 16, launcher[i].ncols, 2);
                return launcher[i].accion;
            }
    desktop_deseleccionar();
    return ACC_NINGUNA;
}

static void ejecutar_accion(UBYTE a) {
    UBYTE tipo = a == ACC_ABOUT ? VENT_ABOUT : (a == ACC_PANEL ? VENT_PANEL : VENT_FINDER);
    if (a == ACC_ABRIR_HD || a == ACC_ABOUT || a == ACC_PANEL) {
        if (ventana_abierta()) { ventana_cerrar(); desktop_aplicar_seleccion(); }
        ventana_abrir(tipo);
    }
}

static void apagar(void) {
    UBYTE p;
    cursor_mostrar(0);
    for (p = 0; p <= 4; p++) { ui_fundido(pal_actual, p, 0); ui_esperar(5); }
    while (joypad()) vsync();
    while (!joypad()) vsync();               // cualquier botón vuelve a encender
}

// Bucle del escritorio: devuelve cuando hay que reiniciar
UBYTE desktop_ejecutar(void) {
    UBYTE cx = 80, cy = 72, prev = 0, j, p, ace = 0, paso, accion, hecho = 0;
    BYTE t;

    desktop_cargar();
    if (ventana_abierta()) ventana_cerrar();
    cursor_mover(cx, cy);
    cursor_mostrar(1);
    for (p = 4; p < 5; p--) { ui_fundido(pal_actual, p, 1); ui_esperar(4); }   // aparece desde blanco

    while (!hecho) {
        vsync();
        j = joypad();
        p = j & ~prev;
        prev = j;

        // Movimiento: acelera si se mantiene la cruceta
        if (j & (J_LEFT | J_RIGHT | J_UP | J_DOWN)) { if (ace < 255) ace++; } else ace = 0;
        paso = ace > 24 ? 3 : (ace > 10 ? 2 : 1);
        if ((j & J_LEFT) && cx >= paso) cx -= paso;
        if ((j & J_RIGHT) && cx + paso < 160) cx += paso;
        if ((j & J_UP) && cy >= paso) cy -= paso;
        if ((j & J_DOWN) && cy + paso < 144) cy += paso;
        cursor_mover(cx, cy);

        if (menu_abierto()) menu_hover(cx, cy);

        if (p & J_B) {                                   // B: retroceder (menú > ventana > selección)
            if (menu_abierto()) { menu_cerrar(); desktop_aplicar_seleccion(); if (ventana_abierta()) ventana_dibujar(); }
            else if (ventana_abierta()) { ventana_cerrar(); desktop_aplicar_seleccion(); }
            else desktop_deseleccionar();
        }
        if (p & J_A) {                                   // A: seleccionar / pulsar
            accion = ACC_NINGUNA;
            if (menu_abierto()) {
                accion = menu_clic(cx, cy);
                menu_cerrar();
                desktop_aplicar_seleccion();
                if (ventana_abierta()) ventana_dibujar();
            } else if ((t = menu_titulo_en(cx, cy)) >= 0) {
                menu_abrir(t);
            } else if (ventana_contiene(cx, cy)) {
                ventana_clic(cx, cy);
                if (!ventana_abierta()) desktop_aplicar_seleccion();
            } else {
                accion = desktop_clic(cx, cy);
            }
            if (accion == ACC_REINICIAR) hecho = 1;
            else if (accion == ACC_APAGAR) { apagar(); hecho = 1; }
            else if (accion) ejecutar_accion(accion);
        }
    }
    if (ventana_abierta()) ventana_cerrar();
    for (p = 0; p <= 4; p++) { ui_fundido(pal_actual, p, 1); ui_esperar(4); }   // sale a blanco
    return 0;
}
