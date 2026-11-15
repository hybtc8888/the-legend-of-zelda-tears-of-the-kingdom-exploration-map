/* Included by the macOS 1.4.3 frontend. The map remains the first responder. */
#include "map_points.h"
#include "map_input.h"
#include "map_locale_data.h"
#include <errno.h>

@interface TOTKMapPointsView : NSView
-(void)addMenu:(NSMenu *)menu;
@end

@implementation TOTKMapPointsView {
    NSDictionary<NSString *, NSArray *> *_layers;
    NSSet<NSString *> *_categories;
    NSArray<NSString *> *_groupIDs;
    NSDictionary<NSString *, NSSet<NSString *> *> *_categoriesByGroup;
    NSDictionary<NSString *, NSString *> *_pointGroups, *_groupLabels;
    NSMutableDictionary<NSString *, NSNumber *> *_groupOverrides;
    NSDictionary<NSString *, NSColor *> *_groupColors;
    NSDictionary<NSString *, NSDictionary *> *_catalogs;
    NSDictionary<NSString *, NSString *> *_categoryGroups;
    NSDictionary<NSString *, NSDictionary *> *_markerCatalogs, *_markerText;
    NSDictionary<NSString *, NSString *> *_strings, *_categoryLabels;
    NSString *_language, *_effectiveLanguage;
    NSMenuItem *_menuItem;
    NSMutableDictionary<NSString *, NSImage *> *_tintedIcons;
    NSDictionary<NSString *, NSString *> *_pointGlyphs;
    NSDictionary<NSString *, NSString *> *_groupGlyphs;
    NSDictionary<NSString *, NSString *> *_activityLabels;
    NSDictionary<NSString *, NSValue *> *_coincidentOffsets;
    NSPoint _pointer;
    BOOL _pointerInside;
    CFTimeInterval _lastPointerMove;
    NSInteger _selectedRow, _firstRow;
    BOOL _mapActive, _panelOpen, _languagePanel;
    NSArray<NSString *> *_languageChoices;
    id _keyboardMonitor;
    NSArray<NSString *> *_layerNames;
    NSTimer *_timer;
    MapPointView _camera;
    uint64_t _seen;
    CFTimeInterval _lastUpdate;
    BOOL _enabled;
    BOOL _showCompleted;
    BOOL _compatibleProgress;
    unsigned _nativeReveals;
    NSDictionary<NSString *, NSNumber *> *_nativeCategories;
    BOOL _allKoroksCollected;
    NSUInteger _korokSeeds;
    NSArray<NSNumber *> *_korokProgressIndexes;
    CFTimeInterval _lastProgressRead;
    NSMutableData *_progress;
    NSMutableDictionary<NSString *, NSNumber *> *_progressIndexes;
}

static const NSInteger mapActionCount = 2;
static const unsigned short mapNumberKeys[] = {18,19,20,21,23,22,26,28,25,29};
static NSString *mapGroupKey(NSUInteger index) {
    return [@"1234567890" substringWithRange:NSMakeRange(index,1)];
}

static NSString *mapText(NSDictionary *point, NSString *key) {
    id value = point[key];
    return [value isKindOfClass:NSString.class] && [value length] ? value : nil;
}

static NSString *mapPointGroup(NSDictionary *point, NSDictionary *categories,
                               NSArray *equipmentPrefixes, NSArray *equipmentSuffixes) {
    NSString *category = point[@"category"];
    if ([category isEqualToString:@"宝箱"]) {
        NSString *item = mapText(point,@"item");
        for (NSString *prefix in equipmentPrefixes)
            if ([item hasPrefix:prefix]) return @"equipment";
        if (!item) {
            NSString *name = mapText(point,@"name"), *reward = mapText(point,@"reward");
            for (NSString *suffix in equipmentSuffixes)
                if ([name hasSuffix:suffix] || [reward hasSuffix:suffix] ||
                    [name hasSuffix:[suffix stringByAppendingString:@"的宝箱"]]) return @"equipment";
        }
    }
    return categories[category] ?: @"collectibles";
}

static NSString *mapCoordinates(NSDictionary *point) {
    return [NSString stringWithFormat:@"%+.0f, %+.0f", [point[@"x"] doubleValue], [point[@"north"] doubleValue]];
}

static NSString *const mapSettingsFile = @"map-points-selection-v1.json";
static NSString *const mapGroupSettingsFile = @"map-groups-v1.json";
static NSString *const mapCompletedSettingsFile = @"map-show-completed.txt";
static NSString *const mapLanguageFile = @"map-language.txt";

-(NSString *)labelForCategory:(NSString *)category {
    return _categoryLabels[category] ?: category;
}

-(NSString *)detailText:(NSDictionary *)point key:(NSString *)key {
    NSString *activity = mapText(point,@"activity");
    if (activity && [key isEqualToString:@"name"] && _activityLabels[activity])
        return _activityLabels[activity];
    NSString *original = mapText(point,key);
    return original ? mapText(_markerText[key],original) ?: original : nil;
}

-(void)useLanguage:(NSString *)language {
    _language = _catalogs[language] ? language : @"auto";
    NSString *effective = _language;
    if ([_language isEqualToString:@"auto"]) {
        effective = @"en";
        for (NSString *preferred in NSLocale.preferredLanguages) {
            NSString *code = [NSLocale localeWithLocaleIdentifier:preferred].languageCode;
            if ([code isEqualToString:@"zh"]) code = @"zh-Hans";
            if (code && _catalogs[code]) {
                effective = code;
                break;
            }
        }
    }
    NSDictionary *catalog = _catalogs[effective];
    _effectiveLanguage = effective;
    _strings = catalog[@"ui"];
    _categoryLabels = catalog[@"categories"];
    _groupLabels = catalog[@"groups"];
    _activityLabels = catalog[@"activities"];
    _markerText = _markerCatalogs[effective];
    _selectedRow = _firstRow = 0;
}

-(void)applyLanguage:(NSString *)code {
    [self useLanguage:code];
    runtime_setting_save(mapLanguageFile.UTF8String, _language.UTF8String, "map language");
    [self updateMenu];
    [self setNeedsDisplay:YES];
}

-(void)changeLanguage:(NSMenuItem *)item {
    [self applyLanguage:item.representedObject];
}

-(NSString *)nameForLanguage:(NSString *)code {
    return _catalogs[code][@"name"] ?: _strings[@"automatic"];
}

-(instancetype)initWithFrame:(NSRect)frame {
    const char *path = getenv("TOTK_MAP_POINTS");
    if (!path || !*path || !strcmp(path, "0")) return nil;
    NSData *data = [NSData dataWithContentsOfFile:@(path)];
    if (!data.length || data.length > 8 * 1024 * 1024) return nil;
    id input = [NSJSONSerialization JSONObjectWithData:data options:0 error:NULL];
    if (![input isKindOfClass:NSDictionary.class] || ![input[@"schema"] isEqual:@1]) return nil;
    NSDictionary *layers = input[@"layers"];
    if (![layers isKindOfClass:NSDictionary.class]) return nil;
    NSMutableDictionary *validated = [NSMutableDictionary dictionary];
    for (NSString *name in @[@"surface", @"sky", @"depths"]) {
        NSArray *points = layers[name];
        if (![points isKindOfClass:NSArray.class] || points.count > 12000) return nil;
        for (id point in points) {
            if (![point isKindOfClass:NSDictionary.class] ||
                ![point[@"category"] isKindOfClass:NSString.class] ||
                ![point[@"x"] isKindOfClass:NSNumber.class] || ![point[@"north"] isKindOfClass:NSNumber.class]) return nil;
            if (fabs([point[@"x"] doubleValue]) > 6500 || fabs([point[@"north"] doubleValue]) > 6500) return nil;
        }
        validated[name] = points;
    }
    BOOL compatibleProgress = [input[@"build_id"] isEqual:@"277178B7DBA1B6D4949A84778D4ABC58B31B34F5"];
    if ((self = [super initWithFrame:frame])) {
        _layers = [validated copy];
        _compatibleProgress = compatibleProgress;
        _nativeCategories = @{@"神庙":@(MAP_NATIVE_SHRINES), @"树根":@(MAP_NATIVE_SHRINES),
            @"鸟望台":@(MAP_NATIVE_TOWERS), @"地名":@(MAP_NATIVE_PLACES),
            @"天空群岛":@(MAP_NATIVE_PLACES),
            @"洞穴入口":@(MAP_NATIVE_ENTRANCES), @"井":@(MAP_NATIVE_ENTRANCES),
            @"地洞入口":@(MAP_NATIVE_ENTRANCES), @"村庄":@(MAP_NATIVE_TOWNS),
            @"驿站":@(MAP_NATIVE_TOWNS), @"龙之泪":@(MAP_NATIVE_TEARS),
            @"残旧的地图":@(MAP_NATIVE_OLD_MAPS), @"左纳乌制造机":@(MAP_NATIVE_DISPENSERS),
            @"神殿":@(MAP_NATIVE_TEMPLES), @"大妖精":@(MAP_NATIVE_FAIRIES)};
        NSMutableDictionary *markerCatalogs = [NSMutableDictionary dictionary];
        if ([input[@"marker_text"] isKindOfClass:NSDictionary.class]) {
            for (NSString *code in mapLanguages()) {
                NSDictionary *locale = input[@"marker_text"][code];
                if (![locale isKindOfClass:NSDictionary.class]) continue;
                NSMutableDictionary *fields = [NSMutableDictionary dictionary];
                for (NSString *key in @[@"name", @"notes", @"reward", @"supplies"]) {
                    if ([locale[key] isKindOfClass:NSDictionary.class]) fields[key] = locale[key];
                }
                markerCatalogs[code] = [fields copy];
            }
        }
        _markerCatalogs = [markerCatalogs copy];
        _tintedIcons = [NSMutableDictionary dictionary];
        _pointGlyphs = @{
            @"神庙":@"5 7 5 13 12 20 19 13 19 7 5 7;9 7 9 12 12 15 15 12 15 7",
            @"树根":@"4 17 12 10 20 17;7 20 12 15 17 20;12 15 12 4;7 7 12 3 17 7",
            @"鸟望台":@"6 5 8 15 5 16 5 19 19 19 19 16 16 15 18 5;8 10 16 10;12 5 12 15",
            @"洞穴入口":@"4 6 4 12 9 19 15 19 20 12 20 6 4 6;9 6 9 12 12 15 15 12 15 6",
            @"井":@"4 13 8 17 16 17 20 13 16 9 8 9 4 13;4 13 5 7 9 5 15 5 19 7 20 13",
            @"地洞入口":@"5 17 12 10 19 17;5 11 12 4 19 11",
            @"残旧的地图":@"5 17 10 19 15 17 19 19 19 7 15 5 10 7 5 5 5 17;10 7 10 19;15 5 15 17",
            @"龙之泪":@"12 20 18 12 18 8 15 5 9 5 6 8 6 12 12 20;10 8 14 8",
            @"左纳乌制造机":@"8 17 10 20 14 20 16 17;6 15 18 15 20 12 17 6 7 6 4 12 6 15;10 10 14 10",
            @"搬运呀哈哈":@"5 8 8 15 13 19 19 19 19 13 16 8 10 5 5 8;8 7 16 15;10 5 19 5 19 8",
            @"村庄":@"4 12 12 20 20 12;6 13 6 5 18 5 18 13;10 5 10 10 14 10 14 5",
            @"驿站":@"5 7 5 12 10 20 13 16 19 14 19 10 12 10 14 7 5 7;10 15 11 15",
            @"大妖精":@"12 20 14 14 20 12 14 10 12 4 10 10 4 12 10 14 12 20",
            @"烹饪点":@"4 14 20 14;6 14 7 8 10 6 14 6 17 8 18 14;9 18 9 20;15 18 15 20",
            @"女神雕像":@"12 20 10 18 12 16 14 18 12 20;12 15 5 12 8 8 9 5 15 5 16 8 19 12 12 15",
            @"矿场":@"5 7 15 17;9 16 14 19 19 17 20 12",
            @"盾牌":@"5 18 19 18 18 11 15 6 12 4 9 6 6 11 5 18;12 15 12 8",
            @"弓与箭":@"6 19 6 5 14 9 17 12 14 15 6 19;4 12 21 12;17 16 21 12 17 8",
            @"防具":@"4 15 8 19 10 16 14 16 16 19 20 15 17 11 16 12 16 5 8 5 8 12 7 11 4 15",
            @"岩石巨人":@"4 8 7 14 10 14 10 19 14 19 14 14 17 14 20 8;7 8 9 5 15 5 17 8",
            @"独眼巨人":@"4 12 8 17 16 17 20 12 16 7 8 7 4 12;10 12 12 14 14 12 12 10 10 12",
            @"方块魔像":@"5 13 12 19 19 13 19 7 12 4 5 7 5 13 12 10 19 13;12 4 12 10",
            @"人马":@"5 18 8 11 12 6 16 11 19 18;7 13 10 14;17 13 14 14;9 7 9 4 15 4 15 7",
            @"巨霸伽马":@"5 15 4 11 7 7 17 7 20 11 19 15;5 15 8 18 16 18 19 15;8 11 16 11",
            @"古栗欧克":@"4 17 8 12 12 17 16 12 20 17;8 12 9 7 12 5 15 7 16 12",
            @"魔犹伊":@"5 14 8 18 16 18 19 14 17 9 14 7 10 7 7 9 5 14;7 9 4 5;17 9 20 5;9 13 15 13",
            @"依盖队设计图":@"6 19 15 19 18 16 18 5 6 5 6 19;9 12 15 12;9 8 15 8",
            @"贤者的遗志":@"12 20 17 15 17 9 12 5 7 9 7 15 12 20;12 7 12 15;8 12 16 12",
            @"卡邦达立牌":@"12 5 12 17;5 17 19 17 19 10 5 10 5 17;9 14 15 14",
        };
        _groupGlyphs = @{
            @"landmarks":@"12 20 17 17 17 12 12 4 7 12 7 17 12 20;11 14 13 14",
            @"collectibles":@"12 20 18 15 18 9 12 4 6 9 6 15 12 20;6 15 18 15;6 9 18 9",
            @"enemies":@"5 19 9 16 15 16 19 19 18 9 15 6 9 6 6 9 5 19;8 12 10 11;14 11 16 12",
            @"chests":@"4 13 7 18 17 18 20 13 20 6 4 6 4 13 20 13;10 13 10 10 14 10 14 13",
            @"koroks":@"5 8 8 15 13 19 19 19 19 13 16 8 10 5 5 8;6 5 16 15;12 11 11 16",
            @"equipment":@"6 5 18 17 19 20 16 19 5 8;4 11 10 5;18 7 19 6;17 8 6 19",
            @"quests":@"6 20 6 4;7 19 18 19 15 15 18 11 7 11;9 7 16 7",
            @"facilities":@"4 12 12 20 20 12;6 13 6 5 18 5 18 13;10 5 10 10 14 10 14 5",
            @"minigames":@"6 5 6 20;6 19 18 19 16 15 18 11 6 11;6 5 10 5",
        };
        NSMutableDictionary *offsets = [NSMutableDictionary dictionary];
        for (NSArray *points in _layers.allValues) {
            NSMutableDictionary *groups = [NSMutableDictionary dictionary];
            for (NSDictionary *point in points) {
                NSArray *coordinate = @[point[@"x"], point[@"north"]];
                NSMutableArray *group = groups[coordinate];
                if (!group) groups[coordinate] = group = [NSMutableArray array];
                NSString *identifier = mapText(point,@"id");
                if (identifier) [group addObject:identifier];
            }
            for (NSArray *group in groups.allValues) {
                if (group.count < 2) continue;
                // Fan out markers that share exact coordinates so each can be hovered.
                CGFloat radius = MAX(.85,group.count/(2*M_PI)*1.25);
                for (NSUInteger i = 0; i < group.count; i++) {
                    CGFloat angle = 2*M_PI*i/group.count;
                    offsets[group[i]] = [NSValue valueWithPoint:NSMakePoint(radius*cos(angle),radius*sin(angle))];
                }
            }
        }
        _coincidentOffsets = [offsets copy];
        _layerNames = @[@"sky", @"surface", @"depths"];
        _groupIDs = mapGroupIDs();
        NSMutableDictionary *groupColors = [NSMutableDictionary dictionary];
        for (NSUInteger i = 0; i < _groupIDs.count; ++i) {
            unsigned rgb = mapGroupColors[i];
            groupColors[_groupIDs[i]] = [NSColor colorWithSRGBRed:((rgb>>16)&255)/255.0
                green:((rgb>>8)&255)/255.0 blue:(rgb&255)/255.0 alpha:1];
        }
        _groupColors = [groupColors copy];
        NSMutableSet *allCategories = [NSMutableSet set];
        NSMutableDictionary *groupMembers = [NSMutableDictionary dictionary];
        NSMutableDictionary *pointGroups = [NSMutableDictionary dictionary];
        _categoryGroups = mapCategoryGroups();
        NSArray *equipmentPrefixes = mapEquipmentPrefixes(), *equipmentSuffixes = mapEquipmentSuffixes();
        for (NSArray *points in _layers.allValues) {
            for (NSDictionary *point in points) {
                NSString *category = point[@"category"];
                NSString *group = mapPointGroup(point,_categoryGroups,equipmentPrefixes,equipmentSuffixes);
                [allCategories addObject:category];
                NSMutableSet *members = groupMembers[group];
                if (!members) groupMembers[group] = members = [NSMutableSet set];
                [members addObject:category];
                NSString *identifier = mapText(point,@"id");
                if (identifier) pointGroups[identifier] = group;
            }
        }
        _categories = [allCategories copy];
        _categoriesByGroup = [groupMembers copy];
        _pointGroups = [pointGroups copy];
        _groupOverrides = [NSMutableDictionary dictionary];
        _catalogs = mapCatalogs();
        _languageChoices = [@[@"auto"] arrayByAddingObjectsFromArray:mapLanguages()];
        char language[32];
        NSString *choice = runtime_setting_word(mapLanguageFile.UTF8String, language, sizeof language)
            ? [@(language) stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceAndNewlineCharacterSet] : @"auto";
        [self useLanguage:choice ?: @"auto"];
        _enabled = YES;
        char saved[8192];
        if (runtime_setting_word(mapSettingsFile.UTF8String, saved, sizeof saved)) {
            NSData *settings = [NSData dataWithBytes:saved length:strlen(saved)];
            id names = [NSJSONSerialization JSONObjectWithData:settings options:0 error:NULL];
            if ([names isKindOfClass:NSArray.class] && [names count]<=128) {
                NSMutableSet *categories = [NSMutableSet set];
                for (id name in names) if ([name isKindOfClass:NSString.class]) [categories addObject:name];
                if ([allCategories containsObject:@"小游戏"]) [categories addObject:@"小游戏"];
                _categories = [categories copy];
            }
        }
        if (runtime_setting_word(mapGroupSettingsFile.UTF8String,saved,sizeof saved)) {
            NSData *settings = [NSData dataWithBytes:saved length:strlen(saved)];
            id groups = [NSJSONSerialization JSONObjectWithData:settings options:0 error:NULL];
            if ([groups isKindOfClass:NSDictionary.class]) {
                for (NSString *group in _groupIDs)
                    if ([groups[group] isKindOfClass:NSNumber.class]) _groupOverrides[group] = groups[group];
                if (!_groupOverrides[@"fairies"] && _groupOverrides[@"facilities"])
                    _groupOverrides[@"fairies"] = _groupOverrides[@"facilities"];
            }
        }
        _showCompleted = !runtime_setting_word(mapCompletedSettingsFile.UTF8String,saved,sizeof saved) ||
            ![[@(saved) stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceAndNewlineCharacterSet] isEqualToString:@"off"];
        NSMutableArray<NSNumber *> *korokIndexes = [NSMutableArray array];
        _progress = [NSMutableData data];
        _progressIndexes = [NSMutableDictionary dictionary];
        if (compatibleProgress) {
            for (NSArray *points in _layers.allValues) for (NSDictionary *point in points) {
                id entry = point[@"progress"];
                NSString *identifier = point[@"id"];
                if (![entry isKindOfClass:NSDictionary.class] || ![identifier isKindOfClass:NSString.class]) continue;
                NSString *keyText = entry[@"key"];
                NSString *kind = entry[@"kind"];
                if (![keyText isKindOfClass:NSString.class] || !keyText.length ||
                    [keyText rangeOfCharacterFromSet:NSCharacterSet.decimalDigitCharacterSet.invertedSet].location!=NSNotFound) continue;
                MapProgressPoint progress = {0};
                char *end = NULL;
                errno = 0;
                progress.key = strtoull(keyText.UTF8String, &end, 10);
                if (errno || !end || *end || !progress.key) continue;
                if ([kind isEqual:@"bool"] && progress.key<=UINT32_MAX) progress.kind=MAP_PROGRESS_BOOL;
                else if ([kind isEqual:@"enum"] && progress.key<=UINT32_MAX) {
                    NSArray *values=entry[@"values"];
                    if (![values isKindOfClass:NSArray.class] || !values.count || values.count>3) continue;
                    for (NSNumber *value in values) {
                        if (![value isKindOfClass:NSNumber.class] || value.unsignedLongLongValue>UINT32_MAX) break;
                        progress.values[progress.value_count++]=value.unsignedIntValue;
                    }
                    if (progress.value_count!=values.count) continue;
                    progress.kind=MAP_PROGRESS_ENUM;
                } else if ([kind isEqual:@"guid"]) progress.kind=MAP_PROGRESS_GUID;
                else if ([kind isEqual:@"respawn"]) progress.kind=MAP_PROGRESS_RESPAWN;
                else continue;
                NSNumber *index = @(_progress.length/sizeof progress);
                _progressIndexes[identifier] = index;
                [_progress appendBytes:&progress length:sizeof progress];
                if ([_pointGroups[identifier] isEqualToString:@"koroks"] &&
                    [point[@"seeds"] isKindOfClass:NSNumber.class]) {
                    NSInteger seeds = [point[@"seeds"] integerValue];
                    if (seeds == 1 || seeds == 2) {
                        _korokSeeds += (NSUInteger)seeds;
                        [korokIndexes addObject:index];
                    }
                }
            }
        }
        _korokProgressIndexes = [korokIndexes copy];
        self.hidden = YES;
        self.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
        self.wantsLayer = YES;
        __weak TOTKMapPointsView *weakSelf = self;
        _timer = [NSTimer timerWithTimeInterval:1.0/30 repeats:YES block:^(NSTimer *timer) {
            TOTKMapPointsView *view = weakSelf;
            if (view) [view refresh];
            else [timer invalidate];
        }];
        [NSRunLoop.mainRunLoop addTimer:_timer forMode:NSRunLoopCommonModes];
        _keyboardMonitor = [NSEvent addLocalMonitorForEventsMatchingMask:NSEventMaskKeyDown
            handler:^NSEvent *(NSEvent *event) { return [weakSelf handleKey:event] ? nil : event; }];
    }
    return self;
}
-(void)dealloc {
    runtime_map_hide_native_koroks(0);
    runtime_map_set_native_reveals(0);
    [_timer invalidate];
    if (_keyboardMonitor) [NSEvent removeMonitor:_keyboardMonitor];
}

-(void)refreshNativeReveals {
    unsigned requested = 0;
    if (_mapActive && _enabled && _compatibleProgress) {
        unsigned disabled = 0;
        for (NSString *category in _nativeCategories) {
            unsigned mask = _nativeCategories[category].unsignedIntValue;
            NSNumber *override = _groupOverrides[_categoryGroups[category]];
            if (override ? override.boolValue : [_categories containsObject:category])
                requested |= mask;
            else
                disabled |= mask;
        }
        requested &= ~disabled;
    }
    _nativeReveals = runtime_map_set_native_reveals(requested);
}

-(BOOL)usesNativeIcon:(NSDictionary *)point {
    if ([point[@"category"] isEqualToString:@"神殿"]) return YES;
    NSString *identifier = mapText(point,@"id");
    if (!identifier || [identifier hasPrefix:@"gs:"]) return NO;
    if ([point[@"category"] isEqualToString:@"小游戏"] && mapText(point,@"game_name"))
        return (_nativeReveals & MAP_NATIVE_PLACES) != 0;
    return (_nativeReveals & _nativeCategories[point[@"category"]].unsignedIntValue) != 0;
}

-(NSRect)panelRect {
    NSRect b = self.bounds;
    CGFloat width = fmin(520, NSWidth(b)-32);
    CGFloat height = fmin(474, NSHeight(b)-64);
    return NSMakeRect(NSMinX(b)+28, NSMinY(b)+46, fmax(0,width), fmax(0,height));
}

-(NSView *)hitTest:(NSPoint)point {
    if (_panelOpen && NSPointInRect([self convertPoint:point fromView:self.superview], [self panelRect])) return self;
    return nil;
}

-(NSInteger)visiblePanelRows {
    return MAX(1, (NSInteger)((NSHeight([self panelRect])-126)/24));
}

-(NSInteger)panelRowCount {
    return _languagePanel ? (NSInteger)_languageChoices.count+1 : mapActionCount+(NSInteger)_groupIDs.count+1;
}

-(NSString *)languageRowTitle {
    NSString *name = [_effectiveLanguage isEqualToString:@"zh-Hans"] ? @"中文" : [self nameForLanguage:_effectiveLanguage];
    return [@"语言Language：" stringByAppendingString:name];
}

-(void)closeLanguagePanel {
    _languagePanel = NO;
    _firstRow = 0;
    _selectedRow = [self panelRowCount]-1;
    [self moveSelection:0];
}

-(void)backFromPanel {
    if (_languagePanel) [self closeLanguagePanel];
    else [self setPanelOpen:NO];
}

-(void)moveSelection:(NSInteger)delta {
    NSInteger total = [self panelRowCount];
    _selectedRow = MAX(0, MIN(total-1, _selectedRow+delta));
    _firstRow = MIN(_firstRow, _selectedRow);
    _firstRow = MAX(_firstRow, _selectedRow-[self visiblePanelRows]+1);
    [self setNeedsDisplay:YES];
}

-(BOOL)inputAvailable {
    return self.window.isKeyWindow && !self.window.attachedSheet && !settingsOpen && !textEntryOpen;
}

-(void)setPanelOpen:(BOOL)open {
    _panelOpen = open;
    _languagePanel = NO;
    if (open) { _selectedRow = _firstRow = 0; runtime_input_clear_window(); }
    runtime_map_input_context(_mapActive && [self inputAvailable], _panelOpen);
    [self refresh];
    [self setNeedsDisplay:YES];
}

-(void)toggleHelp:(id)sender {
    (void)sender;
    if (_mapActive) [self setPanelOpen:!_panelOpen];
}

-(NSControlStateValue)groupState:(NSString *)group {
    NSNumber *override = _groupOverrides[group];
    if (override) return override.boolValue ? NSControlStateValueOn : NSControlStateValueOff;
    NSSet *members = _categoriesByGroup[group];
    if (![members intersectsSet:_categories]) return NSControlStateValueOff;
    return [members isSubsetOfSet:_categories] ? NSControlStateValueOn : NSControlStateValueMixed;
}

-(NSString *)groupForPoint:(NSDictionary *)point {
    return _pointGroups[mapText(point,@"id") ?: @""] ?: @"collectibles";
}

-(void)saveGroupVisibility {
    if (!_groupOverrides[@"fairies"])
        _groupOverrides[@"fairies"] = @([self groupState:@"fairies"] == NSControlStateValueOn);
    NSData *settings = [NSJSONSerialization dataWithJSONObject:_groupOverrides options:0 error:NULL];
    NSString *json = [[NSString alloc] initWithData:settings encoding:NSUTF8StringEncoding];
    if (json) runtime_setting_save(mapGroupSettingsFile.UTF8String,json.UTF8String,"map groups");
    [self refresh];
    [self setNeedsDisplay:YES];
}

-(void)toggleGroup:(NSString *)group {
    _groupOverrides[group] = @([self groupState:group] != NSControlStateValueOn);
    _enabled = YES;
    [self saveGroupVisibility];
}

-(void)toggleGroupMenu:(NSMenuItem *)item {
    [self toggleGroup:item.representedObject];
}

-(void)activateRow {
    if (_languagePanel) {
        if (_selectedRow) [self applyLanguage:_languageChoices[_selectedRow-1]];
        [self closeLanguagePanel];
        return;
    }
    if (_selectedRow == [self panelRowCount]-1) {
        _languagePanel = YES;
        _firstRow = 0;
        _selectedRow = (NSInteger)[_languageChoices indexOfObject:_language]+1;
        [self moveSelection:0];
        return;
    }
    switch (_selectedRow) {
        case 0: [self toggle:nil]; break;
        case 1: if (_progress.length) [self toggleCompleted:nil]; break;
        default: [self toggleGroup:_groupIDs[_selectedRow-mapActionCount]]; break;
    }
    [self setNeedsDisplay:YES];
}

-(BOOL)handleKey:(NSEvent *)event {
    if (event.window != self.window || ![self inputAvailable]) return NO;
    const NSEventModifierFlags modifiers = event.modifierFlags &
        (NSEventModifierFlagCommand | NSEventModifierFlagShift | NSEventModifierFlagControl | NSEventModifierFlagOption);
    const NSEventModifierFlags shift = NSEventModifierFlagCommand | NSEventModifierFlagShift;
    const NSEventModifierFlags toggle = NSEventModifierFlagCommand | NSEventModifierFlagOption;
    if (_mapActive && modifiers == toggle) {
        for (NSUInteger i = 0; i < _groupIDs.count; ++i) {
            if (event.keyCode != mapNumberKeys[i]) continue;
            if (!event.isARepeat) [self toggleGroup:_groupIDs[i]];
            return YES;
        }
    }
    if (modifiers == shift) {
        switch (event.keyCode) {
            case 46: if (!event.isARepeat) [self toggle:nil]; return YES;         /* M */
            case 35: if (!event.isARepeat && _progress.length) [self toggleCompleted:nil]; return YES; /* P */
            case 37: if (!event.isARepeat) [self toggleHelp:nil]; return YES;    /* L */
            default: break;
        }
    }
    if (!_panelOpen || (modifiers & NSEventModifierFlagCommand)) return NO;
    switch (event.keyCode) {
        case 126: [self moveSelection:-1]; break;
        case 125: [self moveSelection:1]; break;
        case 123: [self moveSelection:-[self visiblePanelRows]]; break;
        case 124: [self moveSelection:[self visiblePanelRows]]; break;
        case 36: case 49: if (!event.isARepeat) [self activateRow]; break;
        case 53: [self backFromPanel]; break;
        default: break;
    }
    return YES;
}

-(void)mouseDown:(NSEvent *)event {
    NSPoint point = [self convertPoint:event.locationInWindow fromView:nil];
    NSRect panel = [self panelRect];
    CGFloat rowOffset = NSMaxY(panel)-96-point.y;
    if (rowOffset < 0) return;
    NSInteger visible = (NSInteger)(rowOffset/24);
    NSInteger row = _firstRow+visible;
    if (visible >= [self visiblePanelRows] || row >= [self panelRowCount]) return;
    _selectedRow = row;
    [self activateRow];
}

-(void)scrollWheel:(NSEvent *)event {
    if (event.scrollingDeltaY) [self moveSelection:event.scrollingDeltaY>0 ? -1 : 1];
}

-(void)refresh {
    MapPointView next;
    CFTimeInterval now = CACurrentMediaTime();
    BOOL changed = NO;
    if (runtime_map_point_view(&next) && next.sequence != _seen) {
        _seen = next.sequence;
        _lastUpdate = now;
        changed = _camera.x != next.x || _camera.north != next.north ||
            _camera.scale != next.scale || _camera.layer != next.layer || _camera.visible != next.visible;
        _camera = next;
    }
    BOOL active = _seen && _camera.visible && now - _lastUpdate <= 0.15 && _camera.layer < _layerNames.count;
    if (!active && _mapActive) {
        _panelOpen = NO;
        _languagePanel = NO;
    }
    _mapActive = active;
    BOOL inputActive = active && [self inputAvailable];
    NSPoint mouse = inputActive ? [self convertPoint:self.window.mouseLocationOutsideOfEventStream fromView:nil] : NSZeroPoint;
    BOOL inside = inputActive && NSPointInRect(mouse, self.bounds);
    BOOL moved = inside && (!NSEqualPoints(mouse, _pointer) || !_pointerInside);
    if (mouseHeld || !inside || (changed && !moved && now-_lastPointerMove > .25))
        _lastPointerMove = 0;
    if (moved && !mouseHeld) _lastPointerMove = now;
    changed |= moved || inside != _pointerInside;
    _pointerInside = inside;
    _pointer = mouse;
    if (_panelOpen && (settingsOpen || textEntryOpen)) {
        _panelOpen = _languagePanel = NO;
        changed = YES;
    }
    runtime_map_input_context(inputActive, _panelOpen);
    unsigned actions = runtime_map_input_events();
    if (inputActive && actions) {
        if (actions & MAP_PAD_LAYER) { _enabled = !_enabled; changed = YES; }
        if (actions & MAP_PAD_CLOSE) {
            if (_languagePanel) [self closeLanguagePanel];
            else _panelOpen = NO;
            changed = YES;
        } else if (actions & MAP_PAD_OPEN) {
            _panelOpen = YES;
            _languagePanel = NO;
            _selectedRow = _firstRow = 0;
            runtime_input_clear_window();
            changed = YES;
        }
        if (_panelOpen && !(actions & MAP_PAD_OPEN)) {
            if (actions & MAP_PAD_UP) [self moveSelection:-1];
            if (actions & MAP_PAD_DOWN) [self moveSelection:1];
            if (actions & MAP_PAD_LEFT) [self moveSelection:-[self visiblePanelRows]];
            if (actions & MAP_PAD_RIGHT) [self moveSelection:[self visiblePanelRows]];
            if (actions & MAP_PAD_SELECT) [self activateRow];
        }
        runtime_map_input_context(inputActive, _panelOpen);
    }
    runtime_map_hide_native_koroks(_enabled);
    [self refreshNativeReveals];
    BOOL hide = !active || (!_enabled && !_panelOpen);
    if (active && _enabled && _progress.length && (self.hidden || now-_lastProgressRead>=0.5)) {
        _lastProgressRead = now;
        if (runtime_map_progress(_progress.mutableBytes,_progress.length/sizeof(MapProgressPoint))) {
            changed = YES;
            const MapProgressPoint *state = _progress.bytes;
            _allKoroksCollected = _korokSeeds == 1000 && _korokProgressIndexes.count == 900;
            for (NSNumber *index in _korokProgressIndexes) {
                if (!state[index.unsignedIntegerValue].done) {
                    _allKoroksCollected = NO;
                    break;
                }
            }
        }
    }
    if (self.hidden != hide) { self.hidden = hide; changed = YES; }
    if (changed && !hide) [self setNeedsDisplay:YES];
}
-(void)toggle:(NSMenuItem *)item {
    (void)item;
    _enabled = !_enabled;
    [self refresh];
    [self setNeedsDisplay:YES];
}

-(void)toggleCompleted:(NSMenuItem *)item {
    (void)item;
    _showCompleted = !_showCompleted;
    runtime_setting_save(mapCompletedSettingsFile.UTF8String,_showCompleted ? "on" : "off","show completed map points");
    [self setNeedsDisplay:YES];
}

-(BOOL)validateMenuItem:(NSMenuItem *)item {
    if (item.action == @selector(changeLanguage:)) {
        item.state = [_language isEqualToString:item.representedObject] ? NSControlStateValueOn : NSControlStateValueOff;
        return YES;
    }
    if (item.action == @selector(toggleCompleted:)) {
        item.state = _showCompleted ? NSControlStateValueOn : NSControlStateValueOff;
        return _progress.length>0;
    }
    if (item.action == @selector(toggleHelp:)) {
        item.state = _panelOpen ? NSControlStateValueOn : NSControlStateValueOff;
        return _mapActive && !settingsOpen && !textEntryOpen;
    }
    item.state = item.action == @selector(toggleGroupMenu:) ? [self groupState:item.representedObject] :
        _enabled ? NSControlStateValueOn : NSControlStateValueOff;
    return YES;
}
-(void)addMenu:(NSMenu *)menu {
    _menuItem = [[NSMenuItem alloc] initWithTitle:_strings[@"title"] action:nil keyEquivalent:@""];
    [menu addItem:_menuItem];
    [self updateMenu];
}

-(void)updateMenu {
    _menuItem.title = _strings[@"title"];
    NSMenu *categories = [[NSMenu alloc] initWithTitle:_menuItem.title];
    _menuItem.submenu = categories;
    NSMenuItem *toggle = [[NSMenuItem alloc] initWithTitle:_strings[@"show_layer"] action:@selector(toggle:) keyEquivalent:@"m"];
    toggle.keyEquivalentModifierMask = NSEventModifierFlagCommand | NSEventModifierFlagShift;
    toggle.target = self;
    [categories addItem:toggle];
    NSMenuItem *progress = [[NSMenuItem alloc] initWithTitle:_strings[@"progress"] action:@selector(toggleCompleted:) keyEquivalent:@"p"];
    progress.keyEquivalentModifierMask = NSEventModifierFlagCommand | NSEventModifierFlagShift;
    progress.target = self;
    [categories addItem:progress];
    NSMenuItem *help = [[NSMenuItem alloc] initWithTitle:_strings[@"controls"] action:@selector(toggleHelp:) keyEquivalent:@"l"];
    help.keyEquivalentModifierMask = NSEventModifierFlagCommand | NSEventModifierFlagShift;
    help.target = self;
    [categories addItem:help];
    [categories addItem:NSMenuItem.separatorItem];
    NSMenu *visible = [[NSMenu alloc] initWithTitle:_strings[@"visible_groups"]];
    NSMenuItem *visibleItem = [[NSMenuItem alloc] initWithTitle:visible.title action:nil keyEquivalent:@""];
    visibleItem.submenu = visible;
    [categories addItem:visibleItem];
    for (NSUInteger i = 0; i < _groupIDs.count; ++i) {
        NSString *group = _groupIDs[i], *key = mapGroupKey(i);
        NSString *label = _groupLabels[group];
        NSMenuItem *toggleGroup = [[NSMenuItem alloc] initWithTitle:label action:@selector(toggleGroupMenu:) keyEquivalent:key];
        toggleGroup.keyEquivalentModifierMask = NSEventModifierFlagCommand | NSEventModifierFlagOption;
        toggleGroup.target = self;
        toggleGroup.representedObject = group;
        [visible addItem:toggleGroup];
    }
    [categories addItem:NSMenuItem.separatorItem];
    NSMenu *language = [[NSMenu alloc] initWithTitle:_strings[@"language"]];
    NSMenuItem *languageItem = [[NSMenuItem alloc] initWithTitle:language.title action:nil keyEquivalent:@""];
    languageItem.submenu = language;
    [categories addItem:languageItem];
    for (NSString *code in _languageChoices) {
        NSString *title = [self nameForLanguage:code];
        NSMenuItem *item = [[NSMenuItem alloc] initWithTitle:title action:@selector(changeLanguage:) keyEquivalent:@""];
        item.target = self;
        item.representedObject = code;
        [language addItem:item];
    }

}

-(NSImage *)tintedIconForPoint:(NSDictionary *)point group:(NSString *)group selected:(BOOL)selected {
    NSString *glyph = _pointGlyphs[point[@"category"]] ?: _groupGlyphs[group];
    NSString *cacheKey = [group stringByAppendingFormat:@"/%d/%@",selected,glyph];
    NSImage *cached = _tintedIcons[cacheKey];
    if (cached) return cached;

    NSColor *color = _groupColors[group] ?: NSColor.whiteColor;
    NSImage *tinted = [[NSImage alloc] initWithSize:NSMakeSize(72,72)];
    [tinted lockFocus];
    NSBezierPath *frame = [NSBezierPath bezierPath];
    [frame moveToPoint:NSMakePoint(19,6)];
    for (NSValue *corner in @[[NSValue valueWithPoint:NSMakePoint(6,19)], [NSValue valueWithPoint:NSMakePoint(6,53)],
         [NSValue valueWithPoint:NSMakePoint(19,66)], [NSValue valueWithPoint:NSMakePoint(53,66)],
         [NSValue valueWithPoint:NSMakePoint(66,53)], [NSValue valueWithPoint:NSMakePoint(66,19)],
         [NSValue valueWithPoint:NSMakePoint(53,6)]]) [frame lineToPoint:corner.pointValue];
    [frame closePath];
    [[NSColor colorWithSRGBRed:.04 green:.12 blue:.17 alpha:.94] setFill];
    [frame fill];
    [[color colorWithAlphaComponent:selected ? 1 : .67] setStroke];
    frame.lineWidth = selected ? 4 : 2.5;
    [frame stroke];
    NSBezierPath *sign = [NSBezierPath bezierPath];
    for (NSString *stroke in [glyph componentsSeparatedByString:@";"]) {
        NSScanner *scanner = [NSScanner scannerWithString:stroke];
        double x,y;
        if (![scanner scanDouble:&x] || ![scanner scanDouble:&y]) continue;
        [sign moveToPoint:NSMakePoint(9+2.25*x,9+2.25*y)];
        while ([scanner scanDouble:&x] && [scanner scanDouble:&y])
            [sign lineToPoint:NSMakePoint(9+2.25*x,9+2.25*y)];
    }
    sign.lineJoinStyle = NSLineJoinStyleRound;
    sign.lineCapStyle = NSLineCapStyleRound;
    sign.lineWidth = selected ? 6 : 5.3;
    [color setStroke];
    [sign stroke];
    [tinted unlockFocus];
    _tintedIcons[cacheKey] = tinted;
    return tinted;
}

-(void)drawMarker:(NSDictionary *)point at:(NSPoint)position size:(CGFloat)size dim:(BOOL)dim selected:(BOOL)selected {
    NSString *group = [self groupForPoint:point];
    NSImage *icon = [self tintedIconForPoint:point group:group selected:selected];
    if (!icon) return;
    if (selected) size *= 1.23;
    NSRect box = NSMakeRect(position.x-size/2, position.y-size/2, size, size);
    [NSGraphicsContext saveGraphicsState];
    NSShadow *edge = [NSShadow new];
    edge.shadowColor = [NSColor colorWithWhite:.015 alpha:dim ? .50 : .95];
    edge.shadowBlurRadius = selected ? 2.4 : 1.65;
    edge.shadowOffset = NSMakeSize(0,-.7);
    [edge set];
    [icon drawInRect:box fromRect:NSZeroRect operation:NSCompositingOperationSourceOver fraction:dim ? .38 : 1];
    [NSGraphicsContext restoreGraphicsState];
}

-(void)drawPointDetails:(NSDictionary *)point at:(NSPoint)anchor map:(NSRect)map panel:(NSRect)panel native:(BOOL)native {
    const CGFloat inset = 12;
    CGFloat width = MIN(380, NSWidth(map)*.46);
    if (width < 110 || NSHeight(map) < 90) return;
    NSString *category = [self labelForCategory:point[@"category"]];
    NSString *name = [self detailText:point key:@"name"] ?: category;
    NSString *heading = [NSString stringWithFormat:@"%@  ·  %@", category, mapCoordinates(point)];
    NSString *notes = [self detailText:point key:@"notes"];
    NSString *reward = [self detailText:point key:@"reward"] ?: [self detailText:point key:@"supplies"];
    NSDictionary *origin = [point[@"origin"] isKindOfClass:NSDictionary.class] ? point[@"origin"] : nil;
    NSDictionary *destination = [point[@"destination"] isKindOfClass:NSDictionary.class] ? point[@"destination"] : nil;
    NSMutableArray *lines = [NSMutableArray array];
    if (notes) [lines addObject:notes];
    if (reward && ![notes containsString:reward] &&
        ![name containsString:reward])
        [lines addObject:[NSString stringWithFormat:@"%@  %@",_strings[@"reward"],reward]];
    if (destination) {
        [lines addObject:[NSString stringWithFormat:@"%@  %@",_strings[@"route"],
            [NSString stringWithFormat:@"%@  →  %@",mapCoordinates(origin ?: point),mapCoordinates(destination)]]];
    }
    NSString *body = [lines componentsJoinedByString:@"\n"];
    NSMutableParagraphStyle *style = [NSMutableParagraphStyle new];
    style.lineBreakMode = NSLineBreakByCharWrapping;
    style.lineSpacing = 2;
    NSDictionary *titleAttrs = @{NSFontAttributeName:[NSFont boldSystemFontOfSize:15],
        NSForegroundColorAttributeName:NSColor.whiteColor, NSParagraphStyleAttributeName:style};
    NSDictionary *headingAttrs = @{NSFontAttributeName:[NSFont systemFontOfSize:11],
        NSForegroundColorAttributeName:[NSColor colorWithSRGBRed:.62 green:.85 blue:.94 alpha:1]};
    CGFloat textWidth = width-2*inset;
    NSStringDrawingOptions options = NSStringDrawingUsesLineFragmentOrigin | NSStringDrawingUsesFontLeading;
    CGFloat titleHeight = ceil([name boundingRectWithSize:NSMakeSize(textWidth,CGFLOAT_MAX)
        options:options attributes:titleAttrs].size.height);
    CGFloat maxBody = MAX(0, NSHeight(map)-titleHeight-62);
    CGFloat fontSize = 13, bodyHeight = 0;
    NSDictionary *bodyAttrs;
    do {
        bodyAttrs = @{NSFontAttributeName:[NSFont systemFontOfSize:fontSize],
            NSForegroundColorAttributeName:[NSColor colorWithWhite:.95 alpha:1], NSParagraphStyleAttributeName:style};
        bodyHeight = body.length ? ceil([body boundingRectWithSize:NSMakeSize(textWidth,CGFLOAT_MAX)
            options:options attributes:bodyAttrs].size.height) : 0;
        fontSize -= 1;
    } while (bodyHeight > maxBody && fontSize >= 9);
    bodyHeight = MIN(bodyHeight,maxBody);
    CGFloat height = titleHeight+2*inset+18+(bodyHeight ? bodyHeight+7 : 0);
    CGFloat x = MIN(NSMaxX(map)-width-7, anchor.x+18);
    if (x <= anchor.x+8) x = MAX(NSMinX(map)+7, anchor.x-width-18);
    CGFloat offset = native ? 48 : 18;
    CGFloat y = MAX(NSMinY(map)+7, MIN(NSMaxY(map)-height-7, anchor.y+offset));
    NSRect box = NSMakeRect(x,y,width,height);
    if (NSContainsRect(NSInsetRect(box,-5,-5), NSMakeRect(anchor.x,anchor.y,1,1)) ||
        NSIntersectsRect(box,panel)) {
        CGFloat left = NSMaxX(panel)+8;
        if (left+width <= NSMaxX(map)-7) box.origin.x = left;
        else if (anchor.y-height-18 >= NSMinY(map)) box.origin.y = anchor.y-height-18;
    }
    NSBezierPath *surface = [NSBezierPath bezierPathWithRoundedRect:box xRadius:9 yRadius:9];
    [[NSColor colorWithSRGBRed:.025 green:.07 blue:.115 alpha:.96] setFill];
    [surface fill];
    [[NSColor colorWithSRGBRed:.38 green:.72 blue:.84 alpha:.9] setStroke];
    [surface stroke];
    const CGFloat left = NSMinX(box)+inset;
    CGFloat top = NSMaxY(box)-inset;
    [name drawWithRect:NSMakeRect(left,top-titleHeight,textWidth,titleHeight) options:options attributes:titleAttrs];
    top -= titleHeight+5;
    [heading drawInRect:NSMakeRect(left,top-13,textWidth,15) withAttributes:headingAttrs];
    if (bodyHeight) {
        top -= 19;
        [body drawWithRect:NSMakeRect(left,top-bodyHeight,textWidth,bodyHeight)
            options:options|NSStringDrawingTruncatesLastVisibleLine attributes:bodyAttrs];
    }
}

-(void)drawRect:(NSRect)dirty {
    (void)dirty;
    if (_camera.layer >= _layerNames.count) return;
    NSRect bounds = self.bounds;
    CGFloat unit = fmin(NSWidth(bounds)/1920, NSHeight(bounds)/1080);
    CGFloat pixelsPerMeter = unit * _camera.scale * 3;
    NSPoint center = NSMakePoint(NSMidX(bounds), NSMidY(bounds));
    NSRect map = NSInsetRect(NSMakeRect(center.x-960*unit,center.y-540*unit,1920*unit,1080*unit),105*unit,82*unit);
    if (NSIsEmptyRect(map)) return;
    NSArray *points = _enabled ? _layers[_layerNames[_camera.layer]] : @[];
    const CGFloat size = MAX(14, MIN(30, (15 + 31*_camera.scale)*unit));
    const CGFloat gap = MAX(5,size*.7);
    NSRect layers = NSMakeRect(NSMinX(map),center.y-190*unit,215*unit,380*unit);
    NSRect panel = _panelOpen ? [self panelRect] : NSZeroRect;
    BOOL pointer = _pointerInside && NSPointInRect(_pointer,map);
    // Moving the mouse selects by pointer; panning the map can return selection to its reticle.
    BOOL direct = pointer && _lastPointerMove > 0;
    NSPoint target = direct ? _pointer : center;
    CGFloat closest = MAX(12,size*.73);
    BOOL hover = !_panelOpen && !settingsOpen && !textEntryOpen && self.window.isKeyWindow &&
        !NSPointInRect(target,layers);
    NSDictionary *selected = nil;
    NSPoint anchor = NSZeroPoint;
    BOOL selectedDim = NO;
    BOOL selectedNative = NO;

    CGContextRef context = NSGraphicsContext.currentContext.CGContext;
    CGContextSaveGState(context);
    CGContextClipToRect(context,map);
    NSMutableDictionary<NSNumber *, NSArray *> *markers = [NSMutableDictionary dictionary];
    for (NSDictionary *point in points) {
        NSString *category = point[@"category"];
        NSString *group = [self groupForPoint:point];
        NSNumber *override = _groupOverrides[group];
        BOOL accepted = override ? override.boolValue : [_categories containsObject:category];
        if (!accepted) continue;
        NSString *identifier = mapText(point,@"id");
        BOOL native = [self usesNativeIcon:point];
        NSNumber *index = _progressIndexes[point[@"id"]];
        BOOL dim = NO, respawns = NO;
        if (index) {
            const MapProgressPoint *state = _progress.bytes;
            MapProgressPoint progress = state[index.unsignedIntegerValue];
            dim = progress.done;
            respawns = progress.kind == MAP_PROGRESS_RESPAWN;
        } else if (_allKoroksCollected && [group isEqualToString:@"koroks"]) {
            // The source can contain duplicate Korok annotations with no game flag.
            dim = YES;
        }
        // The game removes collected Old Maps from its native marker list.
        if (dim && [category isEqualToString:@"残旧的地图"]) native = NO;
        if (dim && [category isEqualToString:@"小游戏"] && mapText(point,@"game_name")) native = YES;
        if (dim && !respawns && !_showCompleted && !native) continue;
        CGFloat dx = [point[@"x"] doubleValue]-_camera.x;
        CGFloat dn = [point[@"north"] doubleValue]-_camera.north;
        NSValue *stacked = native ? nil : _coincidentOffsets[identifier ?: @""];
        NSPoint offset = stacked.pointValue;
        NSPoint p = NSMakePoint(center.x+dx*pixelsPerMeter+offset.x*size,
                                center.y+dn*pixelsPerMeter+offset.y*size);
        if (!NSPointInRect(p,map) || NSPointInRect(p,layers) || NSPointInRect(p,panel)) continue;
        if (hover) {
            CGFloat distance = hypot(p.x-target.x,p.y-target.y);
            if (distance < closest) {
                closest = distance;
                selected = point;
                selectedDim = dim;
                selectedNative = native;
                anchor = p;
            }
        }
        if (native) continue;
        unsigned row = (unsigned)((p.y-NSMinY(map))/gap), col = (unsigned)((p.x-NSMinX(map))/gap);
        NSNumber *cell = @(((uint64_t)row<<32)|col);
        NSArray *prior = markers[cell];
        if (!prior || (!dim && [prior[2] boolValue]))
            markers[cell] = @[point, [NSValue valueWithPoint:p], @(dim)];
    }
    for (NSArray *marker in markers.objectEnumerator)
        [self drawMarker:marker[0] at:[marker[1] pointValue] size:size dim:[marker[2] boolValue] selected:NO];
    if (selected) {
        [self drawPointDetails:selected at:anchor map:map panel:panel native:selectedNative];
        if (!selectedNative)
            [self drawMarker:selected at:anchor size:size dim:selectedDim selected:YES];
    }
    CGContextRestoreGState(context);
    if (_panelOpen) [self drawPanel];
}

-(void)drawPanel {
    NSRect panel = [self panelRect];
    [[NSColor colorWithSRGBRed:0.035 green:0.075 blue:0.11 alpha:0.97] setFill];
    [[NSBezierPath bezierPathWithRoundedRect:panel xRadius:12 yRadius:12] fill];
    NSDictionary *title = @{NSFontAttributeName:[NSFont boldSystemFontOfSize:17], NSForegroundColorAttributeName:NSColor.whiteColor};
    NSDictionary *help = @{NSFontAttributeName:[NSFont systemFontOfSize:12], NSForegroundColorAttributeName:[NSColor colorWithWhite:0.85 alpha:1]};
    NSDictionary *label = @{NSFontAttributeName:[NSFont systemFontOfSize:14], NSForegroundColorAttributeName:NSColor.whiteColor};
    CGFloat x = NSMinX(panel)+16, top = NSMaxY(panel), width = NSWidth(panel)-32;
    [_strings[_languagePanel ? @"language" : @"title"] drawInRect:NSMakeRect(x,top-30,width,24) withAttributes:title];
    NSArray *helpKeys = @[@"help_layer", @"help_keyboard", @"help_gamepad"];
    for (NSUInteger i = 0; i < helpKeys.count; i++) {
        NSString *line = _strings[helpKeys[i]];
        CGFloat lineWidth = [line sizeWithAttributes:help].width;
        NSDictionary *attributes = help;
        if (lineWidth > width && width > 0) {
            attributes = @{NSFontAttributeName:[NSFont systemFontOfSize:12*width/lineWidth],
                           NSForegroundColorAttributeName:help[NSForegroundColorAttributeName]};
        }
        [line drawInRect:NSMakeRect(x,top-50-i*18,width,18) withAttributes:attributes];
    }
    NSArray *actionKeys = @[@"⌘⇧M", @"⌘⇧P"];
    NSArray *actionNames = @[@"show_layer", @"progress"];
    NSInteger total = [self panelRowCount];
    NSInteger end = MIN(total,_firstRow+[self visiblePanelRows]);
    for (NSInteger i = _firstRow; i < end; i++) {
        CGFloat y = top-96-(i-_firstRow+1)*24;
        if (i == _selectedRow) {
            [[NSColor colorWithSRGBRed:0.15 green:0.31 blue:0.41 alpha:1] setFill];
            [[NSBezierPath bezierPathWithRoundedRect:NSMakeRect(x-4,y,width+8,24) xRadius:5 yRadius:5] fill];
        }
        if (_languagePanel) {
            NSString *code = i ? _languageChoices[i-1] : nil;
            NSString *name = code ? [self nameForLanguage:code] : _strings[@"back"];
            NSString *mark = code ? ([_language isEqualToString:code] ? @"●" : @"○") : @"‹";
            [[NSString stringWithFormat:@"%@  %@",mark,name]
                drawInRect:NSMakeRect(x,y+3,width,20) withAttributes:label];
            continue;
        }
        if (i == total-1) {
            [[self languageRowTitle] drawInRect:NSMakeRect(x,y+3,width,20) withAttributes:label];
            continue;
        }
        NSString *group = i < mapActionCount ? nil : _groupIDs[i-mapActionCount];
        NSString *name = group ? _groupLabels[group] : _strings[actionNames[i]];
        NSControlStateValue state = group ? [self groupState:group] :
            (i==0 ? _enabled : _showCompleted) ? NSControlStateValueOn : NSControlStateValueOff;
        NSString *mark = state == NSControlStateValueOn ? @"☑" : state == NSControlStateValueMixed ? @"◩" : @"☐";
        NSString *row = [NSString stringWithFormat:@"%@  %@",mark,name];
        CGFloat labelX = x;
        NSString *shortcut = nil;
        if (group) {
            [_groupColors[group] setFill];
            [[NSBezierPath bezierPathWithRoundedRect:NSMakeRect(x+1,y+7,10,10) xRadius:2 yRadius:2] fill];
            labelX += 19;
            shortcut = [@"⌘⌥" stringByAppendingString:mapGroupKey((NSUInteger)(i-mapActionCount))];
        } else {
            shortcut = actionKeys[i];
        }
        CGFloat shortcutWidth = group ? 102 : 54;
        [row drawInRect:NSMakeRect(labelX,y+3,MAX(0,x+width-shortcutWidth-labelX),20) withAttributes:label];
        [shortcut drawInRect:NSMakeRect(x+width-shortcutWidth,y+3,shortcutWidth,20) withAttributes:help];
    }
    if (_languagePanel) return;
    NSString *legend = [NSString stringWithFormat:@"⌘⌥1–9, 0 %@",_strings[@"toggle_group"]];
    [legend drawInRect:NSMakeRect(x,NSMinY(panel)+7,width,17) withAttributes:help];
}
@end

static TOTKMapPointsView *mapPointsView;
