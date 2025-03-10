#ifndef PROJECT_CHECK_DEL_FILE__H
#define PROJECT_CHECK_DEL_FILE__H
#include <iostream>
#include <filesystem>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <string>
#include <algorithm>
class CheckDelFile {
    public:
    CheckDelFile(uint32_t maxFileNum);
    ~CheckDelFile();
    static std::chrono::system_clock::time_point extractTimeFromFilename(const std::string& filename);
    void process();
    private:
        u_int32_t max_File_num_=0;
};
#endif