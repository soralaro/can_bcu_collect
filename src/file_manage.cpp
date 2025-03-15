#include"file_manage.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <sys/stat.h> // 用于mkdir和stat
#include <cstring>    // 用于strerror
#include <cerrno>     // 用于errno
#include <iostream>
#include "check_del_file.h"
#include <glog/logging.h>
#include <jsoncpp/json/json.h> // JsonCpp 头文件
FileManage::FileManage(std::queue<std::vector<uint8_t>>& dataQueue,std::mutex& queueMutex,std::condition_variable& queueCondVar,
    std::atomic<bool>& shouldExit,std::atomic<bool>& shouldCloseFile):dataQueue_(dataQueue),queueMutex_(queueMutex),queueCondVar_(queueCondVar),
    shouldExit_(shouldExit),shouldCloseFile_(shouldCloseFile){
            // 打开 JSON 文件
    std::ifstream config_file("./config.json");
    if (!config_file.is_open()) {
        LOG(ERROR)<< "Failed to open config.json!";
        return;
    }

    // 解析 JSON 文件
    Json::Value config;
    Json::CharReaderBuilder reader;
    std::string errs;
    if (!Json::parseFromStream(reader, config_file, &config, &errs)) {
        LOG(ERROR) << "Failed to parse JSON: ";
        return ;
    }

    // 读取配置项
    unsigned int file_size_max = config["file_size_max"].asUInt();
    unsigned int max_file_num = config["max_file_num"].asUInt();
    std::string save_path = config["save_path"].asString();
    if(file_size_max>0){
        file_size_max_=file_size_max;
    }
    if(max_file_num>0){
        max_file_num_=file_size_max;
    }
    LOG(ERROR)<<"file_size_max "<<file_size_max_<<",max_file_num "<<max_file_num_;
    LOG(ERROR)<<"Save_path "<<save_path_ ;
}

FileManage::~FileManage(){
    if(ptrThread_){
       ptrThread_->join(); 
    }
}
bool FileManage::createDirectoryIfNotExists(const std::string& path){
    struct stat info;

    // 检查路径是否存在
    if (stat(path.c_str(), &info) != 0) {
        // 路径不存在，尝试创建文件夹
        if (mkdir(path.c_str(), 0755) != 0) {
            // 创建文件夹失败
            LOG(ERROR) << "Error creating directory: " << strerror(errno) ;
            return false;
        }
        LOG(ERROR)<< "Directory created: " << path ;
        return true;
    } else if (info.st_mode & S_IFDIR) {
        // 路径存在且是一个文件夹
        LOG(ERROR) << "Directory already exists: " << path;
        return true;
    } else {
        // 路径存在但不是文件夹
        LOG(ERROR) << "Path exists but is not a directory: " << path ;
        return false;
    }
}
std::ofstream  FileManage::creatNewFile(){
    std::string folderPath = save_path_+"bcu_data";

    if (createDirectoryIfNotExists(folderPath)) {
        LOG(ERROR) << "Directory is ready: " << folderPath ;
    } else {
        LOG(ERROR) << "Failed to ensure directory exists: " << folderPath ;
    }
    auto now = std::chrono::system_clock::now();

    // 将时间点转换为time_t（秒级精度）
    std::time_t now_time_t = std::chrono::system_clock::to_time_t(now);

    // 将time_t转换为tm结构（本地时间）
    std::tm now_tm = *std::localtime(&now_time_t);

    // 使用stringstream格式化时间
    std::stringstream ss;
    ss << std::put_time(&now_tm, "%Y-%m-%d_%H:%M:%S");
    current_file_name_=folderPath+"/bcu_data_";
    current_file_name_+=ss.str()+".bin";
    std::ofstream outFile(current_file_name_, std::ios::binary);
    if (!outFile) {
        LOG(ERROR)<< "Error opening file "<<current_file_name_;
    }else{
        LOG(ERROR)<< " opening file "<<current_file_name_;
    }
    return outFile; 
}
void FileManage::fileWriteThread(FileManage *manage){
    std::ofstream outFile;
    CheckDelFile checkDelFile(manage->getMaxFileNum());
    while (!manage->shouldExit_) {
        // 等待队列中有数据
        std::unique_lock<std::mutex> lock(manage->queueMutex_);
        manage->queueCondVar_.wait(lock, [&] { return !manage->dataQueue_.empty() || manage->shouldExit_|| manage->shouldCloseFile_;});
        // 将队列中的数据写入文件
        std::vector<uint8_t> vData;
        while (!manage->dataQueue_.empty()) {
            auto packet = manage->dataQueue_.front();
            vData.insert(vData.end(),packet.begin(),packet.end());
            manage->dataQueue_.pop();
        }
        lock.unlock();
        if(manage->shouldCloseFile_){
            manage->shouldCloseFile_.store(false);
            if(outFile.is_open()){
                outFile.close();
                LOG(ERROR)<<"close file "<<manage->current_file_name_;
            }
        }
        if(vData.size()==0)
            continue;
        if(!outFile.is_open()){
            outFile=manage->creatNewFile();
            if(!outFile.is_open()){
                LOG(ERROR) << "Error  fileWriteThread opening file.";
                return;
            }
            manage->file_size_=0;
        }
        outFile.write(reinterpret_cast<char*>(vData.data()), vData.size());
        manage->file_size_+=vData.size();
        if(manage->file_size_>manage->file_size_max_){
            outFile.close();
            LOG(ERROR)<<"close file "<<manage->current_file_name_;
            checkDelFile.process();
        }
    }
    if(outFile.is_open()){
        outFile.close();
        LOG(ERROR)<<"close file "<<manage->current_file_name_;
    }
}
void FileManage::start()
{
    ptrThread_=std::make_shared<std::thread>(FileManage::fileWriteThread,this);
}
u_int32_t FileManage::getMaxFileNum(){
    return max_file_num_;
}