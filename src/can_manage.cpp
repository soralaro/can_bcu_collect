#include <iomanip>
#include <sys/stat.h> // 用于mkdir和stat
#include <cstring>    // 用于strerror
#include <cerrno>     // 用于errno
#include <iostream>
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
#include "can_manage.h"
#include <glog/logging.h>
#define START 1
#define STOP 0
#define  FRAM_START_FLAG 0xFF55
typedef struct
{
   u_int16_t start_flag;
   u_int16_t data_len;
   u_int32_t tick;
}BCU_DATA_HEAD;
CanManage::CanManage(std::queue<std::vector<uint8_t>>& dataQueue,std::mutex& queueMutex,std::condition_variable& queueCondVar,
    std::atomic<bool>& shouldExit, std::atomic<bool>& shouldCreatNewFile):dataQueue_(dataQueue),queueMutex_(queueMutex),
    queueCondVar_(queueCondVar), shouldExit_(shouldExit),shouldCreatNewFile_(shouldCreatNewFile){
}

CanManage::~CanManage(){
    if(ptrThread_){
       ptrThread_->join(); 
    }
}
int CanManage::init(){
    struct sockaddr_can addr;
    struct ifreq ifr;
    if ((s_ = socket(PF_CAN, SOCK_RAW, CAN_RAW)) < 0) {
        LOG(ERROR)<<"Socket creation failed";
        return -1;
    }

    std::strcpy(ifr.ifr_name, "can0");
    ioctl(s_, SIOCGIFINDEX, &ifr);

    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;
    if (bind(s_, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        LOG(ERROR)<<"Bind failed";
        return -1;
    }
    struct timeval tv;
    tv.tv_sec = 3;  // 1秒超时
    tv.tv_usec = 0;
    setsockopt(s_, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
    return 0;
}
void CanManage::set_collect(int sock,bool flag){
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
        LOG(ERROR)<<"Write Can failed";
    }
}
void CanManage::process(CanManage *m){
    if(0!=m->init()){
        LOG(ERROR)<<"Fail to initialize Can socket!";
        return;
    }
    set_collect(m->s_,START);
    u_int16_t packet_len=0;
    u_int16_t rcv_len=0;
    std::vector<uint8_t> packet;
    while (!m->shouldExit_) {
        struct can_frame frame;
    	// 接收CAN帧
        if (read(m->s_, &frame, sizeof(struct can_frame)) < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // 超时，没有数据可读
                printf("Can timeout, no data available\n");
                set_collect(m->s_,START);
                m->shouldCreatNewFile_.store(true);
            } else {
                LOG(ERROR)<<"Read Can failly.";
                return ;
            }
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
                if(bcuDataHead.data_len>0){
                    packet_len=(bcuDataHead.data_len+7)/8*8+8;
                    packet.resize(packet_len);
                    memcpy(&packet[0],frame.data,8);
                    rcv_len=8;
                }
            }
        }else{
            memcpy(&packet[rcv_len],frame.data,8);
            rcv_len+=8;

            if(rcv_len>=packet_len){
                {
                    std::lock_guard<std::mutex> lock(m->queueMutex_);
                    m->dataQueue_.push(packet);
                }
                m->queueCondVar_.notify_one();
                rcv_len=0;
                packet_len=0;
            }
        }
    }
    set_collect(m->s_,STOP);
    // 关闭Socket
    close(m->s_);
}
void CanManage::start(){
    ptrThread_=std::make_shared<std::thread>(CanManage::process,this);
}
