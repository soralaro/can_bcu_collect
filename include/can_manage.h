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
        std::atomic<bool>& shouldExit,std::atomic<bool>& shouldCloseFile);
    ~CanManage();
    static void set_collect(int sock,bool flag);
    void start();

private:
    static void insert_sort_frame(std::vector<struct can_frame>& frame_buffer,struct can_frame);
    static void process(CanManage *m);
    int init();
    std::shared_ptr<std::thread> ptrThread_;
    int s_;
public:
    std::queue<std::vector<uint8_t>>& dataQueue_;
    std::mutex& queueMutex_;                     // 队列互斥锁
    std::condition_variable& queueCondVar_;      // 条件变量
    std::atomic<bool>& shouldExit_;
    std::atomic<bool>& shouldCloseFile_;
};
class PacketToWrite
{
private:
    std::vector<struct can_frame>& frame_buffer_;
    bool first_;
    u_int16_t rcv_len_=0;
    std::vector<uint8_t> packet_;
    u_int32_t can_id_save_=0;
    u_int32_t rcv_num_=0;
    u_int32_t packet_len_=0;
    /* data */
public:
    PacketToWrite(std::vector<struct can_frame>& frame_buffer);
    ~PacketToWrite();
    bool proc(CanManage *m,bool end=false);
    void reset();
};
#endif