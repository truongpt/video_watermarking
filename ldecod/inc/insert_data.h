
/*!
*************************************************************************************
* \file insert_data.h
*
* \brief
*    Header file module for inserting/extra watermark data to bit stream
*
* \author
*    truongptk30a3@gmail.com
*
*************************************************************************************
*/
/* #define TEST */

#include <stdio.h>

#ifndef TEST
#include "global.h"
#else
typedef struct
{
  short mv_x;
  short mv_y;
} MotionVector;
#endif

typedef struct
{
  MotionVector mv;
  int wm_data;
}WmElem;

  
void watermark_open(char *file_name, unsigned char thres_value, unsigned char embed_mode);
void watermark_close(void);
void watermark_log_data(const MotionVector mvd);
int  is_watermark_insert(unsigned char refer_list);

