#!/usr/bin/env python3
"""Genera los gráficos de Ubuntu GB y Windows 11 GB a partir de la imagen de referencia.

Uso:  python tools/convert_sistema.py ubuntu|windows11      (requiere Pillow y numpy)
Entrada: reference/<sistema>.png (160x144, ya del tamaño de la pantalla)
Salida:  assets/generated/<sistema>/gen_graficos.c, .h y preview_*.png

Resumen:
  - Las 6 primeras paletas de Game Boy Color se calculan agrupando (k-means) los colores de los tiles de la imagen.
  - La paleta 6 la usan ventanas y menús (tema del sistema) y la 7 la selección.
  - Reutiliza de convert_assets.py la fuente, las piezas de ventana/menú y el cursor.
"""
import os
import sys

import numpy as np
from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import convert_assets as ca

RAIZ = ca.RAIZ
PASOS = ca.PASOS_CARGA
NPAL_IMAGEN = 6          # paletas 0..5 para el escritorio y el arranque; 6 = ventanas/menús, 7 = selección

# Tema de cada sistema: arranque + paletas de interfaz (índice 0 = tinta, 3 = fondo)
TEMAS = {
    "ubuntu": dict(
        nombre="UBUNTUGB", titulo="Ubuntu", fondo_boot=(44, 0, 30),
        ui=[(238, 221, 196), (150, 90, 112), (96, 38, 68), (48, 10, 40)],     # ventanas y menús: crema sobre berenjena
        sel=[(255, 255, 255), (233, 84, 32), (233, 84, 32), (233, 84, 32)],   # selección: blanco sobre naranja
        barra=((96, 38, 68), (233, 84, 32)), texto=(238, 221, 196)),
    "windows11": dict(
        nombre="WIN11GB", titulo="Windows 11", fondo_boot=(0, 0, 0),
        ui=[(32, 32, 32), (150, 150, 150), (243, 243, 243), (255, 255, 255)], # ventanas y menús claros
        sel=[(255, 255, 255), (0, 103, 192), (0, 103, 192), (0, 103, 192)],   # selección: blanco sobre azul
        barra=((70, 70, 70), (0, 120, 215)), texto=(255, 255, 255)),
}


# ---------------------------------------------------------------- paletas por clustering
def kmeans(datos, k, pesos=None, iteraciones=12, semilla=1):
    """k-means sencillo (datos: N x D). Devuelve (centros, etiquetas)."""
    rng = np.random.default_rng(semilla)
    datos = np.asarray(datos, float)
    k = min(k, len(datos))
    centros = datos[rng.choice(len(datos), k, replace=False)]
    for _ in range(iteraciones):
        d = ((datos[:, None, :] - centros[None, :, :]) ** 2).sum(-1)
        et = d.argmin(1)
        for j in range(k):
            m = et == j
            if m.any():
                w = None if pesos is None else pesos[m]
                centros[j] = np.average(datos[m], axis=0, weights=w)
    d = ((datos[:, None, :] - centros[None, :, :]) ** 2).sum(-1)
    return centros, d.argmin(1)


def luma(c):
    return 0.299 * c[0] + 0.587 * c[1] + 0.114 * c[2]


def paleta4(pixeles):
    """4 colores representativos de un conjunto de píxeles, ordenados de oscuro a claro."""
    px = np.asarray(pixeles, float)
    unicos, cuenta = np.unique(px.astype(int), axis=0, return_counts=True)
    if len(unicos) <= 4:
        cols = [tuple(int(v) for v in u) for u in unicos]
    else:
        centros, _ = kmeans(unicos, 4, pesos=cuenta.astype(float))
        cols = [tuple(int(round(v)) for v in c) for c in centros]
    while len(cols) < 4:
        cols.append(cols[-1])
    return sorted(cols, key=luma)


def bloques(img):
    """Lista de bloques 8x8 (64 píxeles RGB cada uno) en orden de lectura."""
    return [[img.getpixel((tx * 8 + x, ty * 8 + y)) for y in range(8) for x in range(8)]
            for ty in range(18) for tx in range(20)]


def error_paleta(bloque, pal):
    p = np.asarray(bloque, float)
    c = np.asarray(pal, float)
    return ((p[:, None, :] - c[None, :, :]) ** 2).sum(-1).min(1).sum()


def calcular_paletas(imagenes, k=NPAL_IMAGEN):
    """Agrupa todos los bloques de las imágenes en k paletas de 4 colores."""
    todos = [b for im in imagenes for b in bloques(im)]
    feat = np.array([sum(paleta4(b), ()) for b in todos], float)
    _, et = kmeans(feat, k)
    for _ in range(4):                                   # refinado tipo Lloyd
        pals = []
        for j in range(k):
            px = [p for b, e in zip(todos, et) if e == j for p in b]
            pals.append(paleta4(px) if px else paleta4(todos[j]))
        et = np.array([min(range(k), key=lambda j: error_paleta(b, pals[j])) for b in todos])
    return pals


# ---------------------------------------------------------------- conversión a tiles
def convertir(img, pals, ts):
    """img -> (mapa, atributos) usando el conjunto de tiles ts (sin repetir)."""
    mapa, attr = [], []
    for b in bloques(img):
        j = min(range(len(pals)), key=lambda q: error_paleta(b, pals[q]))
        idx = [ca.mas_cercano(px, pals[j])[0] for px in b]
        n, fx, fy = ts.añadir(idx)
        mapa.append(n)
        attr.append((j, fx, fy))
    return mapa, attr


def reducir_a_256(ts, mapas, pals):
    """Si hay más de 256 tiles distintos, sustituye los menos usados por el más parecido ya existente."""
    if len(ts.tiles) <= 256:
        return
    print("aviso: %d tiles, se reducen a 256" % len(ts.tiles))
    uso = {}
    for mapa, attr in mapas:
        for n, (j, fx, fy) in zip(mapa, attr):
            uso[(n, j)] = uso.get((n, j), 0) + 1

    def pix(n, j):
        return np.array([pals[j][i] for i in ts.tiles[n]], float)

    while len({n for n, _ in uso}) > 256:
        conteo = {}
        for (n, j), c in uso.items():
            conteo[n] = conteo.get(n, 0) + c
        victima = min(conteo, key=conteo.get)
        mejor = None
        for (m, j), c in uso.items():
            if m == victima:
                continue
            e = sum(((pix(victima, jj) - pix(m, j)) ** 2).sum() * cc for (nn, jj), cc in uso.items() if nn == victima)
            if mejor is None or e < mejor[0]:
                mejor = (e, m, j)
        _, m, j = mejor
        for mapa, attr in mapas:
            for i, n in enumerate(mapa):
                if n == victima:
                    mapa[i] = m
                    attr[i] = (j, 0, 0)
        uso = {}
        for mapa, attr in mapas:
            for n, (jj, fx, fy) in zip(mapa, attr):
                uso[(n, jj)] = uso.get((n, jj), 0) + 1
    # renumerar compactando
    usados = sorted({n for mapa, _ in mapas for n in mapa})
    nuevo = {n: i for i, n in enumerate(usados)}
    ts.tiles = [ts.tiles[n] for n in usados]
    for mapa, _ in mapas:
        for i, n in enumerate(mapa):
            mapa[i] = nuevo[n]


def a_bytes(mapa, attr):
    m = [n & 255 for n in mapa]
    a = [j | (fx << 5) | (fy << 6) for (j, fx, fy) in attr]
    return m, a


def preview(ts, mapa, attr, pals, ruta):
    img = Image.new("RGB", (160, 144))
    for i, (n, (j, fx, fy)) in enumerate(zip(mapa, attr)):
        t = ts.tiles[n]
        for y in range(8):
            for x in range(8):
                sx = 7 - x if fx else x
                sy = 7 - y if fy else y
                img.putpixel(((i % 20) * 8 + x, (i // 20) * 8 + y), pals[j][t[sy * 8 + sx]])
    img.resize((640, 576), Image.NEAREST).save(ruta)


# ---------------------------------------------------------------- arranque
def texto(img, s, cx, y, color):
    """Rotula s centrado en cx con la fuente 8x8 (proporcional, como los iconos del macOS 9)."""
    cols = ca.texto_proporcional(s)
    x0 = cx - len(cols) // 2
    for i, c in enumerate(cols):
        for r in range(8):
            if c[r]:
                img.putpixel((x0 + i, y + r), color)


def logo_ubuntu(img):
    """Recorta el icono de Ubuntu del dock de la referencia y lo amplía x2."""
    ref = Image.open(os.path.join(RAIZ, "reference", "ubuntu.png")).convert("RGB")
    ic = ref.crop((4, 15, 22, 33)).resize((36, 36), Image.NEAREST)
    for y in range(36):
        for x in range(36):
            c = ic.getpixel((x, y))
            if luma(c) < 90:                      # el fondo oscuro del dock pasa a ser el del arranque
                ic.putpixel((x, y), img.getpixel((0, 0)))
    img.paste(ic, (62, 36))


def logo_windows(img):
    az = (0, 120, 215)
    for dx in (0, 1):
        for dy in (0, 1):
            x, y = 71 + dx * 10, 38 + dy * 10
            for i in range(9):
                for j in range(9):
                    img.putpixel((x + i, y + j), az)


def frame_arranque(sistema, tema, k):
    img = Image.new("RGB", (160, 144), tema["fondo_boot"])
    (logo_ubuntu if sistema == "ubuntu" else logo_windows)(img)
    texto(img, tema["titulo"], 80, 84, tema["texto"])
    vacio, lleno = tema["barra"]
    x0, y0, ancho = 40, 104, 80
    for x in range(ancho):
        for y in range(4):
            img.putpixel((x0 + x, y0 + y), lleno if x < round(ancho * k / PASOS) else vacio)
    return img


def arranque(sistema, tema):
    frames = [frame_arranque(sistema, tema, k) for k in range(PASOS + 1)]
    pals = calcular_paletas([frames[0], frames[-1]], 8)
    ts = ca.ConjuntoTiles()
    conv = [convertir(f, pals, ts) for f in frames]
    base = conv[0]
    celdas = sorted({i for m, a in conv for i in range(360) if (m[i], a[i]) != (base[0][i], base[1][i])})
    x0 = min(i % 20 for i in celdas); x1 = max(i % 20 for i in celdas)
    y0 = min(i // 20 for i in celdas); y1 = max(i // 20 for i in celdas)
    barra_m, barra_a = [], []
    for m, a in conv:
        mm, aa = a_bytes(m, a)
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                barra_m.append(mm[y * 20 + x]); barra_a.append(aa[y * 20 + x])
    m0, a0 = a_bytes(*base)
    return dict(pals=pals, ts=ts, m0=m0, a0=a0, barra_m=barra_m, barra_a=barra_a,
                barra=(x0, y0, x1 - x0 + 1, y1 - y0 + 1), conv=conv)


# ---------------------------------------------------------------- salida
def main():
    if len(sys.argv) != 2 or sys.argv[1] not in TEMAS:
        sys.exit("uso: convert_sistema.py ubuntu|windows11")
    sistema = sys.argv[1]
    tema = TEMAS[sistema]
    gen = os.path.join(RAIZ, "assets", "generated", sistema)
    os.makedirs(gen, exist_ok=True)

    ref = Image.open(os.path.join(RAIZ, "reference", sistema + ".png")).convert("RGB")
    assert ref.size == (160, 144), ref.size
    pals_esc = calcular_paletas([ref])
    ts = ca.ConjuntoTiles()
    mapa, attr = convertir(ref, pals_esc, ts)
    reducir_a_256(ts, [(mapa, attr)], pals_esc)
    preview(ts, mapa, attr, pals_esc, os.path.join(gen, "preview_escritorio.png"))
    pal_escritorio = pals_esc + [tema["ui"], tema["sel"]]
    m, a = a_bytes(mapa, attr)

    arr = arranque(sistema, tema)
    preview(arr["ts"], arr["conv"][0][0], arr["conv"][0][1], arr["pals"], os.path.join(gen, "preview_bienvenida.png"))
    preview(arr["ts"], arr["conv"][-1][0], arr["conv"][-1][1], arr["pals"], os.path.join(gen, "preview_carga.png"))
    assert len(arr["ts"].tiles) <= 256, len(arr["ts"].tiles)
    ui = ca.piezas_ui()

    with open(os.path.join(gen, "gen_graficos.c"), "w", encoding="utf-8") as f:
        f.write("// GENERADO por tools/convert_sistema.py: no editar a mano.\n#include \"gen_graficos.h\"\n\n")
        f.write(ca.bloque_c("desktop0_tiles", ca.tiles_bytes(ts.tiles)))
        f.write(ca.bloque_c("desktop0_map", m))
        f.write(ca.bloque_c("desktop0_attr", a))
        f.write(ca.bloque_c("desktop0_pal", ca.paletas_flat(pal_escritorio), "const unsigned int", 8, False))
        f.write(ca.bloque_c("boot_pal", ca.paletas_flat(arr["pals"]), "const unsigned int", 8, False))
        f.write(ca.bloque_c("boot_tiles", ca.tiles_bytes(arr["ts"].tiles)))
        f.write(ca.bloque_c("boot_map_carga", arr["m0"]))
        f.write(ca.bloque_c("boot_attr_carga", arr["a0"]))
        f.write(ca.bloque_c("boot_barra_map", arr["barra_m"]))
        f.write(ca.bloque_c("boot_barra_attr", arr["barra_a"]))
        f.write(ca.bloque_c("ui_tiles", ca.tiles_bytes([t for _, t in ui])))
        cv = lambda filas: ca.tile_ascii([r.replace(".", "0").replace("X", "1").replace("W", "2") for r in filas])
        filas = ca.FLECHA + ["........"] * (16 - len(ca.FLECHA))
        f.write(ca.bloque_c("cursor_tiles", ca.tiles_bytes([cv(filas[:8]), cv(filas[8:])])))
        f.write(ca.bloque_c("cursor_pal", [0, ca.rgb555((0, 0, 0)), ca.rgb555((255, 255, 255)), ca.rgb555((255, 255, 255))],
                            "const unsigned int", 4, False))
    with open(os.path.join(gen, "gen_graficos.h"), "w", encoding="utf-8") as f:
        f.write("// GENERADO por tools/convert_sistema.py: no editar a mano.\n#ifndef GEN_GRAFICOS_H\n#define GEN_GRAFICOS_H\n\n")
        f.write("extern const unsigned char desktop0_tiles[], desktop0_map[], desktop0_attr[];\n")
        f.write("extern const unsigned int desktop0_pal[];\n#define DESKTOP0_TILE_COUNT %d\n" % len(ts.tiles))
        f.write("extern const unsigned int boot_pal[];\n")
        f.write("extern const unsigned char boot_tiles[], boot_map_carga[], boot_attr_carga[], boot_barra_map[], boot_barra_attr[];\n")
        f.write("#define BOOT_TILE_COUNT %d\n#define BOOT_PASOS %d\n" % (len(arr["ts"].tiles), PASOS))
        f.write("#define BOOT_BARRA_X %d\n#define BOOT_BARRA_Y %d\n#define BOOT_BARRA_W %d\n#define BOOT_BARRA_H %d\n" % arr["barra"])
        f.write("extern const unsigned char ui_tiles[], cursor_tiles[];\nextern const unsigned int cursor_pal[];\n\n")
        f.write("#define UI_TILE_COUNT %d\n#define UI_FONT 0\n#define UI_TITLE_FONT 96\n" % len(ui))
        for i, (nombre, _) in enumerate(ui):
            if not nombre.startswith(("FONT_", "TITLE_FONT_")):
                f.write("#define UI_%s %d\n" % (nombre, i))
        f.write("\n#endif\n")
    print("%s: escritorio %d tiles | arranque %d tiles | ui %d tiles" % (sistema, len(ts.tiles), len(arr["ts"].tiles), len(ui)))


if __name__ == "__main__":
    main()
