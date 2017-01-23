
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
#include <stdio.h>

/* #define TEST */
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
int  watermark_mv_embed(MotionVector *mvd, int is_move_pos, int get_next);
void watermark_get_data(const MotionVector mvd);
int  is_watermark_insert(unsigned char refer_list);
