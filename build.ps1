# Compila las ROMs: convierte los gráficos (Python + Pillow + numpy) y llama a GBDK-2020.
# Requiere GBDK-2020 (https://github.com/gbdk-2020/gbdk-2020): define GBDK_HOME con su carpeta
# o deja `lcc` en el PATH.
#   .\build.ps1                    compila los tres sistemas
#   .\build.ps1 -Sistema ubuntu    solo uno (macos9 | ubuntu | windows11)
param([ValidateSet("todos", "macos9", "ubuntu", "windows11")][string]$Sistema = "todos")
$raiz = $PSScriptRoot
if ($env:GBDK_HOME) { $env:PATH = "$env:GBDK_HOME\bin;$env:PATH" }
New-Item -ItemType Directory -Force "$raiz\build" | Out-Null

function Compilar-MacOS9 {
    python "$raiz\tools\convert_assets.py"
    # -yC: solo Game Boy Color · -yt0x03 -ya1: MBC1 + RAM con batería (guardado del fondo)
    lcc -Wm-yC -Wm-yt0x03 -Wm-ya1 -Wm-yn"MACOS9GB" -I"$raiz\assets\generated" -I"$raiz\src" -o "$raiz\build\MacOS9.gbc" (Get-ChildItem "$raiz\src\*.c").FullName "$raiz\assets\generated\gen_graficos.c"
}

# Ubuntu y Windows 11 comparten src\sistema\sistema.c (con -DSIS_...) y las utilidades de pantalla y cursor del Mac OS 9
function Compilar-Sistema($carpeta, $define, $nombre, $salida) {
    python "$raiz\tools\convert_sistema.py" $carpeta
    $gen = "$raiz\assets\generated\$carpeta"
    lcc -Wm-yC -Wm-yt0x01 -Wm-yn"$nombre" $define -I"$gen" -I"$raiz\src" -o "$raiz\build\$salida" "$raiz\src\sistema\sistema.c" "$raiz\src\ui.c" "$raiz\src\cursor.c" "$gen\gen_graficos.c"
}

if ($Sistema -in "todos", "macos9") { Compilar-MacOS9 }
if ($Sistema -in "todos", "ubuntu") { Compilar-Sistema "ubuntu" "-DSIS_UBUNTU" "UBUNTUGB" "UbuntuGB.gbc" }
if ($Sistema -in "todos", "windows11") { Compilar-Sistema "windows11" "-DSIS_WINDOWS" "WIN11GB" "Windows11GB.gbc" }
