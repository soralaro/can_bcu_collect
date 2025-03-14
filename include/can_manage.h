#ifndef PROJECT_CAN_MANAGE_H
#define PROJECT_CAN_MANAGE_H
#include <memory>
#include <atomic>
#include <chrono>
#include <queue>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <sys/socket.h>
#include <linux/can.h>
#include <linux/can/raw.h>

class CanManage{
public:
    CanManage(std::queue<std::vector<uint8_t>>& dataQueue,std::mutex& queueMutex,std::condition_variable& queueCondVar,
        std::atomic<bool>& shouldExit,std::atomic<bool>& shouldCreatNewFile);
    ~CanManage();
    static void set_collect(int sock,bool flag);
    void start();
private:
    static void insert_sort_frame(std::vector<struct can_frame>& frame_buffer,struct can_frame);
    static void process(CanManage *m);
    int init();
    std::queue<std::vector<uint8_t>>& dataQueue_;
    std::mutex& queueMutex_;                     // 队列互斥锁
    std::condition_variable& queueCondVar_;      // 条件变量
    std::atomic<bool>& shouldExit_;
    std::atomic<bool>& shouldCreatNewFile_;
    std::shared_ptr<std::thread> ptrThread_;
    int s_;
};
#endif