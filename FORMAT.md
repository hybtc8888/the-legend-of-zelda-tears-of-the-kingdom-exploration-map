# 地图数据格式 v1

文件是 UTF-8 JSON。根对象包含 schema、游戏标题 ID game、version、build_id 和 layers。三层名称是 surface、sky 和 depths。

每个点有唯一的字符串 id、分类 category、东向坐标 x、北向坐标 north 和游戏世界高度 height。坐标以游戏内部米为单位。显示给玩家的高度是另一套坐标，适配器应使用地图投影提供的转换。搬运任务的 destination 使用同一坐标约定。

点位的 progress 可省略。状态键 key 一律用十进制字符串，避免 64 位整数经过 JavaScript 时丢失精度。

| kind | 查询状态 | 显示规则 |
| --- | --- | --- |
| bool | 32 位 GameData 键和值 | 匹配 value 后隐藏 |
| enum | 32 位 GameData 键和值 | 匹配 values 中任一值后隐藏 |
| guid | 64 位一次性状态 | 状态为真时隐藏 |
| respawn | 64 位可重置状态 | 状态为真时变暗，重置后恢复 |

游戏适配器提供当前地图层、可见状态、中心点、比例、视口和同一时刻的只读进度快照。状态不可用时显示普通图标。地图图层不处理传送输入。原版已激活的传送图标优先。分类默认全选，用户的选择保存在本机配置。
