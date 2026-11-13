/* Included by the macOS 1.4.3 frontend. The map remains the first responder. */
#include "map_points.h"
#include "map_input.h"
#include <errno.h>

@interface TOTKMapPointsView : NSView
-(void)addMenu:(NSMenu *)menu;
@end

@implementation TOTKMapPointsView {
    NSDictionary<NSString *, NSArray *> *_layers;
    NSSet<NSString *> *_categories;
    NSArray<NSString *> *_categoryNames;
    NSInteger _preset, _selectedRow, _firstRow;
    BOOL _mapActive, _panelOpen;
    id _keyboardMonitor;
    NSArray<NSString *> *_layerNames;
    NSTimer *_timer;
    MapPointView _camera;
    uint64_t _seen;
    CFTimeInterval _lastUpdate;
    BOOL _enabled;
    BOOL _progressEnabled;
    CFTimeInterval _lastProgressRead;
    NSMutableData *_progress;
    NSMutableDictionary<NSString *, NSNumber *> *_progressIndexes;
}

static const NSInteger mapActionCount = 6;

static NSArray<NSString *> *mapActionTitles(void) {
    return @[@"显示探索图层", @"按探索进度隐藏或变暗", @"仅显示地标",
             @"仅显示收集点", @"仅显示 Boss 和人马", @"恢复自选分类"];
}

static BOOL mapCategoryInPreset(NSString *category, NSInteger preset) {
    switch (preset) {
        case 1: return [@[@"神庙", @"树根", @"鸟望台", @"神殿", @"村庄", @"驿站", @"地名", @"天空群岛",
                          @"洞穴入口", @"地洞入口", @"井", @"左纳乌制造机"] containsObject:category];
        case 2: return [@[@"呀哈哈", @"搬运呀哈哈", @"宝箱", @"防具", @"贤者的遗志", @"残旧的地图",
                          @"依盖队设计图", @"设计图石板", @"龙之泪", @"魔犹伊", @"卡邦达立牌"] containsObject:category];
        case 3: return [@[@"人马", @"岩石巨人", @"古栗欧克", @"巨霸伽马", @"方块魔像",
                          @"独眼巨人", @"莫尔德拉吉克"] containsObject:category];
        default: return NO;
    }
}

static NSString *const mapSettingsFile = @"map-points-selection-v1.json";

-(instancetype)initWithFrame:(NSRect)frame {
    const char *path = getenv("TOTK_MAP_POINTS");
    if (!path || !*path || !strcmp(path, "0")) return nil;
    NSData *data = [NSData dataWithContentsOfFile:@(path)];
    if (!data.length || data.length > 2 * 1024 * 1024) return nil;
    id input = [NSJSONSerialization JSONObjectWithData:data options:0 error:NULL];
    if (![input isKindOfClass:NSDictionary.class] || ![input[@"schema"] isEqual:@1]) return nil;
    NSDictionary *layers = input[@"layers"];
    if (![layers isKindOfClass:NSDictionary.class]) return nil;
    NSMutableDictionary *validated = [NSMutableDictionary dictionary];
    for (NSString *name in @[@"surface", @"sky", @"depths"]) {
        NSArray *points = layers[name];
        if (![points isKindOfClass:NSArray.class] || points.count > 4000) return nil;
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
        _layerNames = @[@"sky", @"surface", @"depths"];
        NSMutableSet *allCategories = [NSMutableSet set];
        for (NSArray *points in _layers.allValues)
            for (NSDictionary *point in points) [allCategories addObject:point[@"category"]];
        _categories = [allCategories copy];
        _categoryNames = [allCategories.allObjects sortedArrayUsingSelector:@selector(localizedStandardCompare:)];
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
        _progressEnabled = compatibleProgress;
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
                _progressIndexes[identifier] = @(_progress.length/sizeof progress);
                [_progress appendBytes:&progress length:sizeof progress];
            }
        }
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
    [_timer invalidate];
    if (_keyboardMonitor) [NSEvent removeMonitor:_keyboardMonitor];
}

-(NSRect)panelRect {
    NSRect b = self.bounds;
    CGFloat width = fmin(440, NSWidth(b)-32);
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

-(void)moveSelection:(NSInteger)delta {
    NSInteger total = mapActionCount + (NSInteger)_categoryNames.count;
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
    if (open) { _selectedRow = _firstRow = 0; runtime_input_clear_window(); }
    runtime_map_input_context(_mapActive && [self inputAvailable], _panelOpen);
    [self refresh];
    [self setNeedsDisplay:YES];
}

-(void)toggleHelp:(id)sender {
    (void)sender;
    if (_mapActive) [self setPanelOpen:!_panelOpen];
}

-(void)selectPreset:(NSInteger)preset {
    _preset = _preset == preset ? 0 : preset;
    if (_preset) _enabled = YES;
    [self refresh];
    [self setNeedsDisplay:YES];
}

-(void)presetMenu:(NSMenuItem *)item {
    if (_mapActive) [self selectPreset:item.tag];
}

-(void)toggleCategory:(NSString *)name {
    NSMutableSet *categories = [_categories mutableCopy];
    if ([categories containsObject:name]) [categories removeObject:name];
    else [categories addObject:name];
    _categories = [categories copy];
    _preset = 0;
    NSData *settings = [NSJSONSerialization dataWithJSONObject:categories.allObjects options:0 error:NULL];
    NSString *json = [[NSString alloc] initWithData:settings encoding:NSUTF8StringEncoding];
    if (json) runtime_setting_save(mapSettingsFile.UTF8String,json.UTF8String,"map categories");
}

-(void)activateRow {
    switch (_selectedRow) {
        case 0: [self toggle:nil]; break;
        case 1: if (_progress.length) [self toggleProgress:nil]; break;
        case 2: case 3: case 4: [self selectPreset:_selectedRow-1]; break;
        case 5: [self selectPreset:0]; break;
        default: [self toggleCategory:_categoryNames[_selectedRow-mapActionCount]]; break;
    }
    [self setNeedsDisplay:YES];
}

-(BOOL)handleKey:(NSEvent *)event {
    if (event.window != self.window || ![self inputAvailable]) return NO;
    const NSEventModifierFlags modifiers = event.modifierFlags &
        (NSEventModifierFlagCommand | NSEventModifierFlagShift | NSEventModifierFlagControl | NSEventModifierFlagOption);
    if (modifiers == (NSEventModifierFlagCommand | NSEventModifierFlagShift)) {
        switch (event.keyCode) {
            case 46: if (!event.isARepeat) [self toggle:nil]; return YES;         /* M */
            case 35: if (!event.isARepeat && _progress.length) [self toggleProgress:nil]; return YES; /* P */
            case 37: if (!event.isARepeat) [self toggleHelp:nil]; return YES;    /* L */
            case 18: case 19: case 20: case 29:                                /* 1, 2, 3, 0 */
                if (!event.isARepeat && _mapActive) [self selectPreset:event.keyCode==29 ? 0 : event.keyCode-17];
                return _mapActive;
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
        case 53: [self setPanelOpen:NO]; break;
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
    if (visible >= [self visiblePanelRows] || row >= mapActionCount+(NSInteger)_categoryNames.count) return;
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
        _preset = 0;
    }
    _mapActive = active;
    BOOL inputActive = active && [self inputAvailable];
    if (_panelOpen && (settingsOpen || textEntryOpen)) { _panelOpen = NO; changed = YES; }
    runtime_map_input_context(inputActive, _panelOpen);
    unsigned actions = runtime_map_input_events();
    if (inputActive && actions) {
        if (actions & MAP_PAD_LAYER) { _enabled = !_enabled; changed = YES; }
        if (actions & MAP_PAD_CLOSE) { _panelOpen = NO; changed = YES; }
        else if (actions & MAP_PAD_OPEN) { _panelOpen = YES; _selectedRow = _firstRow = 0; runtime_input_clear_window(); changed = YES; }
        if (_panelOpen && !(actions & MAP_PAD_OPEN)) {
            if (actions & MAP_PAD_UP) [self moveSelection:-1];
            if (actions & MAP_PAD_DOWN) [self moveSelection:1];
            if (actions & MAP_PAD_LEFT) [self moveSelection:-[self visiblePanelRows]];
            if (actions & MAP_PAD_RIGHT) [self moveSelection:[self visiblePanelRows]];
            if (actions & MAP_PAD_SELECT) [self activateRow];
        }
        runtime_map_input_context(inputActive, _panelOpen);
    }
    BOOL hide = !active || (!_enabled && !_panelOpen);
    if (active && _enabled && _progressEnabled && (self.hidden || now-_lastProgressRead>=0.5)) {
        _lastProgressRead = now;
        changed |= runtime_map_progress(_progress.mutableBytes,_progress.length/sizeof(MapProgressPoint));
    }
    if (self.hidden != hide) { self.hidden = hide; changed = YES; }
    if (changed && !hide) [self setNeedsDisplay:YES];
}
-(void)toggle:(NSMenuItem *)item {
    NSString *name = item.representedObject;
    if (name) {
        [self toggleCategory:name];
    } else {
        _enabled = !_enabled;
    }
    [self refresh];
    [self setNeedsDisplay:YES];
}
-(void)toggleProgress:(NSMenuItem *)item {
    (void)item;
    _progressEnabled = !_progressEnabled;
    _lastProgressRead = 0;
    [self refresh];
    [self setNeedsDisplay:YES];
}
-(BOOL)validateMenuItem:(NSMenuItem *)item {
    if (item.action == @selector(toggleProgress:)) {
        item.state = _progressEnabled ? NSControlStateValueOn : NSControlStateValueOff;
        return _progress.length>0;
    }
    if (item.action == @selector(toggleHelp:) || item.action == @selector(presetMenu:)) {
        item.state = (item.action == @selector(toggleHelp:) ? _panelOpen : item.tag && _preset==item.tag)
            ? NSControlStateValueOn : NSControlStateValueOff;
        return _mapActive && !settingsOpen && !textEntryOpen;
    }
    item.state = (item.representedObject ? [_categories containsObject:item.representedObject] : _enabled)
        ? NSControlStateValueOn : NSControlStateValueOff;
    return YES;
}
-(void)addMenu:(NSMenu *)menu {
    NSMenu *categories = [[NSMenu alloc] initWithTitle:@"探索地图"];
    NSMenuItem *parent = [[NSMenuItem alloc] initWithTitle:categories.title action:nil keyEquivalent:@""];
    parent.submenu = categories;
    [menu addItem:parent];
    NSMenuItem *toggle = [[NSMenuItem alloc] initWithTitle:@"显示点位" action:@selector(toggle:) keyEquivalent:@"m"];
    toggle.keyEquivalentModifierMask = NSEventModifierFlagCommand | NSEventModifierFlagShift;
    toggle.target = self;
    [categories addItem:toggle];
    NSMenuItem *progress = [[NSMenuItem alloc] initWithTitle:@"按探索进度隐藏或变暗" action:@selector(toggleProgress:) keyEquivalent:@"p"];
    progress.keyEquivalentModifierMask = NSEventModifierFlagCommand | NSEventModifierFlagShift;
    progress.target = self;
    [categories addItem:progress];
    NSMenuItem *help = [[NSMenuItem alloc] initWithTitle:@"分类与快捷键" action:@selector(toggleHelp:) keyEquivalent:@"l"];
    help.keyEquivalentModifierMask = NSEventModifierFlagCommand | NSEventModifierFlagShift;
    help.target = self;
    [categories addItem:help];
    [categories addItem:NSMenuItem.separatorItem];
    for (NSInteger i = 0; i < 4; i++) {
        NSInteger tag = (i+1)%4;
        NSMenuItem *item = [[NSMenuItem alloc] initWithTitle:mapActionTitles()[i+2] action:@selector(presetMenu:)
            keyEquivalent:[NSString stringWithFormat:@"%ld",(long)tag]];
        item.keyEquivalentModifierMask = NSEventModifierFlagCommand | NSEventModifierFlagShift;
        item.target = self;
        item.tag = tag;
        [categories addItem:item];
    }
    [categories addItem:NSMenuItem.separatorItem];
    for (NSString *name in _categoryNames) {
        NSMenuItem *item = [[NSMenuItem alloc] initWithTitle:name action:@selector(toggle:) keyEquivalent:@""];
        item.target = self;
        item.representedObject = name;
        [categories addItem:item];
    }
}

-(void)drawRect:(NSRect)dirty {
    (void)dirty;
    if (_camera.layer >= _layerNames.count) return;
    NSRect bounds = self.bounds;
    CGFloat width = NSWidth(bounds), height = NSHeight(bounds);
    CGFloat unit = fmin(width/1920, height/1080);
    CGFloat pixelsPerMeter = unit * _camera.scale * 3;
    NSPoint center = NSMakePoint(NSMidX(bounds), NSMidY(bounds));
    NSRect map = NSInsetRect(NSMakeRect(center.x - 960*unit, center.y - 540*unit, 1920*unit, 1080*unit),
                             105*unit, 82*unit);
    NSArray *points = _enabled ? _layers[_layerNames[_camera.layer]] : @[];
    CGFloat radius = fmax(2.3, 4*unit), gap = 19*unit;
    NSRect cursor = NSMakeRect(center.x-55*unit, center.y-45*unit, 440*unit, 92*unit);
    NSRect confirm = NSMakeRect(center.x-195*unit, center.y-325*unit, 390*unit, 240*unit);
    NSRect layers = NSMakeRect(NSMinX(map), center.y-190*unit, 215*unit, 380*unit);
    CGContextRef context = NSGraphicsContext.currentContext.CGContext;
    CGContextSaveGState(context);
    CGContextClipToRect(context, map);
    NSMutableSet *cells = [NSMutableSet set];
    NSRect panel = _panelOpen ? [self panelRect] : NSZeroRect;
    for (NSDictionary *point in points) {
        NSString *category = point[@"category"];
        if (_preset ? !mapCategoryInPreset(category,_preset) : ![_categories containsObject:category]) continue;
        NSNumber *index = _progressEnabled ? _progressIndexes[point[@"id"]] : nil;
        BOOL dim = NO;
        if (index) {
            const MapProgressPoint *state = _progress.bytes;
            MapProgressPoint progress = state[index.unsignedIntegerValue];
            if (progress.done && progress.kind!=MAP_PROGRESS_RESPAWN) continue;
            dim = progress.done && progress.kind==MAP_PROGRESS_RESPAWN;
        }
        CGFloat dx = [point[@"x"] doubleValue] - _camera.x;
        CGFloat dn = [point[@"north"] doubleValue] - _camera.north;
        NSPoint p = NSMakePoint(center.x + dx * pixelsPerMeter, center.y + dn * pixelsPerMeter);
        if (!NSPointInRect(p, map) || NSPointInRect(p, cursor) || NSPointInRect(p, confirm) || NSPointInRect(p, layers) || NSPointInRect(p, panel)) continue;
        unsigned row = (unsigned)((p.y - NSMinY(map))/gap), col = (unsigned)((p.x - NSMinX(map))/gap);
        NSNumber *cell = @(((uint64_t)row << 32) | col);
        if ([cells containsObject:cell]) continue;
        [cells addObject:cell];
        if (dim) CGContextSetRGBFillColor(context,.5,.5,.5,.52);
        else if ([category containsString:@"呀哈哈"]) CGContextSetRGBFillColor(context,.56,.87,.42,.95);
        else if ([category isEqualToString:@"防具"]) CGContextSetRGBFillColor(context,.95,.54,.85,.95);
        else if ([category isEqualToString:@"贤者的遗志"]) CGContextSetRGBFillColor(context,.55,.85,.99,.95);
        else CGContextSetRGBFillColor(context,1,.72,.33,.95);
        CGRect icon = CGRectMake(p.x-radius,p.y-radius,2*radius,2*radius);
        CGContextSetRGBStrokeColor(context,.08,.1,.14,.9);
        CGContextSetLineWidth(context,fmax(1,1.4*unit));
        CGContextFillEllipseInRect(context,icon);
        CGContextStrokeEllipseInRect(context,icon);
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
    [@"探索地图" drawInRect:NSMakeRect(x,top-30,width,24) withAttributes:title];
    [@"⌘⇧L / R3 收起　 长按 R3 开关图层" drawInRect:NSMakeRect(x,top-50,width,18) withAttributes:help];
    [@"方向键选择　回车确认　Esc 返回" drawInRect:NSMakeRect(x,top-68,width,18) withAttributes:help];
    [@"手柄十字键选择　游戏确认键切换　返回键收起" drawInRect:NSMakeRect(x,top-86,width,18) withAttributes:help];
    NSArray *keys = @[@"⌘⇧M", @"⌘⇧P", @"⌘⇧1", @"⌘⇧2", @"⌘⇧3", @"⌘⇧0"];
    NSInteger total = mapActionCount+(NSInteger)_categoryNames.count;
    NSInteger end = MIN(total,_firstRow+[self visiblePanelRows]);
    for (NSInteger i = _firstRow; i < end; i++) {
        CGFloat y = top-96-(i-_firstRow+1)*24;
        if (i == _selectedRow) {
            [[NSColor colorWithSRGBRed:0.15 green:0.31 blue:0.41 alpha:1] setFill];
            [[NSBezierPath bezierPathWithRoundedRect:NSMakeRect(x-4,y,width+8,24) xRadius:5 yRadius:5] fill];
        }
        NSString *name = i<mapActionCount ? mapActionTitles()[i] : _categoryNames[i-mapActionCount];
        BOOL checked = i==0 ? _enabled : i==1 ? _progressEnabled : i==5 ? !_preset
            : i<mapActionCount ? _preset==i-1 : [_categories containsObject:name];
        NSString *row = [NSString stringWithFormat:@"%@  %@",checked ? @"☑" : @"☐",name];
        [row drawInRect:NSMakeRect(x,y+3,width-62,20) withAttributes:label];
        if (i<mapActionCount) [keys[i] drawInRect:NSMakeRect(x+width-56,y+3,56,20) withAttributes:help];
    }
    [[NSString stringWithFormat:@"%ld / %ld",(long)(_selectedRow+1),(long)total]
        drawInRect:NSMakeRect(x,NSMinY(panel)+7,width,17) withAttributes:help];
}
@end

static TOTKMapPointsView *mapPointsView;
