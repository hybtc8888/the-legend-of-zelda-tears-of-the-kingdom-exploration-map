# The Legend of Zelda: Tears of the Kingdom — Mapa de exploración de Hyrule

[English](../../README.md) · [简体中文](../zh-Hans/README.md) · [日本語](../ja/README.md) · **Español** · [Русский](../ru/README.md) · [Français](../fr/README.md) · [Deutsch](../de/README.md)

Este mod añade una capa al mapa del juego **1.4.3** en MacNativeGames para Mac con chip de Apple. Los puntos siguen las vistas del cielo, la superficie y el subsuelo, así como el desplazamiento y el zoom. Los santuarios y las raíces activadas conservan sus iconos y el viaje rápido del juego.

- **4428 puntos en 35 categorías**, con las **1000 semillas de kolog**: 800 pruebas de una semilla y 100 encargos de dos. El paquete registra la salida y el destino de cada traslado.
- Incluye 152 santuarios, 120 raíces, 15 atalayas y 30 generadores de artilugios zonnan, además de cuevas, pozos, cofres, armaduras, lágrimas de dragón y jefes.
- Los puntos con seguimiento se ocultan al completarlos. Los jefes y centaleones que reaparecen se atenúan al derrotarlos y recuperan el brillo al volver. Puedes desactivar el filtro para ver los puntos completados.
- Todas las categorías están activadas al principio. Puedes guardar tu selección y usar filtros temporales de lugares, coleccionables o jefes y centaleones. Los inicios de misiones y los recursos de recolección dinámicos quedan fuera de la cobertura actual.

## Instalación en Mac

Necesitas un Mac con chip de Apple, Python 3.14, el código fuente de MacNativeGames, tu juego base y la actualización **1.4.3**. Consulta la [guía de compilación de MacNativeGames en inglés](https://github.com/ly05010419/MacNativeGames/blob/main/docs/install.md).

1. Desde la raíz de este proyecto, ejecuta `python3.14 mac/integrate.py "/path/to/MacNativeGames"`.
2. Compila o actualiza la app de Mac para la versión 1.4.3 desde ese código. Si vas a actualizarla, cierra el juego primero.
3. Con la RomFS 1.4.3 extraída, normalmente en `totk/extracted/v143/romfs`, ejecuta `python3.14 build_map.py "/path/to/romfs" map-points.json`.
4. Instala con `python3.14 mac/configure.py map-points.json --language es`. Pulsa Intro para conservar todas las categorías o escribe los números que quieres excluir, separados por espacios. Añade `--all` para omitir la pregunta.

Cambia de idioma en **View → Mapa de exploración → Idioma** en la barra de menús del Mac. `--language auto` usa el idioma del sistema. La selección de categorías se conserva al cambiar de idioma. El paquete se genera a partir de tus archivos y el mod lee el progreso del juego sin editar partidas guardadas.

## Controles

Abre el mapa del juego con **Tab / M** o el botón **−** del mando. Cambia de nivel con **3 / 5** o con **↑ / ↓** en la cruceta.

| Teclas | Acción |
| --- | --- |
| **⌘⇧L** | Mostrar u ocultar el panel de categorías y atajos |
| **⌘⇧M** | Activar o desactivar la capa |
| **⌘⇧P** | Activar o desactivar el filtro de progreso |
| **⌘⇧1 / 2 / 3** | Ver temporalmente lugares / coleccionables / jefes y centaleones |
| **⌘⇧0** | Restaurar las categorías elegidas |

Pulsa **R3** brevemente para abrir o cerrar el panel o mantenlo pulsado unos 0,7 segundos para alternar la capa. En el panel, la cruceta selecciona filas con arriba/abajo y cambia de página con izquierda/derecha. Usa los botones de confirmar y volver del juego. También puedes usar flechas, Intro y Esc, o el ratón y la rueda. Elige otra vez el mismo filtro temporal o sal del mapa para restaurar tus categorías.

[Formato del paquete en inglés](../map-pack.md) · [Licencia MIT](../../LICENSE)
