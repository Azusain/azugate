#include "../../include/file_index.hpp"
#include <algorithm>
#include <spdlog/spdlog.h>

namespace azugate {

std::string DirectoryIndexGenerator::GenerateIndexPage(const std::string& directory_path, const std::string& request_path) {
    try {
        if (!std::filesystem::exists(directory_path) || !std::filesystem::is_directory(directory_path)) {
            return "";
        }
        
        auto files = GetDirectoryListing(directory_path);
        return GenerateHtmlPage(request_path, files);
    } catch (const std::exception& e) {
        SPDLOG_ERROR("Error generating directory index: {}", e.what());
        return "";
    }
}

std::vector<FileInfo> DirectoryIndexGenerator::GetDirectoryListing(const std::string& directory_path) {
    std::vector<FileInfo> files;
    
    try {
        for (const auto& entry : std::filesystem::directory_iterator(directory_path)) {
            FileInfo file_info;
            file_info.name = entry.path().filename().string();
            file_info.path = entry.path().string();
            file_info.is_directory = entry.is_directory();
            
            if (entry.is_regular_file()) {
                std::error_code ec;
                file_info.size = std::filesystem::file_size(entry, ec);
                if (ec) {
                    file_info.size = 0;
                }
            } else {
                file_info.size = 0;
            }
            
            std::error_code ec;
            file_info.last_modified = std::filesystem::last_write_time(entry, ec);
            
            files.push_back(file_info);
        }
        
        // Sort directories first, then files, both alphabetically
        std::sort(files.begin(), files.end(), [](const FileInfo& a, const FileInfo& b) {
            if (a.is_directory != b.is_directory) {
                return a.is_directory > b.is_directory; // directories first
            }
            return a.name < b.name; // alphabetical
        });
        
    } catch (const std::exception& e) {
        SPDLOG_ERROR("Error reading directory {}: {}", directory_path, e.what());
    }
    
    return files;
}

std::string DirectoryIndexGenerator::FormatFileSize(std::uintmax_t size) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    double size_d = static_cast<double>(size);
    size_t unit_index = 0;
    
    while (size_d >= 1024.0 && unit_index < 4) {
        size_d /= 1024.0;
        unit_index++;
    }
    
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << size_d << " " << units[unit_index];
    return oss.str();
}

std::string DirectoryIndexGenerator::FormatTime(std::filesystem::file_time_type file_time) {
    try {
        auto system_time = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            file_time - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now()
        );
        auto time_t = std::chrono::system_clock::to_time_t(system_time);
        
        std::ostringstream oss;
        oss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
        return oss.str();
    } catch (...) {
        return "Unknown";
    }
}

std::string DirectoryIndexGenerator::EscapeHtml(const std::string& text) {
    std::string escaped;
    escaped.reserve(text.size() * 1.1); // Reserve some extra space
    
    for (char c : text) {
        switch (c) {
            case '<': escaped += "&lt;"; break;
            case '>': escaped += "&gt;"; break;
            case '&': escaped += "&amp;"; break;
            case '"': escaped += "&quot;"; break;
            case '\'': escaped += "&#39;"; break;
            default: escaped += c; break;
        }
    }
    
    return escaped;
}

std::string DirectoryIndexGenerator::GenerateHtmlPage(const std::string& request_path, const std::vector<FileInfo>& files) {
    std::ostringstream html;
    
    std::string escaped_path = EscapeHtml(request_path);
    
    html << "<!DOCTYPE html>\n"
         << "<html>\n"
         << "<head>\n"
         << "    <title>Index of " << escaped_path << "</title>\n"
         << "    <style>\n"
         << "        body { font-family: Arial, sans-serif; margin: 40px; }\n"
         << "        h1 { border-bottom: 1px solid #ccc; padding-bottom: 10px; }\n"
         << "        table { border-collapse: collapse; width: 100%; }\n"
         << "        th, td { text-align: left; padding: 8px 12px; border-bottom: 1px solid #ddd; }\n"
         << "        th { background-color: #f5f5f5; font-weight: bold; }\n"
         << "        tr:hover { background-color: #f9f9f9; }\n"
         << "        a { text-decoration: none; color: #0066cc; }\n"
         << "        a:hover { text-decoration: underline; }\n"
         << "        .directory { font-weight: bold; }\n"
         << "        .size { text-align: right; }\n"
         << "        .icon { width: 20px; text-align: center; }\n"
         << "    </style>\n"
         << "</head>\n"
         << "<body>\n"
         << "    <h1>Index of " << escaped_path << "</h1>\n"
         << "    <table>\n"
         << "        <thead>\n"
         << "            <tr>\n"
         << "                <th class=\"icon\"></th>\n"
         << "                <th>Name</th>\n"
         << "                <th class=\"size\">Size</th>\n"
         << "                <th>Last Modified</th>\n"
         << "            </tr>\n"
         << "        </thead>\n"
         << "        <tbody>\n";
    
    // Add parent directory link if not at root
    if (request_path != "/") {
        std::string parent_path = request_path;
        if (parent_path.back() == '/') {
            parent_path.pop_back();
        }
        auto last_slash = parent_path.find_last_of('/');
        if (last_slash != std::string::npos) {
            parent_path = parent_path.substr(0, last_slash + 1);
        } else {
            parent_path = "/";
        }
        
        html << "            <tr>\n"
             << "                <td class=\"icon\">📁</td>\n"
             << "                <td><a href=\"" << EscapeHtml(parent_path) << "\" class=\"directory\">..</a></td>\n"
             << "                <td class=\"size\">-</td>\n"
             << "                <td>-</td>\n"
             << "            </tr>\n";
    }
    
    // Add files and directories
    for (const auto& file : files) {
        std::string file_url = request_path;
        if (file_url.back() != '/') {
            file_url += '/';
        }
        file_url += file.name;
        
        std::string escaped_name = EscapeHtml(file.name);
        std::string escaped_url = EscapeHtml(file_url);
        
        html << "            <tr>\n"
             << "                <td class=\"icon\">" << (file.is_directory ? "📁" : "📄") << "</td>\n"
             << "                <td><a href=\"" << escaped_url << "\""
             << (file.is_directory ? " class=\"directory\"" : "") << ">" << escaped_name << "</a></td>\n"
             << "                <td class=\"size\">" << (file.is_directory ? "-" : FormatFileSize(file.size)) << "</td>\n"
             << "                <td>" << FormatTime(file.last_modified) << "</td>\n"
             << "            </tr>\n";
    }
    
    html << "        </tbody>\n"
         << "    </table>\n"
         << "    <hr>\n"
         << "    <address>Azugate File Server</address>\n"
         << "</body>\n"
         << "</html>\n";
    
    return html.str();
}

} // namespace azugate
