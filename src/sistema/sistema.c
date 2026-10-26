// Ubuntu GB y Windows 11 GB: arranque, escritorio con zonas clicables, menú y ventanas.
// Se compila una vez por sistema con -DSIS_UBUNTU o -DSIS_WINDOWS; los gráficos vienen de
// assets/generated/<sistema>/ (tools/convert_sistema.py) y ui.c / cursor.c son los mismos que en Mac OS 9.
#include "os.h"

#define SP_UI   6      // paleta de ventanas y menús (tema del sistema)
#define SP_SEL  7      // paleta de selección (texto blanco sobre el color de acento)
#define ACC_VENT 0     // 0..n-1 abren la ventana con ese número
#define ACC_NADA     255
#define ACC_REINICIO 100
#define ACC_APAGADO  101

typedef struct { const char *titulo; UBYTE carpetas; const char *lineas[4]; } Ventana;
typedef struct { UBYTE x0, y0, x1, y1; BYTE accion; } Zona;     // accion: ventana, o -1 = abrir el menú
typedef struct { const char *texto; UBYTE accion; } Opcion;

#if defined(SIS_UBUNTU)
static const Ventana ventanas[] = {
    { "About",        0, { "Ubuntu GB", "Game Boy Color", "", "Version 0.1" } },
    { "Firefox",      0, { "Web Browser", "", "No network :(", "" } },
    { "Files",        1, { "Home", "Documents", "Downloads", "Pictures" } },
    { "Applications", 0, { "Files", "Firefox", "Terminal", "Settings" } },
};
static const Zona zonas[] = {
    {   0,   0,  46,  10,  3 },     // Actividades
    { 134,   0, 160,  10, -1 },     // bandeja del sistema
    {   2,  14,  24,  34,  0 },     // logo de Ubuntu
    {   2,  36,  24,  58,  1 },     // Firefox
    {   2,  60,  24,  82,  2 },     // Archivos
    {   2, 112,  24, 136,  3 },     // Mostrar aplicaciones
};
static const Opcion opciones[] = { { "Restart", ACC_REINICIO }, { "Power Off", ACC_APAGADO } };
#define MENU_X 9
#define MENU_Y 1
#define MENU_W 11
#define VENT_X 4
#define VENT_W 15
#elif defined(SIS_WINDOWS)
static const Ventana ventanas[] = {
    { "About",    0, { "Windows 11 GB", "Game Boy Color", "", "Version 0.1" } },
    { "Explorer", 1, { "Documents", "Pictures", "Downloads", "Desktop" } },
    { "Edge",     0, { "Microsoft Edge", "", "No network :(", "" } },
};
static const Zona zonas[] = {
    {  58, 130,  72, 144, -1 },     // Inicio
    {  73, 130,  89, 144,  1 },     // Explorador de archivos
    {  90, 130, 104, 144,  2 },     // Edge
};
static const Opcion opciones[] = {
    { "About", ACC_VENT + 0 }, { "Explorer", ACC_VENT + 1 }, { "Restart", ACC_REINICIO }, { "Shut Down", ACC_APAGADO },
};
#define MENU_X 5
#define MENU_Y 10
#define MENU_W 12
#define VENT_X 2
#define VENT_W 16
#else
#error "Define SIS_UBUNTU o SIS_WINDOWS"
#endif

#define NUM_ZONAS   (sizeof(zonas) / sizeof(zonas[0]))
#define NUM_OPCIONES (sizeof(opciones) / sizeof(opciones[0]))
#define VENT_Y 2
#define VENT_H 10

static BYTE vent = -1;            // ventana abierta (-1 = ninguna)
static BYTE fila_sel = -1;        // fila marcada en las ventanas de carpetas
static UBYTE mn_visible = 0;
static BYTE resaltado = -1;       // opción del menú bajo el cursor

// ---------------------------------------------------------------- arranque
static void dibujar_barra(UBYTE k) {
    UBYTE x, y;
    unsigned int i = (unsigned int)k * BOOT_BARRA_W * BOOT_BARRA_H;
    for (y = 0; y < BOOT_BARRA_H; y++)
        for (x = 0; x < BOOT_BARRA_W; x++, i++)
            ui_tile(BOOT_BARRA_X + x, BOOT_BARRA_Y + y, boot_barra_map[i], boot_barra_attr[i]);
}

static void arrancar(void) {
    UBYTE p;
    cursor_mostrar(0);
    ui_fundido(boot_pal, 4, 0);                      // todo negro mientras se prepara
    set_bkg_data(0, BOOT_TILE_COUNT, boot_tiles);
    ui_cargar_pantalla(boot_map_carga, boot_attr_carga);
    for (p = 4; p < 5; p--) { ui_fundido(boot_pal, p, 0); ui_esperar(5); }   // aparece (p baja de 4 a 0)
    ui_esperar(20);
    for (p = 1; p <= BOOT_PASOS; p++) { dibujar_barra(p); ui_esperar(9); }
    ui_esperar(20);
    for (p = 0; p <= 4; p++) { ui_fundido(boot_pal, p, 0); ui_esperar(5); }  // se apaga hacia el escritorio
}

// ---------------------------------------------------------------- menú
static void mn_abrir(void) {
    UBYTE i, j, fila;
    mn_visible = 1;
    resaltado = -1;
    for (j = 0; j < NUM_OPCIONES + 2; j++) {
        fila = j == 0 ? UI_MENU_TOP_L : (j == NUM_OPCIONES + 1 ? UI_MENU_BOT_L : UI_MENU_MID_L);
        ui_tile(MENU_X, MENU_Y + j, fila, ATTR_BANCO1 | SP_UI);
        for (i = 1; i < MENU_W - 1; i++) ui_tile(MENU_X + i, MENU_Y + j, fila + 1, ATTR_BANCO1 | SP_UI);
        ui_tile(MENU_X + MENU_W - 1, MENU_Y + j, fila + 2, ATTR_BANCO1 | SP_UI);
        if (j >= 1 && j <= NUM_OPCIONES)
            ui_texto(MENU_X + 1, MENU_Y + j, opciones[j - 1].texto, UI_FONT, SP_UI, MENU_W - 2);
    }
}

static void mn_cerrar(void) {
    if (!mn_visible) return;
    ui_restaurar_escritorio(MENU_X, MENU_Y, MENU_W, NUM_OPCIONES + 2);
    mn_visible = 0;
    resaltado = -1;
}

static BYTE opcion_en(UBYTE x, UBYTE y) {
    UBYTE fx = x >> 3, fy = y >> 3;
    if (fx > MENU_X && fx < MENU_X + MENU_W - 1 && fy > MENU_Y && fy <= MENU_Y + NUM_OPCIONES) return fy - MENU_Y - 1;
    return -1;
}

static void mn_hover(UBYTE x, UBYTE y) {
    BYTE o = opcion_en(x, y);
    if (o == resaltado) return;
    if (resaltado >= 0) ui_paleta_restaurar(MENU_X + 1, MENU_Y + 1 + resaltado, MENU_W - 2, 1);
    if (o >= 0) ui_paleta_rect(MENU_X + 1, MENU_Y + 1 + o, MENU_W - 2, 1, SP_SEL);
    resaltado = o;
}

// ---------------------------------------------------------------- ventanas
static void marco(UBYTE x, UBYTE y, UBYTE w, UBYTE h) {
    UBYTE i, j, a = ATTR_BANCO1 | SP_UI;
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

static void vn_dibujar(void) {
    const Ventana *v = &ventanas[vent];
    char buf[20] = " ";
    UBYTE n, i;
    marco(VENT_X, VENT_Y, VENT_W, VENT_H);
    strcat(buf, v->titulo);
    strcat(buf, " ");
    n = strlen(buf);
    ui_titulo(VENT_X + (VENT_W - n) / 2, VENT_Y, buf, SP_UI, n);
    for (i = 0; i < 4; i++) {
        if (v->carpetas) {
            ui_tile(VENT_X + 1, VENT_Y + 2 + i, UI_FOLDER, ATTR_BANCO1 | SP_UI);
            ui_texto(VENT_X + 2, VENT_Y + 2 + i, v->lineas[i], UI_FONT, SP_UI, VENT_W - 3);
        } else {
            ui_texto(VENT_X + 1, VENT_Y + 2 + i, v->lineas[i], UI_FONT, SP_UI, VENT_W - 2);
        }
    }
    if (v->carpetas && fila_sel >= 0) ui_paleta_rect(VENT_X + 1, VENT_Y + 2 + fila_sel, VENT_W - 2, 1, SP_SEL);
}

// "Zoom" sencillo: el marco crece (o se encoge) desde el centro
static void marco_zoom(UBYTE paso) {
    UBYTE w = VENT_W * paso / 4, h = VENT_H * paso / 4;
    if (w < 3) w = 3;
    if (h < 3) h = 3;
    marco(VENT_X + (VENT_W - w) / 2, VENT_Y + (VENT_H - h) / 2, w, h);
}

static void vn_abrir(BYTE n) {
    UBYTE paso;
    vent = n;
    fila_sel = -1;
    for (paso = 1; paso < 4; paso++) { marco_zoom(paso); ui_esperar(3); }
    vn_dibujar();
}

static void vn_cerrar(void) {
    UBYTE paso;
    for (paso = 3; paso > 0; paso--) {
        ui_restaurar_escritorio(VENT_X, VENT_Y, VENT_W, VENT_H);
        marco_zoom(paso);
        ui_esperar(3);
    }
    ui_restaurar_escritorio(VENT_X, VENT_Y, VENT_W, VENT_H);
    vent = -1;
}

static UBYTE vn_contiene(UBYTE x, UBYTE y) {
    return vent >= 0 && x >= VENT_X * 8 && x < (VENT_X + VENT_W) * 8 && y >= VENT_Y * 8 && y < (VENT_Y + VENT_H) * 8;
}

static void vn_clic(UBYTE x, UBYTE y) {
    UBYTE fx = x >> 3, fy = y >> 3;
    if (fy == VENT_Y && fx == VENT_X) { vn_cerrar(); return; }       // botón de cierre
    if (ventanas[vent].carpetas && fy >= VENT_Y + 2 && fy < VENT_Y + 6 && fx > VENT_X && fx < VENT_X + VENT_W - 1) {
        if (fila_sel >= 0) ui_paleta_restaurar(VENT_X + 1, VENT_Y + 2 + fila_sel, VENT_W - 2, 1);
        fila_sel = fy - VENT_Y - 2;
        ui_paleta_rect(VENT_X + 1, fy, VENT_W - 2, 1, SP_SEL);
    }
}

// ---------------------------------------------------------------- escritorio
static void apagar(void) {
    UBYTE p;
    cursor_mostrar(0);
    for (p = 0; p <= 4; p++) { ui_fundido(desktop0_pal, p, 0); ui_esperar(5); }
    while (joypad()) vsync();
    while (!joypad()) vsync();               // cualquier botón vuelve a encender
}

static void abrir_ventana(BYTE n) {
    if (vent >= 0) vn_cerrar();
    vn_abrir(n);
}

// Un clic sobre el escritorio: abre el menú o la ventana de la zona pulsada
static void clic_escritorio(UBYTE x, UBYTE y) {
    UBYTE i;
    for (i = 0; i < NUM_ZONAS; i++)
        if (x >= zonas[i].x0 && x < zonas[i].x1 && y >= zonas[i].y0 && y < zonas[i].y1) {
            if (zonas[i].accion < 0) mn_abrir(); else abrir_ventana(zonas[i].accion);
            return;
        }
}

static void escritorio(void) {
    UBYTE cx = 80, cy = 72, prev = 0, j, p, ace = 0, paso, hecho = 0, accion;
    BYTE o;

    ui_fundido(desktop0_pal, 4, 1);
    set_bkg_data(0, DESKTOP0_TILE_COUNT, desktop0_tiles);
    ui_elegir_fondo(desktop0_map, desktop0_attr);
    ui_cargar_pantalla(desktop0_map, desktop0_attr);
    vent = -1;
    mn_visible = 0;
    cursor_mover(cx, cy);
    cursor_mostrar(1);
    for (p = 4; p < 5; p--) { ui_fundido(desktop0_pal, p, 1); ui_esperar(4); }   // aparece desde blanco

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

        if (mn_visible) mn_hover(cx, cy);

        if (p & J_B) {                                   // B: retroceder (menú > ventana)
            if (mn_visible) { mn_cerrar(); if (vent >= 0) vn_dibujar(); }
            else if (vent >= 0) vn_cerrar();
        }
        if (p & J_A) {                                   // A: seleccionar / pulsar
            accion = ACC_NADA;
            if (mn_visible) {
                o = opcion_en(cx, cy);
                mn_cerrar();
                if (vent >= 0) vn_dibujar();
                if (o >= 0) accion = opciones[o].accion;
            } else if (vn_contiene(cx, cy)) {
                vn_clic(cx, cy);
            } else {
                clic_escritorio(cx, cy);
            }
            if (accion == ACC_REINICIO) hecho = 1;
            else if (accion == ACC_APAGADO) { apagar(); hecho = 1; }
            else if (accion < ACC_REINICIO) abrir_ventana(accion);
        }
    }
    if (vent >= 0) vn_cerrar();
    for (p = 0; p <= 4; p++) { ui_fundido(desktop0_pal, p, 1); ui_esperar(4); }   // sale a blanco
}

void main(void) {
    DISPLAY_OFF;
    ui_cargar_tiles_ui();      // fuente y piezas de interfaz: banco 1, permanentes
    cursor_iniciar();
    SHOW_BKG;
    DISPLAY_ON;
    for (;;) {
        arrancar();
        escritorio();          // hasta Restart / Shut Down
    }
}
