#ifndef PROJECT_COLLECT_BCU_DATA_H
#define PROJECT_COLLECT_BCU_DATA_H
#include<cstdint>
#define START 1
#define STOP 0
#define  FRAM_START_FLAG 0xFF55
typedef struct
{
   u_int16_t start_flag;
   u_int16_t data_len;
   u_int32_t tick;
}BCU_DATA_HEAD;

#endif