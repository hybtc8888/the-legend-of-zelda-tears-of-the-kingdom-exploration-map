"""Integrate the exploration map into a local MacNativeGames source checkout."""
import argparse
from pathlib import Path

from map_locale import render_header

HERE = Path(__file__).resolve().parent


def edit(text, old, new):
    if text.count(old) != 1:
        raise ValueError(f"Unrecognized MacNativeGames source near {old.strip()!r}")
    return text.replace(old, new, 1)


def integrate(tree):
    game = tree / "games/totk"
    runtime = game / "runtime"
    filenames = ("map_points.h", "map_points_view.h", "map_runtime.c", "map_input.h", "map_input.c")
    catalog = runtime / "map_locale_data.h"
    catalog_text = render_header()
    installed = [(runtime / name).is_file() for name in filenames]
    if any(installed) or catalog.exists():
        hooks = (
            (runtime / "frontend.m", '#include "map_points_view.h"'),
            (runtime / "loader.c", "runtime_map_install(&modules[0])"),
            (game / "Makefile", "runtime/map_runtime.c runtime/map_input.c"),
        )
        if not all(installed) or not all(marker in path.read_text() for path, marker in hooks):
            raise ValueError("Incomplete exploration map integration; use a clean MacNativeGames checkout")
        for name in filenames:
            (runtime / name).write_bytes((HERE / name).read_bytes())
        catalog.write_text(catalog_text, encoding="utf-8")
        return

    loader = runtime / "loader.c"
    front = runtime / "frontend.m"
    launcher = game / "app/launcher.m"
    input_source = runtime / "input.c"
    gamepad = runtime / "gamepad.m"
    makefile = game / "Makefile"

    line = 'for(unsigned i=0;i<NM;i++)if(!strcmp(modules[i].name,"main"))runtime_game_patches(&modules[i]);'
    loader_new = edit(loader.read_text(), line, line + '''
#if defined(TOTK_GAME_V143) && !TARGET_OS_IPHONE
 {extern void runtime_map_install(Module *);runtime_map_install(&modules[0]);}
#endif''')

    front_new = edit(front.read_text(), "static TOTKMenuActions *menuActions;",
        '''static TOTKMenuActions *menuActions;
#if defined(TOTK_GAME_V143)
#include "map_points_view.h"
#endif''')
    front_new = edit(front_new, " front_menubar_fps_add(menu);NSApp.mainMenu=menu;", '''
#if defined(TOTK_GAME_V143)
 [mapPointsView addMenu:view];
#endif
 front_menubar_fps_add(menu);NSApp.mainMenu=menu;''')
    front_new = edit(front_new, "  front_hdr_init();\n  front_install_menu();", '''  front_hdr_init();
#if defined(TOTK_GAME_V143)
  mapPointsView=[[TOTKMapPointsView alloc] initWithFrame:NSZeroRect];
#endif
  front_install_menu();''')
    line = "  [window.contentView addSubview:bootOverlay];[bootProgress startAnimation:nil];"
    front_new = edit(front_new, line, line + '''
#if defined(TOTK_GAME_V143)
  if(mapPointsView){
   mapPointsView.frame=window.contentView.bounds;
   [window.contentView addSubview:mapPointsView positioned:NSWindowBelow relativeTo:bootOverlay];
  }
#endif''')

    line = '        NSString *logs = [home stringByAppendingPathComponent:@"Library/Logs/MacNativeGames"];'
    launcher_new = edit(launcher.read_text(), line, '''
#if defined(TOTK_GAME_V143)
        if (!getenv("TOTK_MAP_POINTS")) {
            NSString *points = [data stringByAppendingPathComponent:@"map-points-v1.json"];
            if (access(points.fileSystemRepresentation, R_OK) == 0)
                setenv("TOTK_MAP_POINTS", points.fileSystemRepresentation, 0);
        }
#endif
''' + line)
    input_new = edit(input_source.read_text(), " }*out=state;pthread_mutex_unlock(&inputLock);", """
#if defined(TOTK_GAME_V143) && !TARGET_OS_IPHONE
 if(!ignore_user()){
  extern void runtime_map_filter_input(uint64_t, uint64_t *, int32_t *, int32_t *, int32_t *, int32_t *, uint64_t);
  runtime_map_filter_input(pad.buttons,&state.buttons,&state.lx,&state.ly,&state.rx,&state.ry,runtime_now_ns());
 }
#endif
 }*out=state;pthread_mutex_unlock(&inputLock);""")
    gamepad_new = edit(gamepad.read_text(), """    if (touchState.connected) out->connected = 1;
    vr_pad_merge(out, 0);""", """    if (touchState.connected) out->connected = 1;
#if defined(TOTK_GAME_V143) && TARGET_OS_OSX
    extern void runtime_map_input_physical(uint64_t, uint16_t, int);
    runtime_map_input_physical(textPad.buttons, controllerMap[5], !textCapture);
#endif
    vr_pad_merge(out, 0);""")

    build = makefile.read_text()
    start = build.index("\nbuild/totk-native-v143:")
    end = build.index("\n\n", start)
    block = build[start:end]
    build = build[:start] + edit(block, "runtime/loader.c", "runtime/loader.c runtime/map_runtime.c runtime/map_input.c") + build[end:]

    for name in filenames:
        (runtime / name).write_bytes((HERE / name).read_bytes())
    catalog.write_text(catalog_text, encoding="utf-8")
    for path, data in [(loader, loader_new), (front, front_new), (launcher, launcher_new), (input_source, input_new),
                       (gamepad, gamepad_new), (makefile, build)]:
        path.write_text(data)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    args = parser.parse_args()
    try:
        integrate(args.source)
    except (OSError, ValueError) as error:
        parser.exit(1, f"{error}\n")
