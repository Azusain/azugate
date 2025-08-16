#ifndef __FILE_INDEX_H
#define __FILE_INDEX_H

#include <string>
#include <vector>
#include <filesystem>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace azugate {

struct FileInfo {
    std::string name;
    std::string path;
    std::uintmax_t size;
    std::filesystem::file_time_type last_modified;
    bool is_directory;
};

class DirectoryIndexGenerator {
public:
    static std::string GenerateIndexPage(const std::string& directory_path, const std::string& request_path);
    
private:
    static std::vector<FileInfo> GetDirectoryListing(const std::string& directory_path);
    static std::string FormatFileSize(std::uintmax_t size);
    static std::string FormatTime(std::filesystem::file_time_type file_time);
    static std::string EscapeHtml(const std::string& text);
    static std::string GenerateHtmlPage(const std::string& request_path, const std::vector<FileInfo>& files);
};

} // namespace azugate

#endif
