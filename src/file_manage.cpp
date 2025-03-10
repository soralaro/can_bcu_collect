#include"file_manage.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <sys/stat.h> // 用于mkdir和stat
#include <cstring>    // 用于strerror
#include <cerrno>     // 用于errno
#include <iostream>
#include "check_del_file.h"
FileManage::FileManage(std::queue<std::vector<uint8_t>>& dataQueue,std::mutex& queueMutex,std::condition_variable& queueCondVar,
    std::atomic<bool>& shouldExit):dataQueue_(dataQueue),queueMutex_(queueMutex),queueCondVar_(queueCondVar),
    shouldExit_(shouldExit){
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
std::ofstream  FileManage::creatNewFile(){
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
    }
    return outFile; 
}
void FileManage::fileWriteThread(FileManage *manage){
    auto outFile=manage->creatNewFile(); 
    if(!outFile){
        std::cerr << "Error  fileWriteThread opening file." << std::endl;
    }
    CheckDelFile checkDelFile(manage->getMaxFileNum());
    while (!manage->shouldExit_) {
        std::unique_lock<std::mutex> lock(manage->queueMutex_);

        // 等待队列中有数据
        manage->queueCondVar_.wait(lock, [&] { return !manage->dataQueue_.empty() || manage->shouldExit_; });

        // 将队列中的数据写入文件
        while (!manage->dataQueue_.empty()) {
            auto data = manage->dataQueue_.front();
            outFile.write(reinterpret_cast<char*>(data.data()), data.size());
            manage->file_size_+=data.size();
            manage->dataQueue_.pop();
        }
        lock.unlock();
        if(manage->file_size_>manage->file_size_max_){
            outFile.close();
            manage->file_size_=0;
            outFile=manage->creatNewFile();
            if(!outFile){
                std::cerr << "Error  fileWriteThread opening file." << std::endl;
                return;
            }
            checkDelFile.process();
        }
    }
    outFile.close();
}
void FileManage::start()
{
    ptrThread_=std::make_shared<std::thread>(FileManage::fileWriteThread,this);
}
u_int32_t FileManage::getMaxFileNum(){
    return max_file_num_;
}