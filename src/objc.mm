#include <Cocoa/Cocoa.h>
#include <string>

std::optional<std::string> FigureOutGDPathObjC() {
	NSArray<NSRunningApplication*>* gd = [[NSWorkspace sharedWorkspace].runningApplications filteredArrayUsingPredicate:[NSPredicate predicateWithBlock:^BOOL(NSRunningApplication* ra, NSDictionary* bindings) {
	    return [ra.localizedName isEqualToString:@"Geometry Dash"];
	}]];
	
	if (gd.count == 0) {
	    return {};
	} else {
	    return std::string(gd[0].bundleURL.path.UTF8String);
	}
}
