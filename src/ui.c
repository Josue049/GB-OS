// Utilidades de pantalla: tiles con atributos CGB, texto, paletas y fundidos
#include "os.h"

// Copia en RAM de lo que hay en pantalla (tile y atributo) para poder restaurar paletas
static UBYTE sombra_attr[360];

static void escribir_attr(UBYTE x, UBYTE y, UBYTE a) {
    VBK_REG = VBK_BANK_1;
    set_bkg_tile_xy(x, y, a);
    VBK_REG = VBK_BANK_0;
}

void ui_cargar_tiles_ui(void) {
    VBK_REG = VBK_BANK_1;
    set_bkg_data(0, UI_TILE_COUNT, ui_tiles);
    VBK_REG = VBK_BANK_0;
}

void ui_cargar_pantalla(const UBYTE *mapa, const UBYTE *attr) {
    memcpy(sombra_attr, attr, 360);
    set_bkg_tiles(0, 0, 20, 18, mapa);
    VBK_REG = VBK_BANK_1;
    set_bkg_tiles(0, 0, 20, 18, attr);
    VBK_REG = VBK_BANK_0;
}

void ui_tile(UBYTE x, UBYTE y, UBYTE tile, UBYTE attr) {
    sombra_attr[y * 20 + x] = attr;
    set_bkg_tile_xy(x, y, tile);
    escribir_attr(x, y, attr);
}

// Escribe texto con la fuente del banco 1 (base = UI_FONT o UI_TITLE_FONT); rellena con espacios hasta 'ancho'
void ui_texto(UBYTE x, UBYTE y, const char *s, UBYTE base, UBYTE attr, UBYTE ancho) {
    UBYTE i;
    for (i = 0; i < ancho; i++) {
        char c = *s ? *s++ : ' ';
        ui_tile(x + i, y, base + (c - 32), ATTR_BANCO1 | attr);
    }
}

// Escritorio activo (mapa y atributos en ROM) para poder repintar zonas tapadas por ventanas/menús
static const UBYTE *fondo_map, *fondo_attr;
void ui_elegir_fondo(const UBYTE *mapa, const UBYTE *attr) { fondo_map = mapa; fondo_attr = attr; }

// Texto con la fuente de títulos (solo letras y espacio; lleva la raya inferior de la barra de título)
void ui_titulo(UBYTE x, UBYTE y, const char *s, UBYTE attr, UBYTE ancho) {
    UBYTE i;
    for (i = 0; i < ancho; i++) {
        char c = *s ? *s++ : ' ';
        ui_tile(x + i, y, UI_TITLE_FONT + (c == ' ' ? 0 : c - 'A' + 1), ATTR_BANCO1 | attr);
    }
}

// Repinta una zona con el diseño original del escritorio
void ui_restaurar_escritorio(UBYTE x, UBYTE y, UBYTE w, UBYTE h) {
    UBYTE i, j;
    unsigned int k;
    for (j = y; j < y + h; j++)
        for (i = x; i < x + w; i++) {
            k = j * 20 + i;
            ui_tile(i, j, fondo_map[k], fondo_attr[k]);
        }
}

// Cambia solo la paleta de una zona (selección/resaltado), sin tocar la copia en RAM
void ui_paleta_rect(UBYTE x, UBYTE y, UBYTE w, UBYTE h, UBYTE pal) {
    UBYTE i, j;
    for (j = y; j < y + h; j++)
        for (i = x; i < x + w; i++)
            escribir_attr(i, j, (sombra_attr[j * 20 + i] & ~7) | pal);
}

void ui_paleta_restaurar(UBYTE x, UBYTE y, UBYTE w, UBYTE h) {
    UBYTE i, j;
    for (j = y; j < y + h; j++)
        for (i = x; i < x + w; i++)
            escribir_attr(i, j, sombra_attr[j * 20 + i]);
}

// Mezcla las 8 paletas hacia blanco o negro. paso: 0 = original ... 4 = color final
void ui_fundido(const unsigned int *pal, UBYTE paso, UBYTE a_blanco) {
    static unsigned int tmp[32];
    UBYTE i;
    int d = a_blanco ? 31 : 0;
    for (i = 0; i < 32; i++) {
        int r = pal[i] & 31, g = (pal[i] >> 5) & 31, b = (pal[i] >> 10) & 31;
        r += (d - r) * paso / 4;
        g += (d - g) * paso / 4;
        b += (d - b) * paso / 4;
        tmp[i] = r | (g << 5) | (b << 10);
    }
    set_bkg_palette(0, 8, tmp);
}

void ui_esperar(UBYTE frames) {
    while (frames--) vsync();
}
