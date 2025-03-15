#ifndef PROJECT_FILE_MANAGE_H
#define PROJECT_FILE_MANAGE_H
#include <memory>
#include <atomic>
#include <chrono>
#include <queue>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <thread>
class FileManage{
public:
    FileManage(std::queue<std::vector<uint8_t>>& dataQueue,std::mutex& queueMutex,std::condition_variable& queueCondVar,
        std::atomic<bool>& shouldExit, std::atomic<bool>& shouldCloseFile);
    ~FileManage();
    bool createDirectoryIfNotExists(const std::string& path);
    void start();
    u_int32_t getMaxFileNum();
    void SetNoticNewFile();
private:

    std::ofstream  creatNewFile();
    static void fileWriteThread(FileManage *manage);
    std::queue<std::vector<uint8_t>>& dataQueue_;
    std::mutex& queueMutex_;                     // 队列互斥锁
    std::condition_variable& queueCondVar_;      // 条件变量
    std::atomic<bool>& shouldExit_;
    std::atomic<bool>& shouldCloseFile_;
    std::shared_ptr<std::thread> ptrThread_;
    u_int32_t file_size_=0;
    u_int32_t file_size_max_=10*1024*1024;
    u_int32_t max_file_num_=500;
    std::string save_path_="./";
    std::string current_file_name_;
};
#endif