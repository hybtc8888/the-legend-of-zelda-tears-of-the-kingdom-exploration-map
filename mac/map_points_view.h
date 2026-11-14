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
    NSDictionary<NSString *, NSDictionary *> *_markerCatalogs, *_markerText;
    NSDictionary<NSString *, NSString *> *_strings, *_categoryLabels;
    NSString *_language, *_effectiveLanguage;
    NSMenuItem *_menuItem;
    NSString *_layerDirectory;
    NSMutableDictionary<NSString *, id> *_icons;
    NSMutableDictionary<NSString *, NSImage *> *_tintedIcons;
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
    BOOL _allKoroksCollected;
    NSUInteger _korokSeeds;
    NSArray<NSNumber *> *_korokProgressIndexes;
    CFTimeInterval _lastProgressRead;
    NSMutableData *_progress;
    NSMutableDictionary<NSString *, NSNumber *> *_progressIndexes;
}

static const NSInteger mapActionCount = 2;
static const unsigned short mapNumberKeys[] = {18,19,20,21,23,22,26,28};

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
        _layerDirectory = [@(path) stringByDeletingLastPathComponent];
        _icons = [NSMutableDictionary dictionary];
        _tintedIcons = [NSMutableDictionary dictionary];
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
        NSDictionary *categoryGroups = mapCategoryGroups();
        NSArray *equipmentPrefixes = mapEquipmentPrefixes(), *equipmentSuffixes = mapEquipmentSuffixes();
        for (NSArray *points in _layers.allValues) {
            for (NSDictionary *point in points) {
                NSString *category = point[@"category"];
                NSString *group = mapPointGroup(point,categoryGroups,equipmentPrefixes,equipmentSuffixes);
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
                _categories = [categories copy];
            }
        }
        if (runtime_setting_word(mapGroupSettingsFile.UTF8String,saved,sizeof saved)) {
            NSData *settings = [NSData dataWithBytes:saved length:strlen(saved)];
            id groups = [NSJSONSerialization JSONObjectWithData:settings options:0 error:NULL];
            if ([groups isKindOfClass:NSDictionary.class]) {
                for (NSString *group in _groupIDs)
                    if ([groups[group] isKindOfClass:NSNumber.class]) _groupOverrides[group] = groups[group];
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
    [_timer invalidate];
    if (_keyboardMonitor) [NSEvent removeMonitor:_keyboardMonitor];
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
    for (NSUInteger digit = 1; digit <= _groupIDs.count; ++digit) {
        NSString *group = _groupIDs[digit-1], *key = [@(digit) stringValue];
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

-(NSString *)iconKeyForPoint:(NSDictionary *)point selected:(BOOL)selected {
    NSString *name = (selected ? mapText(point,@"selected_icon") : nil) ?: mapText(point,@"icon");
    const char *filename = name.UTF8String;
    if (filename && strlen(filename) == 24 && strspn(filename,"0123456789abcdef") == 20 &&
        !strcmp(filename+20,".png")) return name;

    NSString *category = point[@"category"];
    NSString *symbol = @{@"天空群岛":@"cloud.fill", @"地名":@"mappin.circle.fill",
                         @"依盖队设计图":@"doc.text.fill", @"染色店":@"drop.fill"}[category];
    return [@"symbol:" stringByAppendingString:symbol ?: @"mappin.circle.fill"];
}

-(NSImage *)iconForKey:(NSString *)key {
    id cached = _icons[key];
    if (cached) return cached == NSNull.null ? nil : cached;
    NSImage *image;
    if ([key hasPrefix:@"symbol:"]) {
        image = [NSImage imageWithSystemSymbolName:[key substringFromIndex:7] accessibilityDescription:nil];
    } else {
        NSString *directory = [_layerDirectory stringByAppendingPathComponent:@"map-icons"];
        image = [[NSImage alloc] initWithContentsOfFile:[directory stringByAppendingPathComponent:key]];
    }
    _icons[key] = image ?: NSNull.null;
    return image;
}

-(NSImage *)iconForPoint:(NSDictionary *)point selected:(BOOL)selected {
    NSString *key = [self iconKeyForPoint:point selected:selected];
    return [self iconForKey:key] ?: [self iconForKey:@"symbol:mappin.circle.fill"];
}

-(NSImage *)tintedIconForPoint:(NSDictionary *)point group:(NSString *)group selected:(BOOL)selected {
    NSString *key = [self iconKeyForPoint:point selected:selected];
    NSString *cacheKey = [group stringByAppendingFormat:@"/%@",key];
    NSImage *cached = _tintedIcons[cacheKey];
    if (cached) return cached;

    NSImage *original = [self iconForPoint:point selected:selected];
    if (!original) return nil;
    NSColor *color = _groupColors[group];
    NSSize size = NSMakeSize(72,72);
    CGFloat scale = 64 / MAX(original.size.width,original.size.height);
    NSSize glyph = NSMakeSize(original.size.width*scale,original.size.height*scale);
    NSRect bounds = NSMakeRect(0,0,size.width,size.height);
    NSRect image = NSMakeRect((size.width-glyph.width)/2,(size.height-glyph.height)/2,glyph.width,glyph.height);
    NSImage *tinted = [[NSImage alloc] initWithSize:size];
    [tinted lockFocus];
    [original drawInRect:image fromRect:NSZeroRect operation:NSCompositingOperationSourceOver fraction:1];
    [color setFill];
    NSRectFillUsingOperation(bounds,NSCompositingOperationSourceIn);
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

-(void)drawPointDetails:(NSDictionary *)point at:(NSPoint)anchor map:(NSRect)map panel:(NSRect)panel {
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
    CGFloat y = MAX(NSMinY(map)+7, MIN(NSMaxY(map)-height-7, anchor.y+18));
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
    const CGFloat size = MAX(12, MIN(30, (15 + 31*_camera.scale)*unit));
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
        if (dim && !respawns && !_showCompleted) continue;
        CGFloat dx = [point[@"x"] doubleValue]-_camera.x;
        CGFloat dn = [point[@"north"] doubleValue]-_camera.north;
        NSValue *stacked = _coincidentOffsets[mapText(point,@"id") ?: @""];
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
                anchor = p;
            }
        }
        unsigned row = (unsigned)((p.y-NSMinY(map))/gap), col = (unsigned)((p.x-NSMinX(map))/gap);
        NSNumber *cell = @(((uint64_t)row<<32)|col);
        NSArray *prior = markers[cell];
        if (!prior || (!dim && [prior[2] boolValue]))
            markers[cell] = @[point, [NSValue valueWithPoint:p], @(dim)];
    }
    for (NSArray *marker in markers.objectEnumerator)
        [self drawMarker:marker[0] at:[marker[1] pointValue] size:size dim:[marker[2] boolValue] selected:NO];
    if (selected) {
        [self drawPointDetails:selected at:anchor map:map panel:panel];
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
            NSInteger digit = i-mapActionCount+1;
            shortcut = [NSString stringWithFormat:@"⌘⌥%ld",(long)digit];
        } else {
            shortcut = actionKeys[i];
        }
        CGFloat shortcutWidth = group ? 102 : 54;
        [row drawInRect:NSMakeRect(labelX,y+3,MAX(0,x+width-shortcutWidth-labelX),20) withAttributes:label];
        [shortcut drawInRect:NSMakeRect(x+width-shortcutWidth,y+3,shortcutWidth,20) withAttributes:help];
    }
    if (_languagePanel) return;
    NSString *legend = [NSString stringWithFormat:@"⌘⌥1–8 %@",_strings[@"toggle_group"]];
    [legend drawInRect:NSMakeRect(x,NSMinY(panel)+7,width,17) withAttributes:help];
}
@end

static TOTKMapPointsView *mapPointsView;
