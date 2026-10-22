#!/usr/bin/env python3
"""Convierte las capas exportadas del XCF a tiles/mapas/paletas de Game Boy Color.

Uso:  python tools/convert_assets.py        (requiere Pillow)
Entrada: assets/source/layers/*.png (export_layers.py) y MacOS9_GBC_fondos.gpl
Salida:  assets/generated/*.c, *.h y preview_*.png

Resumen:
  - Compone el escritorio (capa 01 + barra/launcher de la capa 00) y la pantalla de inicio (capa 02).
  - Divide en tiles de 8x8, elige para cada uno la paleta de 4 colores que menos error da
    y elimina tiles repetidos (también reflejados en X/Y).
  - Genera además la fuente y las piezas de interfaz (ventanas, menús) en el banco 1 de VRAM.
"""
import os
from PIL import Image

RAIZ = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
SRC = os.path.join(RAIZ, "assets", "source")
GEN = os.path.join(RAIZ, "assets", "generated")
FONT_H = os.path.join(os.path.dirname(os.path.abspath(__file__)), "font8x8_basic.h")
os.makedirs(GEN, exist_ok=True)

# ---------------------------------------------------------------- paletas
def leer_gpl(ruta):
    cols = []
    for ln in open(ruta, encoding="utf-8"):
        p = ln.split()
        if len(p) >= 3 and p[0].isdigit():
            cols.append((int(p[0]), int(p[1]), int(p[2])))
    return [cols[i:i + 4] for i in range(0, len(cols), 4)]

GPL = leer_gpl(os.path.join(SRC, "MacOS9_GBC_fondos.gpl"))

def paletas_escritorio(n):
    """Las 8 paletas del escritorio n (0 = patrón morado, 1 = Blue Finder).
    0,1,2,5,7 son las del GPL tal cual; 3, 4 y 6 son ajustes mínimos."""
    base = GPL[0] if n == 0 else GPL[4]
    fondo = GPL[0][1] if n == 0 else GPL[4][2]       # color plano de fondo (para iconos y bordes)
    return [
        base,                                          # 0 fondo del escritorio
        GPL[1],                                        # 1 menús
        GPL[2],                                        # 2 ventanas
        [GPL[3][0], fondo, GPL[3][2], GPL[3][3]],      # 3 iconos (el fondo de icono = fondo del escritorio)
        [(0, 0, 0), (189, 189, 189), (222, 222, 222), fondo],   # 4 bordes barra/launcher sobre el fondo
        GPL[5],                                        # 5 launcher
        [(255, 255, 255), GPL[6][2], GPL[6][1], GPL[6][1]],     # 6 selección (texto blanco sobre azul)
        GPL[4] if n == 0 else GPL[0],                  # 7 el OTRO escritorio (para la miniatura del panel Desktop)
    ]
# Arranque (bienvenida + carga): colores de las capturas del diseño, solo los grises/azules de la caja
PAL_INICIO = [
    GPL[0],                                                            # 0 fondo morado del escritorio
    [(0, 0, 0), (146, 146, 146), (221, 221, 221), (255, 255, 255)],    # 1 caja: texto y relleno gris
    [(0, 0, 0), (0, 0, 136), (68, 99, 195), (123, 153, 214)],          # 2 logo sin blanco: negro, azul marino y dos azules
    [GPL[0][1], (82, 82, 82), (221, 221, 221), (255, 255, 255)],       # 3 caja sobre el fondo (bordes)
    [(0, 0, 0), (68, 99, 195), (123, 153, 214), (255, 255, 255)],      # 4 logo azul de Finder
    [(0, 0, 0), (146, 146, 146), (122, 151, 212), (221, 221, 221)],    # 5 barra de progreso
    [(0, 0, 136), (68, 99, 195), (123, 153, 214), (255, 255, 255)],    # 6 logo con los rasgos azul marino
    [(82, 82, 82), (146, 146, 146), (122, 151, 212), (221, 221, 221)], # 7 barra con la sombra de la caja
]
CANDIDATAS = [0, 1, 2, 3, 4, 5]   # la 6 se reserva a la selección en tiempo de ejecución

def c5(v):
    return (v * 31 + 127) // 255      # 8 bits -> 5 bits redondeando (no truncando)

def rgb555(c):
    """Color RGB888 -> RGB15 de Game Boy Color (bits 0-4 R, 5-9 G, 10-14 B)."""
    return (c5(c[2]) << 10) | (c5(c[1]) << 5) | c5(c[0])

def mas_cercano(px, pal):
    mejor, err = 0, 1 << 30
    for i, c in enumerate(pal):
        e = (px[0] - c[0]) ** 2 + (px[1] - c[1]) ** 2 + (px[2] - c[2]) ** 2
        if e < err:
            mejor, err = i, e
    return mejor, err

# ---------------------------------------------------------------- fuente (font8x8, dominio público)
def cargar_fuente():
    import re
    txt = open(FONT_H).read()
    filas = re.findall(r"\{\s*((?:0x[0-9A-Fa-f]{2},?\s*){8})\}", txt)
    glifos = [[int(b, 16) for b in re.findall(r"0x[0-9A-Fa-f]{2}", f)] for f in filas]
    return glifos[32:128]       # ASCII 32..127

FUENTE = cargar_fuente()

def glifo_px(g):
    """8 filas de 8 pixeles (1 = tinta). En font8x8 el bit 0 es el pixel izquierdo."""
    return [[(fila >> x) & 1 for x in range(8)] for fila in g]

def texto_proporcional(txt):
    """Devuelve una lista de columnas (cada una 8 bits de alto) para rotular el icono."""
    cols = []
    for ch in txt:
        if ch == " ":
            cols += [[0] * 8] * 3
            continue
        px = glifo_px(FUENTE[ord(ch) - 32])
        xs = [x for x in range(8) if any(px[y][x] for y in range(8))]
        for x in range(min(xs), max(xs) + 1):
            cols.append([px[y][x] for y in range(8)])
        cols.append([0] * 8)
    return cols[:-1]

# ---------------------------------------------------------------- pixel art propio (iconos)
def fila_hd(inner):
    return "." + "#" + inner + "#" + "."

ICONO_HD = [
    "................", "................", "................",
    "..############..",
    fila_hd("wwwwwwwwwwww"), fila_hd("wooooooooooo"), fila_hd("wooooooooooo"),
    fila_hd("wooooooooooo"), fila_hd("############"), fila_hd("wooooooooooo"),
    fila_hd("wo#########o"), fila_hd("wooooooooooo"), fila_hd("wooooooooo#o"),
    "..############..", "................", "................",
]
assert all(len(f) == 16 for f in ICONO_HD)

# carpeta pequeña 8x8 (índices de la paleta 2: 0 negro, 1 gris, 2 gris claro, 3 blanco)
CARPETA = ["33333333", "30003333", "01222003", "02222220", "02222220", "02222220", "01111110", "33333333"]

# Cursor: flecha clásica de Mac OS 9 reducida a 8x12 (base plana y palito en diagonal, como el original) (X = borde negro, W = relleno blanco, . = transparente).
FLECHA = [
    "W.......",
    "WW......",
    "WXW.....",
    "WXXW....",
    "WXXXW...",
    "WXXXXW..",
    "WXXXXXW.",
    "WXXXXWWW",
    "WXWXW...",
    "WWWXW...",
    "W..WXW..",
    "....WW..",
]

# ---------------------------------------------------------------- tiles
def a_2bpp(idx):
    """64 índices (0..3) -> 16 bytes en formato planar de Game Boy."""
    out = []
    for y in range(8):
        lo = hi = 0
        for x in range(8):
            v = idx[y * 8 + x]
            lo |= (v & 1) << (7 - x)
            hi |= (v >> 1) << (7 - x)
        out += [lo, hi]
    return out

def variantes(idx):
    """El tile original y sus reflejos: (indices, flipX, flipY)."""
    t = [idx[y * 8:(y + 1) * 8] for y in range(8)]
    fx = [r[::-1] for r in t]
    return [(idx, 0, 0), (sum(fx, []), 1, 0), (sum(t[::-1], []), 0, 1), (sum(fx[::-1], []), 1, 1)]

class ConjuntoTiles:
    """Guarda tiles únicos; devuelve (indice, flips) reutilizando los ya existentes."""
    def __init__(self):
        self.tiles, self.tabla = [], {}
    def añadir(self, idx):
        idx = tuple(idx)
        if idx in self.tabla:
            return self.tabla[idx]
        n = len(self.tiles)
        self.tiles.append(idx)
        for v, fx, fy in variantes(list(idx)):
            self.tabla.setdefault(tuple(v), (n, fx, fy))
        return n, 0, 0

def convertir_pantalla(img, paletas, forzadas=None, ts=None):
    """img 160x144 RGB -> (tiles, mapa[360], atributos[360]).
    Los tiles con índice >= 256 van al banco 1 de VRAM (bit 3 del atributo)."""
    forzadas = forzadas or {}
    ts = ts or ConjuntoTiles()
    mapa, attr = [], []
    for ty in range(18):
        for tx in range(20):
            bloque = [img.getpixel((tx * 8 + x, ty * 8 + y)) for y in range(8) for x in range(8)]
            cand = [forzadas[(tx, ty)]] if (tx, ty) in forzadas else CANDIDATAS_ACTUAL
            mejor = None
            for p in cand:
                r = [mas_cercano(px, paletas[p]) for px in bloque]
                e = sum(a[1] for a in r)
                if mejor is None or e < mejor[0]:
                    mejor = (e, p, [a[0] for a in r])
            _, p, idx = mejor
            n, fx, fy = ts.añadir(idx)
            mapa.append(n & 255)
            attr.append(p | (fx << 5) | (fy << 6) | (8 if n >= 256 else 0))
    return ts.tiles, mapa, attr

# ---------------------------------------------------------------- composición del escritorio
def componer_escritorio(n, paletas):
    capa = "01.png" if n == 0 else "02.png"        # 01 = patrón morado, 02 = Blue Finder
    base = Image.open(os.path.join(SRC, "layers", capa)).convert("RGB").crop((0, 0, 160, 144))
    sup = Image.open(os.path.join(SRC, "layers", "00.png")).convert("RGBA")
    base = base.convert("RGBA")
    base.alpha_composite(sup)
    img = base.convert("RGB")
    forz = {}
    # Icono Macintosh HD (no está en el diseño: se dibuja con el estilo de los iconos del Launcher)
    ix, iy = 13, 3                       # posición en tiles
    pal3 = paletas[3]
    col = {".": pal3[1], "#": pal3[0], "o": pal3[2], "w": pal3[3]}
    for y, fila in enumerate(ICONO_HD):
        for x, c in enumerate(fila):
            img.putpixel((ix * 8 + x, iy * 8 + y), col[c])
    for ty in range(iy, iy + 2):
        for tx in range(ix, ix + 2):
            forz[(tx, ty)] = 3
    # Rótulo "Macintosh HD" proporcional sobre caja clara (paleta 2), centrado bajo el icono
    cols = texto_proporcional("Macintosh HD")
    ancho = len(cols)
    cx = ix * 8 + 8
    x0 = cx - ancho // 2
    t0, t1 = x0 // 8, (x0 + ancho - 1) // 8
    ly = (iy + 2) * 8
    p2 = paletas[2]
    for tx in range(t0, t1 + 1):
        forz[(tx, iy + 2)] = 2
        for y in range(8):
            for x in range(8):
                img.putpixel((tx * 8 + x, ly + y), p2[3])
    for i, c in enumerate(cols):
        for y in range(8):
            if c[y]:
                img.putpixel((x0 + i, ly + y), p2[0])
    info = dict(icon=(ix, iy), label=(t0, t1, iy + 2))
    return img, forz, info

# ---------------------------------------------------------------- piezas de interfaz (banco 1)
def tile_texto(g, borde_inf=False, tinta=0, fondo=3):
    px = glifo_px(g)
    idx = [tinta if px[y][x] else fondo for y in range(8) for x in range(8)]
    if borde_inf:
        for x in range(8):
            idx[7 * 8 + x] = 0
    return idx

def tile_ascii(filas):
    assert len(filas) == 8 and all(len(f) == 8 for f in filas), filas
    return [int(c) for f in filas for c in f]

def piezas_ui():
    """Lista ordenada (nombre, 64 índices). La fuente ocupa los primeros 96 tiles."""
    P = []
    for i in range(96):
        P.append(("FONT_%d" % i, tile_texto(FUENTE[i])))
    for c in " " + "".join(chr(k) for k in range(ord("A"), ord("z") + 1)):   # solo letras: títulos
        P.append(("TITLE_FONT_%d" % ord(c), tile_texto(FUENTE[ord(c) - 32], borde_inf=True)))
    franja = ["33333333", "22222222"] * 3 + ["33333333", "00000000"]
    franja = ["33333333", "22222222", "33333333", "22222222", "33333333", "22222222", "33333333", "00000000"]
    # Ventana (paleta 2)
    P.append(("WIN_TITLE", tile_ascii(franja)))
    P.append(("WIN_TITLE_L", tile_ascii(["0" + f[1:] for f in franja[:7]] + ["00000000"])))
    P.append(("WIN_TITLE_R", tile_ascii([f[:7] + "0" for f in franja[:7]] + ["00000000"])))
    P.append(("WIN_CLOSE", tile_ascii(["03333333", "03333333", "03000003", "03033303", "03033303",
                                       "03033303", "03000003", "00000000"])))
    P.append(("WIN_BODY", tile_ascii(["33333333"] * 8)))
    P.append(("WIN_L", tile_ascii(["03333333"] * 8)))
    P.append(("WIN_R", tile_ascii(["33333330"] * 8)))
    P.append(("WIN_BOT", tile_ascii(["33333333"] * 7 + ["00000000"])))
    P.append(("WIN_BOT_L", tile_ascii(["03333333"] * 7 + ["00000000"])))
    P.append(("WIN_BOT_R", tile_ascii(["33333330"] * 7 + ["00000000"])))
    P.append(("FOLDER", tile_ascii(CARPETA)))
    # Menú desplegable (paleta 1): 3 columnas (izq, centro, der) x 3 filas (arriba, medio, abajo)
    for fila, nombre in (("top", ["22222222", "00000000"] + ["3" * 8] * 6),
                         ("mid", ["3" * 8] * 8),
                         ("bot", ["3" * 8] * 7 + ["00000000"])):
        for lado, f in (("L", lambda r: "0" + r[1:]), ("M", lambda r: r), ("R", lambda r: r[:7] + "0")):
            filas = [f(r) if r != "00000000" else r for r in nombre]
            if lado != "M" and fila == "top":
                filas[0] = "22222222"
            P.append(("MENU_%s_%s" % (fila.upper(), lado), tile_ascii(filas)))
    # Miniaturas de los dos escritorios para el panel Desktop (5x4 tiles = 40x32 px)
    for n in range(2):
        for k, t in enumerate(miniatura(n)):
            P.append(("THUMB%d_%d" % (n, k), t))
    return P

def miniatura(n):
    """Reduce la imagen del escritorio n a 40x32 con los colores de su paleta 0; devuelve 20 tiles."""
    capa = "01.png" if n == 0 else "02.png"
    pal = GPL[0] if n == 0 else GPL[4]
    im = Image.open(os.path.join(SRC, "layers", capa)).convert("RGB").crop((0, 0, 160, 144))
    im = im.resize((40, 32), Image.LANCZOS)
    tiles = []
    for ty in range(4):
        for tx in range(5):
            tiles.append([mas_cercano(im.getpixel((tx * 8 + x, ty * 8 + y)), pal)[0]
                          for y in range(8) for x in range(8)])
    return tiles

# ---------------------------------------------------------------- salida C
def bloque_c(nombre, datos, tipo="const unsigned char", ancho=16, hexa=True):
    s = "%s %s[%d] = {\n" % (tipo, nombre, len(datos))
    for i in range(0, len(datos), ancho):
        trozo = datos[i:i + ancho]
        s += "    " + ", ".join(("0x%02X" % v if hexa else "0x%04X" % v) for v in trozo) + ",\n"
    return s + "};\n\n"

def tiles_bytes(tiles):
    out = []
    for t in tiles:
        out += a_2bpp(t)
    return out

def paletas_flat(pals):
    return [rgb555(c) for p in pals for c in p]

def preview(tiles, mapa, attr, pals, ruta):
    img = Image.new("RGB", (160, 144))
    for i, (n, a) in enumerate(zip(mapa, attr)):
        t = tiles[n + (256 if a & 8 else 0)]
        pal = pals[a & 7]
        for y in range(8):
            for x in range(8):
                sx = 7 - x if a & 0x20 else x
                sy = 7 - y if a & 0x40 else y
                img.putpixel(((i % 20) * 8 + x, (i // 20) * 8 + y), pal[t[sy * 8 + sx]])
    img.resize((640, 576), Image.NEAREST).save(ruta)

# ---------------------------------------------------------------- pantallas de arranque
PASOS_CARGA = 15      # pasos de la barra de progreso

def pantallas_arranque():
    """Bienvenida y carga (con barra animada). Devuelve tiles y los mapas necesarios."""
    bienv = Image.open(os.path.join(SRC, "boot_bienvenida.png")).convert("RGB")
    caja = Image.open(os.path.join(SRC, "boot_cargando.png")).convert("RGB")
    cx, cy = [int(v) for v in open(os.path.join(SRC, "boot_caja.txt")).read().split()]
    base = bienv.copy()
    base.paste(caja, (cx, cy))
    # Geometría de la barra dentro de la caja: borde izquierdo negro en x=30, interior y=84..87
    xl = 30
    xr = max(x for x in range(60, 110) if caja.getpixel((x, 86)) == (0, 0, 0))
    gris, azul = (221, 221, 221), (122, 151, 212)

    def frame(k):
        im = base.copy()
        relleno = round((xr - xl - 1) * k / PASOS_CARGA)
        for x in range(xl + 1, xr):
            for y in range(84, 88):
                im.putpixel((cx + x, cy + y), azul if x - xl - 1 < relleno else gris)
        return im

    global CANDIDATAS_ACTUAL
    CANDIDATAS_ACTUAL = [0, 1, 2, 3, 4, 5, 6, 7]
    ts = ConjuntoTiles()
    _, m_b, a_b = convertir_pantalla(bienv, PAL_INICIO, ts=ts)
    frames = [convertir_pantalla(frame(k), PAL_INICIO, ts=ts)[1:] for k in range(PASOS_CARGA + 1)]
    m0, a0 = frames[0]
    # Zona de la barra: celdas que cambian entre los pasos
    celdas = sorted({i for m, a in frames for i in range(360) if (m[i], a[i]) != (m0[i], a0[i])})
    x0 = min(i % 20 for i in celdas); x1 = max(i % 20 for i in celdas)
    y0 = min(i // 20 for i in celdas); y1 = max(i // 20 for i in celdas)
    barra_m, barra_a = [], []
    for m, a in frames:
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                barra_m.append(m[y * 20 + x]); barra_a.append(a[y * 20 + x])
    preview(ts.tiles, m_b, a_b, PAL_INICIO, os.path.join(GEN, "preview_bienvenida.png"))
    preview(ts.tiles, frames[PASOS_CARGA // 2][0], frames[PASOS_CARGA // 2][1], PAL_INICIO,
            os.path.join(GEN, "preview_carga.png"))
    return dict(tiles=ts.tiles, m_b=m_b, a_b=a_b, m0=m0, a0=a0, barra_m=barra_m, barra_a=barra_a,
                barra=(x0, y0, x1 - x0 + 1, y1 - y0 + 1))

def main():
    global CANDIDATAS_ACTUAL
    CANDIDATAS_ACTUAL = CANDIDATAS
    arr = pantallas_arranque()
    escritorios = []
    for n in range(2):
        pals = paletas_escritorio(n)
        img, forz, info = componer_escritorio(n, pals)
        img.save(os.path.join(GEN, "preview_composicion%d.png" % n))
        t, m, a = convertir_pantalla(img, pals, forz)
        preview(t, m, a, pals, os.path.join(GEN, "preview_escritorio%d.png" % n))
        escritorios.append((t, m, a, pals))
    ui = piezas_ui()
    assert len(ui) <= 256

    with open(os.path.join(GEN, "gen_graficos.c"), "w", encoding="utf-8") as f:
        f.write("// GENERADO por tools/convert_assets.py: no editar a mano.\n#include \"gen_graficos.h\"\n\n")
        for n, (t, m, a, pals) in enumerate(escritorios):
            f.write(bloque_c("desktop%d_tiles" % n, tiles_bytes(t)))
            f.write(bloque_c("desktop%d_map" % n, m))
            f.write(bloque_c("desktop%d_attr" % n, a))
            f.write(bloque_c("desktop%d_pal" % n, paletas_flat(pals), "const unsigned int", 8, False))
        f.write(bloque_c("boot_pal", paletas_flat(PAL_INICIO), "const unsigned int", 8, False))
        f.write(bloque_c("boot_tiles", tiles_bytes(arr["tiles"])))
        f.write(bloque_c("boot_map_bienvenida", arr["m_b"]))
        f.write(bloque_c("boot_attr_bienvenida", arr["a_b"]))
        f.write(bloque_c("boot_map_carga", arr["m0"]))
        f.write(bloque_c("boot_attr_carga", arr["a0"]))
        f.write(bloque_c("boot_barra_map", arr["barra_m"]))
        f.write(bloque_c("boot_barra_attr", arr["barra_a"]))
        f.write(bloque_c("ui_tiles", tiles_bytes([t for _, t in ui])))
        cv = lambda filas: tile_ascii([r.replace(".", "0").replace("X", "1").replace("W", "2") for r in filas])
        filas = FLECHA + ["........"] * (16 - len(FLECHA))          # sprite 8x16: 2 tiles (la mitad inferior casi vacía)
        flecha = [cv(filas[:8]), cv(filas[8:])]
        f.write(bloque_c("cursor_tiles", tiles_bytes(flecha)))
        f.write(bloque_c("cursor_pal", [0, rgb555((0, 0, 0)), rgb555((255, 255, 255)), rgb555((255, 255, 255))],
                         "const unsigned int", 4, False))
    with open(os.path.join(GEN, "gen_graficos.h"), "w", encoding="utf-8") as f:
        f.write("// GENERADO por tools/convert_assets.py: no editar a mano.\n#ifndef GEN_GRAFICOS_H\n#define GEN_GRAFICOS_H\n\n")
        for n in range(2):
            f.write("extern const unsigned char desktop%d_tiles[], desktop%d_map[], desktop%d_attr[];\n" % (n, n, n))
            f.write("extern const unsigned int desktop%d_pal[];\n" % n)
            f.write("#define DESKTOP%d_TILE_COUNT %d\n" % (n, len(escritorios[n][0])))
        f.write("extern const unsigned int boot_pal[];\n")
        f.write("extern const unsigned char boot_tiles[], boot_map_bienvenida[], boot_attr_bienvenida[];\n")
        f.write("extern const unsigned char boot_map_carga[], boot_attr_carga[], boot_barra_map[], boot_barra_attr[];\n")
        f.write("#define BOOT_TILE_COUNT %d\n#define BOOT_PASOS %d\n" % (len(arr["tiles"]), PASOS_CARGA))
        f.write("#define BOOT_BARRA_X %d\n#define BOOT_BARRA_Y %d\n#define BOOT_BARRA_W %d\n#define BOOT_BARRA_H %d\n"
                % arr["barra"])
        f.write("extern const unsigned char ui_tiles[], cursor_tiles[];\n")
        f.write("extern const unsigned int cursor_pal[];\n\n")
        f.write("#define UI_TILE_COUNT %d\n\n" % len(ui))
        f.write("// Posición (en tiles) del icono Macintosh HD y de su rótulo (igual en los dos escritorios)\n")
        f.write("#define HD_ICON_X %d\n#define HD_ICON_Y %d\n" % info["icon"])
        f.write("#define HD_LABEL_X0 %d\n#define HD_LABEL_X1 %d\n#define HD_LABEL_Y %d\n\n" % info["label"])
        f.write("// Índices de tile del banco 1 (fuente: UI_FONT + (caracter - 32))\n")
        f.write("#define UI_FONT 0\n#define UI_TITLE_FONT 96\n")
        for i, (nombre, _) in enumerate(ui):
            if not nombre.startswith(("FONT_", "TITLE_FONT_")):
                f.write("#define UI_%s %d\n" % (nombre, i))
        f.write("\n#endif\n")
    print("escritorios: %d y %d tiles | ui: %d tiles" % (len(escritorios[0][0]), len(escritorios[1][0]), len(ui)))

if __name__ == "__main__":
    main()
