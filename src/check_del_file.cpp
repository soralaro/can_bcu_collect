
#include <iostream>
#include <filesystem>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <string>
#include <algorithm>
#include "check_del_file.h"
namespace fs = std::filesystem;
CheckDelFile::CheckDelFile(uint32_t maxFileNum):max_File_num_(maxFileNum){}
CheckDelFile::~CheckDelFile(){}
// 从文件名中提取时间
std::chrono::system_clock::time_point CheckDelFile::extractTimeFromFilename(const std::string& filename) {
   // 查找时间部分的起始位置
   size_t firstUnderscore = filename.find("_");
   if (firstUnderscore == std::string::npos) {
       throw std::invalid_argument("Filename does not contain a valid prefix.");
   }

   size_t secondUnderscore = filename.find("_", firstUnderscore + 1);
   if (secondUnderscore == std::string::npos) {
       throw std::invalid_argument("Filename does not contain a valid timestamp.");
   }

   // 提取时间部分（假设格式为 YYYY-MM-DD_HH:MM:SS）
   std::string timePart = filename.substr(secondUnderscore + 1, 19);
   if (timePart.length() != 19) {
       throw std::invalid_argument("Timestamp format is invalid.");
   }

   // 解析时间字符串
   std::tm tm = {};
   std::istringstream ss(timePart);
   ss >> std::get_time(&tm, "%Y-%m-%d_%H:%M:%S");
   if (ss.fail()) {
       throw std::invalid_argument("Failed to parse timestamp.");
   }

   // 转换为 time_t 并返回 time_point
   return std::chrono::system_clock::from_time_t(std::mktime(&tm));
}

void CheckDelFile::process() {
    std::string directory = "bcu_data";
    fs::path dirPath(directory);

    if (!fs::exists(dirPath) || !fs::is_directory(dirPath)) {
        std::cerr << "Directory does not exist." << std::endl;
        return ;
    }

    std::vector<fs::path> files;
    // 遍历目录下的所有文件
    for (const auto& entry : fs::directory_iterator(dirPath)) {
        if (entry.is_regular_file()) {
            files.push_back(entry.path());
        }
    }

    if (files.size() > max_File_num_) {
        // 找出时间最早的文件
        auto earliestFile = *std::min_element(files.begin(), files.end(), [](const fs::path& a, const fs::path& b) {
            return extractTimeFromFilename(a.filename().string()) < extractTimeFromFilename(b.filename().string());
        });

        std::cout << "Deleting file: " << earliestFile << std::endl;
        // 删除时间最早的文件
        if (fs::remove(earliestFile)) {
            std::cout << "File deleted successfully." << std::endl;
        } else {
            std::cerr << "Failed to delete the file." << std::endl;
        }
    } else {
        std::cout << "The number of files is less than or equal to 100, no deletion will be performed." << std::endl;
    }
}