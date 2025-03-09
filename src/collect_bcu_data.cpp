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
#include <fstream>
#include <sstream>
#include <iomanip>
#include <sys/stat.h> // 用于mkdir和stat
#include <cstring>    // 用于strerror
#include <cerrno>     // 用于errno
#include "collect_bcu_data.h"


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
// 检查文件夹是否存在，如果不存在则创建
bool createDirectoryIfNotExists(const std::string& path) {
    struct stat info;

    // 检查路径是否存在
    if (stat(path.c_str(), &info) != 0) {
        // 路径不存在，尝试创建文件夹
        if (mkdir(path.c_str(), 0755) != 0) {
            // 创建文件夹失败
            std::cerr << "Error creating directory: " << strerror(errno) << std::endl;
            return false;
        }
        std::cout << "Directory created: " << path << std::endl;
        return true;
    } else if (info.st_mode & S_IFDIR) {
        // 路径存在且是一个文件夹
        std::cout << "Directory already exists: " << path << std::endl;
        return true;
    } else {
        // 路径存在但不是文件夹
        std::cerr << "Path exists but is not a directory: " << path << std::endl;
        return false;
    }
}
// 写文件的线程函数
void fileWriteThread() {

    std::string folderPath = "bcu_data";

    if (createDirectoryIfNotExists(folderPath)) {
        std::cout << "Directory is ready: " << folderPath << std::endl;
    } else {
        std::cerr << "Failed to ensure directory exists: " << folderPath << std::endl;
    }
    auto now = std::chrono::system_clock::now();

    // 将时间点转换为time_t（秒级精度）
    std::time_t now_time_t = std::chrono::system_clock::to_time_t(now);

    // 将time_t转换为tm结构（本地时间）
    std::tm now_tm = *std::localtime(&now_time_t);

    // 使用stringstream格式化时间
    std::stringstream ss;
    ss << std::put_time(&now_tm, "%Y-%m-%d_%H:%M:%S");
    std::string filename=folderPath+"/bcu_data_";
    filename+=ss.str()+".bin";
    std::ofstream outFile(filename, std::ios::binary);
    if (!outFile) {
        std::cerr << "Error opening file." << std::endl;
        return;
    }

    while (!shouldExit) {
        std::unique_lock<std::mutex> lock(queueMutex);

        // 等待队列中有数据
        queueCondVar.wait(lock, [] { return !dataQueue.empty() || shouldExit; });

        // 将队列中的数据写入文件
        while (!dataQueue.empty()) {
            auto data = dataQueue.front();
            outFile.write(reinterpret_cast<char*>(data.data()), data.size());
            dataQueue.pop();
        }

        lock.unlock();
    }

    outFile.close();
}
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
     // 启动输入监听线程
    std::thread listenerThread(inputListener);
    // 启动文件写入线程
    std::thread fileThread(fileWriteThread);
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
    fileThread.join();
    return 0;
}
