#include <iostream>
#include <cstring>
#include <unistd.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/can.h>
#include <linux/can/raw.h>
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


// 全局标志，用于指示程序是否应该退出
std::atomic<bool> shouldExit(false);

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
void set_collect(int sock,bool flag){
    struct can_frame frame;
    frame.can_id = 0x89000171; // CAN ID
    frame.can_dlc = 8;    
    frame.data[0] = 0x00;
    frame.data[1] = 0x03;    //collect command
    frame.data[2] = flag;    //START or STOP
    frame.data[3] = 0x00;
    frame.data[4] = 0x00;
    frame.data[5] = 0x00;
    frame.data[6] = 0x00;
    frame.data[7] = 0x00;
    if (write(sock, &frame, sizeof(struct can_frame)) != sizeof(struct can_frame)) {
        perror("Write failed");
        return 1;
    }
}
std::queue<std::vector<uint8_t>> dataQueue; // 数据队列
std::mutex queueMutex;                     // 队列互斥锁
std::condition_variable queueCondVar;      // 条件变量


int main() {
    int s;
    struct sockaddr_can addr;
    struct ifreq ifr;
    if ((s = socket(PF_CAN, SOCK_RAW, CAN_RAW)) < 0) {
        perror("Socket creation failed");
        return 1;
    }

    std::strcpy(ifr.ifr_name, "can0");
    ioctl(s, SIOCGIFINDEX, &ifr);

    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;
    if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("Bind failed");
        return 1;
    }
    set_collect(s,START);
    std::shared_ptr<FileManage> ptrFileManage=std::make_shared<FileManage>(dataQueue,queueMutex,queueCondVar,shouldExit);
    ptrFileManage->start();
     // 启动输入监听线程
    std::thread listenerThread(inputListener);
    u_int16_t packet_len=0;
    u_int16_t rcv_len=0;
    std::vector<uint8_t> packet;
    while (!shouldExit) {
        struct can_frame frame;
    	// 接收CAN帧
    	if (read(s, &frame, sizeof(struct can_frame)) < 0) {
        	perror("Read failed");
        	return 1;
    	}
        if(packet_len==0){
            BCU_DATA_HEAD bcuDataHead;
            memcpy(&bcuDataHead,frame.data,8);
            bcuDataHead.start_flag=ntohs(bcuDataHead.start_flag);
            if(bcuDataHead.start_flag==FRAM_START_FLAG){
                bcuDataHead.data_len=ntohs(bcuDataHead.data_len);
                bcuDataHead.tick=ntohl(bcuDataHead.tick);
    	        std::cout << "ID: " << std::hex << frame.can_id << std::endl;
                printf("len %u,tick %u \n",bcuDataHead.data_len,bcuDataHead.tick);
                packet_len=(bcuDataHead.data_len+7)/8*8+8;
                packet.resize(packet_len);
                memcpy(&packet[0],frame.data,8);
                rcv_len=8;
            }
        }else{
            memcpy(&packet[rcv_len],frame.data,8);
            rcv_len+=8;

            if(rcv_len>=packet_len){
                {
                    std::lock_guard<std::mutex> lock(queueMutex);
                    dataQueue.push(packet);
                }
                queueCondVar.notify_one();
                rcv_len=0;
                packet_len=0;
            }
        }
    }
    set_collect(s,STOP);
    // 关闭Socket
    close(s);
    // 等待输入监听线程结束
    queueCondVar.notify_all();
    listenerThread.join();
    return 0;
}
