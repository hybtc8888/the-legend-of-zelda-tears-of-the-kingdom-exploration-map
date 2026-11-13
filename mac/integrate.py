"""Integrate the exploration map into a local MacNativeGames source checkout."""
import argparse
from pathlib import Path

HERE = Path(__file__).resolve().parent


def edit(text, old, new):
    if text.count(old) != 1:
        raise ValueError(f"Unrecognized MacNativeGames source near {old.strip()!r}")
    return text.replace(old, new, 1)


def integrate(tree):
    game = tree / "games/totk"
    runtime = game / "runtime"
    filenames = ("map_points.h", "map_points_view.h", "map_runtime.c")
    if any((runtime / name).exists() for name in filenames):
        raise ValueError("Start with a checkout without an existing exploration map integration")

    loader = runtime / "loader.c"
    front = runtime / "frontend.m"
    launcher = game / "app/launcher.m"
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
    build = makefile.read_text()
    start = build.index("\nbuild/totk-native-v143:")
    end = build.index("\n\n", start)
    block = build[start:end]
    build = build[:start] + edit(block, "runtime/loader.c", "runtime/loader.c runtime/map_runtime.c") + build[end:]

    for name in filenames:
        (runtime / name).write_bytes((HERE / name).read_bytes())
    for path, data in [(loader, loader_new), (front, front_new), (launcher, launcher_new), (makefile, build)]:
        path.write_text(data)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    args = parser.parse_args()
    try:
        integrate(args.source)
    except (OSError, ValueError) as error:
        parser.exit(1, f"{error}\n")
