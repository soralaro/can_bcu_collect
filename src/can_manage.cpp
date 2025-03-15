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
    std::atomic<bool>& shouldExit, std::atomic<bool>& shouldCloseFile):dataQueue_(dataQueue),queueMutex_(queueMutex),
    queueCondVar_(queueCondVar), shouldExit_(shouldExit),shouldCloseFile_(shouldCloseFile){
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
    
    int recv_buffer_size = 10*1024 * 1024; // 设置为1MB
 // 设置接收缓冲区大小
    if (setsockopt(s_, SOL_SOCKET, SO_RCVBUF, &recv_buffer_size, sizeof(recv_buffer_size))){ 
        perror("setsockopt failed");
        close(s_);
        return -1;
    }

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
void CanManage::insert_sort_frame(std::vector<struct can_frame>& frame_buffer,struct can_frame frame){
    u_int32_t size=frame_buffer.size();
    if(size==0){
        frame_buffer.push_back(frame);
    }else {
        u_int32_t can_id=frame.can_id;
        auto it=frame_buffer.begin();
        for(u_int32_t i=0;i<size;i++){
            if(it->can_id > can_id){
                if(it->can_id-can_id >32){ 
                    it++;
                }else {
                    frame_buffer.insert(it,frame);
                    break;
                }
            }else {
                if(can_id-it->can_id>32){
                    frame_buffer.insert(it,frame);
                    break;
                }else{
                    it++;
                }
            }
        }
        if(it==frame_buffer.end())
            frame_buffer.push_back(frame);
    }
}

void PacketToWrite::reset(){
    first_=true;
    can_id_save_=0;
    rcv_len_=0;
    rcv_num_=0;
    packet_len_=0;
    packet_.resize(0);
}
PacketToWrite::PacketToWrite(std::vector<struct can_frame>& frame_buffer):frame_buffer_(frame_buffer){
    first_=true;
    can_id_save_=0;
    rcv_len_=0;
    rcv_num_=0;
    packet_len_=0;
}

PacketToWrite::~PacketToWrite(){
}

bool PacketToWrite::proc(CanManage *m,bool end){
    auto frame=frame_buffer_[0];
    frame_buffer_.erase(frame_buffer_.begin());
    u_int32_t can_id=frame.can_id;
    bool error=false;
    if(first_){
        can_id_save_=can_id;
        first_=false;
    }else{
        if(can_id == 0x00){
            if(can_id_save_!=0xff)
                error=true;
        }else if(can_id!=(can_id_save_+1)){
            error=true;    
        }
    }
    if(error){
        if(!end){
            LOG(FATAL)<<"erro can lost! can_id "<<can_id<<",can_id_save "<<can_id_save_ << ",rcv "<<rcv_num_;
            return false;
        }
        else{
            LOG(ERROR)<<"erro, can lost at the end! can_id "<<can_id<<",can_id_save "<<can_id_save_ << ",rcv "<<rcv_num_;
            return false;
        }
    }
    can_id_save_=can_id;
    rcv_num_++;
    if(packet_len_==0){
        BCU_DATA_HEAD bcuDataHead;
        memcpy(&bcuDataHead,frame.data,8);
        bcuDataHead.start_flag=ntohs(bcuDataHead.start_flag);
        if(bcuDataHead.start_flag==FRAM_START_FLAG){
            bcuDataHead.data_len=ntohs(bcuDataHead.data_len);
            bcuDataHead.tick=ntohl(bcuDataHead.tick);
            printf("num %u,ID: %x,len %u,tick %u \n",rcv_num_,frame.can_id,bcuDataHead.data_len,bcuDataHead.tick);
            if(bcuDataHead.data_len>0){
                packet_len_=(bcuDataHead.data_len+7)/8*8+8;
                packet_.resize(packet_len_);
                memcpy(&packet_[0],frame.data,8);
                rcv_len_=8;
            }
        }else {
            //printf(" 2ID: %x\n",frame.can_id);
        }
    }else{
        //printf(" 3ID: %x\n",frame.can_id);
        memcpy(&packet_[rcv_len_],frame.data,8);
        rcv_len_+=8;

        if(rcv_len_>=packet_len_){
            {
                std::lock_guard<std::mutex> lock(m->queueMutex_);
                m->dataQueue_.push(packet_);
            }
            m->queueCondVar_.notify_one();
            rcv_len_=0;
            packet_len_=0;
        }
    }
    return true;
}
void CanManage::process(CanManage *m){
    if(0!=m->init()){
        LOG(ERROR)<<"Fail to initialize Can socket!";
        return;
    }
    set_collect(m->s_,START);
    std::vector<struct can_frame> frame_buffer;
    std::shared_ptr<PacketToWrite>  packetToWrite=std::make_shared<PacketToWrite>(frame_buffer);
    u_int32_t time_out_times=0;
    while (!m->shouldExit_) {
        struct can_frame frame;
    	// 接收CAN帧
        if (read(m->s_, &frame, sizeof(struct can_frame)) < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                if(time_out_times==0){
                    while(frame_buffer.size()>0){
                        if(!packetToWrite->proc(m,true))
                            frame_buffer.resize(0);
                    }
                    packetToWrite->reset();
                }
                if(time_out_times==1){
                    m->shouldCloseFile_.store(true);
                    m->queueCondVar_.notify_one();
                }
                // 超时，没有数据可读
                printf("Can timeout %u s, no data available\n",time_out_times*3);
                time_out_times ++;
                set_collect(m->s_,START);
		        continue;
            } else {
                LOG(FATAL)<<"Read Can failly.";
                return ;
            }
    	}
		time_out_times=0;	
        u_int32_t can_id=frame.can_id;
	    can_id=(can_id>>16)&0x00ff;
        frame.can_id=can_id;
        insert_sort_frame(frame_buffer,frame);
        if(frame_buffer.size()<32){
            continue;
        }
        packetToWrite->proc(m);
    }
    set_collect(m->s_,STOP);
    // 关闭Socket
    close(m->s_);
}
void CanManage::start(){
    ptrThread_=std::make_shared<std::thread>(CanManage::process,this);
}
