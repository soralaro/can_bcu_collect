#include <iostream>
#include <cstring>
#include <unistd.h>
#include <termios.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <thread>
#include <atomic>
#include <chrono>
#include <queue>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <memory>
#include "collect_bcu_data.h"
#include "file_manage.h"
#include "can_manage.h"


// 全局标志，用于指示程序是否应该退出
std::atomic<bool> shouldExit(false);
std::atomic<bool> shouldCreatNewFile(false);
std::queue<std::vector<uint8_t>> dataQueue; // 数据队列
std::mutex queueMutex;                     // 队列互斥锁
std::condition_variable queueCondVar;      // 条件变量

// 监听用户输入的线程函数
void inputListener() {
    char input;
    std::cout<<"inpulisten thread start "<<std::endl;
    while (!shouldExit) {
        std::cin >> input;
        if (input == 'c' || input == 'C') {
            shouldExit = true;
            break;
        }
    }
}

int main() {
    std::shared_ptr<FileManage> ptrFileManage=std::make_shared<FileManage>(dataQueue,queueMutex,queueCondVar,shouldExit,shouldCreatNewFile);
    std::shared_ptr<CanManage> ptrCanManage=std::make_shared<CanManage>(dataQueue,queueMutex,queueCondVar,shouldExit,shouldCreatNewFile);
    ptrCanManage->start();
    ptrFileManage->start();
     // 启动输入监听线程
    std::thread listenerThread(inputListener);

    // 等待输入监听线程结束
    listenerThread.join();
    queueCondVar.notify_all();
    return 0;
}
