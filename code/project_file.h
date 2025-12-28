#ifndef _H_PROJECT_FILE_
#define _H_PROJECT_FILE_

#include "types.h"
#include "document.h"
#include <string>
#include <memory>

namespace ProjectFile {
    // Magic number and version (kept as "SPP1" for backward compatibility)
    constexpr u32 MAGIC = 0x53505031;  // "SPP1"
    constexpr u32 VERSION = 2;  // Version 2 adds embedded fonts

    // Save document to .pp file
    bool save(const std::string& path, const Document& doc);

    // Load document from .pp file
    std::unique_ptr<Document> load(const std::string& path);

    // Check if file is a valid project file
    bool isProjectFile(const std::string& path);
}

#endif
