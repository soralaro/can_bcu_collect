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
private:
    /* data */
public:
    FileManage(std::queue<std::vector<uint8_t>>& dataQueue,std::mutex& queueMutex,std::condition_variable& queueCondVar,
        std::atomic<bool>& shouldExit);
    ~FileManage();
    static bool createDirectoryIfNotExists(const std::string& path);
    void start();
private:
    static void fileWriteThread(FileManage *manage);
    std::queue<std::vector<uint8_t>>& dataQueue_;
    std::mutex& queueMutex_;                     // 队列互斥锁
    std::condition_variable& queueCondVar_;      // 条件变量
    std::atomic<bool>& shouldExit_;
    std::shared_ptr<std::thread> ptrThread_;
};
#endif