# 王国之泪地图 MOD

任何人都可以免费使用、复制、修改、分享或移植本项目的原创代码和文档，无需申请或署名。首版面向 MacNativeGames 的《王国之泪》1.4.3。欢迎志愿者通过公开的数据格式和运行时接口移植到其他平台。

地图点位从玩家自己的游戏文件在本机生成。生成器支持解包后的 RomFS 目录和 MacNativeGames 的 index.json、romfs.bin 格式。需要 Python 3.14。

    python3.14 build_map.py /path/to/romfs /path/to/map-points.json

生成器检查 152 座神庙、120 个树根、15 座鸟望台、30 台左纳乌制造机、800 个单颗呀哈哈和 100 组搬运任务。搬运任务带起点和目的地，合计 1,000 颗种子。当前数据还覆盖宝箱、防具、Boss、洞穴、井、龙之泪等分类。任务起点和动态采集资源尚未纳入。

游戏内集成见 [数据格式和运行时接口](FORMAT.md)。

## 在 Mac 上安装

1. 运行生成器，得到地图包。
2. 在干净的 MacNativeGames 源码目录上运行 `python3.14 mac/integrate.py /path/to/MacNativeGames`。脚本会检查要接入的源码位置。
3. 按 MacNativeGames 的正常流程构建或更新 1.4.3 Mac App。
4. 运行 `python3.14 mac/configure.py /path/to/map-points.json`。按回车保留全部勾选，或输入要关闭的分类编号。
5. 打开游戏，在「显示 → 探索地图」中调整分类和进度显示。

兼容范围为 MacNativeGames 的 Apple Silicon Mac 运行时和《王国之泪》1.4.3。需要其他平台的适配请参考 [数据格式和运行时接口](FORMAT.md)。
