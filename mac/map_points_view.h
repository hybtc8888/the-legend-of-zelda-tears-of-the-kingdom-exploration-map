/* Included by the macOS 1.4.3 frontend. The map remains the first responder. */
#include "map_points.h"
#include <errno.h>

@interface TOTKMapPointsView : NSView
-(void)addMenu:(NSMenu *)menu;
@end

@implementation TOTKMapPointsView {
    NSDictionary<NSString *, NSArray *> *_layers;
    NSSet<NSString *> *_categories;
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
    }
    return self;
}
-(void)dealloc { [_timer invalidate]; }
-(NSView *)hitTest:(NSPoint)point { (void)point; return nil; }

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
    BOOL hide = !_enabled || !_seen || !_camera.visible || now - _lastUpdate > 0.15 || _camera.layer >= _layerNames.count;
    if (!hide && _progressEnabled && (self.hidden || now-_lastProgressRead>=0.5)) {
        _lastProgressRead = now;
        changed |= runtime_map_progress(_progress.mutableBytes,_progress.length/sizeof(MapProgressPoint));
    }
    if (self.hidden != hide) { self.hidden = hide; changed = YES; }
    if (changed && !hide) [self setNeedsDisplay:YES];
}
-(void)toggle:(NSMenuItem *)item {
    NSString *name = item.representedObject;
    if (name) {
        NSMutableSet *categories = [_categories mutableCopy];
        if ([categories containsObject:name]) [categories removeObject:name];
        else [categories addObject:name];
        _categories = [categories copy];
        NSData *settings = [NSJSONSerialization dataWithJSONObject:[categories allObjects] options:0 error:NULL];
        NSString *json = [[NSString alloc] initWithData:settings encoding:NSUTF8StringEncoding];
        if (json) runtime_setting_save(mapSettingsFile.UTF8String,json.UTF8String,"map categories");
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
    item.state = (item.representedObject ? [_categories containsObject:item.representedObject] : _enabled)
        ? NSControlStateValueOn : NSControlStateValueOff;
    return YES;
}
-(void)addMenu:(NSMenu *)menu {
    NSMenu *categories = [[NSMenu alloc] initWithTitle:@"探索地图"];
    NSMenuItem *parent = [[NSMenuItem alloc] initWithTitle:categories.title action:nil keyEquivalent:@""];
    parent.submenu = categories;
    [menu addItem:parent];
    NSMenuItem *toggle = [[NSMenuItem alloc] initWithTitle:@"显示点位" action:@selector(toggle:) keyEquivalent:@""];
    toggle.target = self;
    [categories addItem:toggle];
    NSMenuItem *progress = [[NSMenuItem alloc] initWithTitle:@"按探索进度隐藏或变暗" action:@selector(toggleProgress:) keyEquivalent:@""];
    progress.target = self;
    [categories addItem:progress];
    [categories addItem:NSMenuItem.separatorItem];
    NSMutableSet *names = [NSMutableSet set];
    for (NSArray *points in _layers.allValues) {
        for (NSDictionary *point in points) {
            NSString *name = point[@"category"];
            [names addObject:name];
        }
    }
    for (NSString *name in [[names allObjects] sortedArrayUsingSelector:@selector(localizedStandardCompare:)]) {
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
    NSArray *points = _layers[_layerNames[_camera.layer]];
    CGFloat radius = fmax(2.3, 4*unit), gap = 19*unit;
    NSRect cursor = NSMakeRect(center.x-55*unit, center.y-45*unit, 440*unit, 92*unit);
    NSRect confirm = NSMakeRect(center.x-195*unit, center.y-325*unit, 390*unit, 240*unit);
    NSRect layers = NSMakeRect(NSMinX(map), center.y-190*unit, 215*unit, 380*unit);
    CGContextRef context = NSGraphicsContext.currentContext.CGContext;
    CGContextSaveGState(context);
    CGContextClipToRect(context, map);
    NSMutableSet *cells = [NSMutableSet set];
    for (NSDictionary *point in points) {
        NSString *category = point[@"category"];
        if (![_categories containsObject:category]) continue;
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
        if (!NSPointInRect(p, map) || NSPointInRect(p, cursor) || NSPointInRect(p, confirm) || NSPointInRect(p, layers)) continue;
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
}
@end

static TOTKMapPointsView *mapPointsView;
