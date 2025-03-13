import struct
import csv
import numpy as np
import pdb
import os
from datetime import datetime
 
# 定义文件目录
directory = "bcu_data"
# 输出的CSV文件路径
csv_file_path = 'output.csv' 
# 定义时间范围
start_time = datetime.strptime("2025-03-12_09:59:00", "%Y-%m-%d_%H:%M:%S")
end_time = datetime.strptime("2025-03-12_15:00:28", "%Y-%m-%d_%H:%M:%S")

# 获取目录下的所有文件
files = os.listdir(directory)

# 筛选符合条件的文件
filtered_files = []
for file in files:
    if file.startswith("bcu_data_") and file.endswith(".bin"):
        # 从文件名中提取时间部分
        time_str = file[len("bcu_data_"):-len(".bin")]
        #pdb.set_trace()
        file_time = datetime.strptime(time_str, "%Y-%m-%d_%H:%M:%S")

        # 判断文件时间是否在范围内
        if start_time <= file_time <= end_time:
            filtered_files.append(os.path.join(directory, file))
import re
def extract_time(file_name):
    # 使用正则表达式提取时间信息
    pattern = r'(\d{4}-\d{2}-\d{2}_\d{2}:\d{2}:\d{2})'
    match = re.search(pattern, file_name)
    if match:
        return match.group(1)
    return None

# 按照时间排序
filtered_files.sort(key=extract_time)


ticks = [] 
cell_currents = []
cell_voltage_v_s = []  
num_cells = 0
tick_base = 0
def parse_binary_file(binary_file_path):
     global ticks , cell_currents,cell_voltage_v_s,num_cells,tick_base

     with open(binary_file_path, 'rb') as bin_file:
        # 写入CSV文件的表头
        while True:
            # 读取数据包的开头
            #pdb.set_trace()
            header = bin_file.read(2)
            if not header:
                break  # 文件结束
            
            # 检查数据包开头是否为0xFF55
            if header != b'\xff\x55':
                continue
            
            # 读取包长度（16位）
            packet_length = struct.unpack('>H', bin_file.read(2))[0]
            
            # 读取采集时钟tick（32位）
            tick = struct.unpack('>I', bin_file.read(4))[0]
            if(tick_base == 0 ):
                tick_base = tick
            ticks.append(tick-tick_base) 
            # 计算电芯数量
            num= (packet_length) // 2 -1  # 减去6字节（2字节头 + 2字节长度 + 4字节tick）
            if(num_cells==0):
                num_cells=num 
            elif(num_cells!=num):
                print("number of cells must the same  everytimes")
            cell_current = struct.unpack('<H', bin_file.read(2))[0]
            cell_currents.append(cell_current)
            cell_voltage_v = []
            # 读取电芯电压数据（16位）
            for _ in range(num_cells):
                voltage = struct.unpack('<H', bin_file.read(2))[0]
                cell_voltage_v.append(voltage)
            cell_voltage_v_s.append(cell_voltage_v) 
            # 写入CSV文件

for file in filtered_files:
    print(file)
    parse_binary_file(file)

with open(csv_file_path, 'w', newline='') as csv_file:
    csv_writer = csv.writer(csv_file)
    csv_writer.writerow([f'{i}' for i in range(0, len(ticks))]) 
    csv_writer.writerow([f'{tick}' for tick in ticks]) 
    csv_writer.writerow([f'{current}' for current in cell_currents]) 
    np_mat=np.array(cell_voltage_v_s)
    cell_voltage_v_s=np_mat.T.tolist() #trans mat
    for i in range(0,num_cells):
        csv_writer.writerow([f'{voltage}' for voltage in cell_voltage_v_s[i]]) 

