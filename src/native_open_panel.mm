#include "native_open_panel.hpp"

#import <AppKit/AppKit.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#include "softloaf_trichrome/model.hpp"

namespace softloaf::trichrome::desktop {

std::vector<std::filesystem::path> NativeOpenFilesOrFolders(const char* title) {
    std::vector<std::filesystem::path> out;
    @autoreleasepool {
        NSOpenPanel* panel = [NSOpenPanel openPanel];
        panel.canChooseFiles = YES;
        panel.canChooseDirectories = YES;
        panel.allowsMultipleSelection = YES;
        panel.resolvesAliases = YES;
        panel.treatsFilePackagesAsDirectories = NO;
        if (title) panel.message = [NSString stringWithUTF8String:title];

        if (@available(macOS 11.0, *)) {
            NSMutableArray<UTType*>* types = [NSMutableArray array];
            for (const std::string& ext : SupportedStillImageExtensions()) {
                const std::string bare_ext = ext.size() > 0 && ext[0] == '.'
                    ? ext.substr(1)
                    : ext;
                NSString* ns_ext = [NSString stringWithUTF8String:bare_ext.c_str()];
                UTType* type = [UTType typeWithFilenameExtension:ns_ext];
                if (type) [types addObject:type];
            }
            if (types.count > 0) panel.allowedContentTypes = types;
        }

        if ([panel runModal] == NSModalResponseOK) {
            for (NSURL* url in panel.URLs) {
                if (url.fileURL && url.path)
                    out.emplace_back(std::filesystem::path(url.path.UTF8String));
            }
        }
    }
    return out;
}

std::filesystem::path NativeChooseExportTarget(const char* title) {
    @autoreleasepool {
        NSOpenPanel* panel = [NSOpenPanel openPanel];
        panel.canChooseFiles = NO;
        panel.canChooseDirectories = YES;
        panel.allowsMultipleSelection = NO;
        panel.canCreateDirectories = YES;
        panel.resolvesAliases = YES;
        panel.treatsFilePackagesAsDirectories = NO;
        if (title) panel.message = [NSString stringWithUTF8String:title];

        if ([panel runModal] == NSModalResponseOK) {
            NSURL* url = panel.URL;
            if (url.fileURL && url.path)
                return std::filesystem::path(url.path.UTF8String);
        }
    }
    return {};
}

}  // namespace softloaf::trichrome::desktop
