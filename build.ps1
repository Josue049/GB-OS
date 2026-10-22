# Compila la ROM: convierte los gráficos (Python + Pillow) y llama a GBDK-2020.
# Requiere GBDK-2020 (https://github.com/gbdk-2020/gbdk-2020): define GBDK_HOME con su carpeta
# o deja `lcc` en el PATH.
$raiz = $PSScriptRoot
if ($env:GBDK_HOME) { $env:PATH = "$env:GBDK_HOME\bin;$env:PATH" }
python "$raiz\tools\convert_assets.py"
New-Item -ItemType Directory -Force "$raiz\build" | Out-Null
# -yC: solo Game Boy Color · -yt0x03 -ya1: MBC1 + RAM con batería (guardado del fondo)
lcc -Wm-yC -Wm-yt0x03 -Wm-ya1 -Wm-yn"MACOS9GB" -I"$raiz\assets\generated" -I"$raiz\src" -o "$raiz\build\MacOS9.gbc" (Get-ChildItem "$raiz\src\*.c").FullName "$raiz\assets\generated\gen_graficos.c"
