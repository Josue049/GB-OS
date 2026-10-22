# Script de GIMP (Python-fu): exporta cada capa del XCF a assets/source/layers/NN.png
# Ejecutar desde la raíz del proyecto:
#   gimp-console-2.10 -i -b "(python-fu-eval 1 \"execfile('tools/export_layers.py')\")" -b "(gimp-quit 0)"
import os
from gimpfu import *

base = os.getcwd()
img = pdb.gimp_file_load(os.path.join(base, "assets", "source", "MacOS9.xcf"), "x")
out = os.path.join(base, "assets", "source", "layers")
if not os.path.isdir(out):
    os.makedirs(out)
for i, capa in enumerate(img.layers):
    pdb.file_png_save_defaults(img, capa, os.path.join(out, "%02d.png" % i), "x")
