
/*!
*************************************************************************************
* \file insert_data.c
*
* \brief
*    Module for inserting/extra watermark data to bit stream
*
* \author
*    truongptk30a3@gmail.com
*
*************************************************************************************
*/
#include "insert_data.h"

#define WM_PRINT(...)
#define WM_ERROR printf


FILE *watermark_file;
unsigned char th_v = 0;
unsigned char mode = 0; //0: no effect, 1: forward mv, 2 backward, 3: bi-predict, 4: no embed
unsigned char is_space_or_end = 0; // 1: space,  2: end 


WmElem wm_table[] =
 {
   {{-1, 0},0},
   {{-1, 1},1},
   {{ 0, 1},10},
   {{ 1, 1},11},
   {{ 1, 0},100},
   {{ 1,-1},101},
   {{ 0,-1},110},
   {{-1,-1},111},
   {{ 0, 0},999},   
 };

void watermark_open(char *file_name, unsigned char thres_value, unsigned char embed_mode)
{
  WM_PRINT("wm file %s, thres value %d embed_mode %d\n",file_name,thres_value,embed_mode);
  watermark_file = fopen(file_name, "w");
  th_v = thres_value;
  mode = embed_mode;  
}

void watermark_close(void)
{
  fclose(watermark_file);
}

int is_watermark_insert(unsigned char refer_list)
{
  return (mode & (refer_list + 1));
}

void watermark_log_data(const MotionVector mvd)
{
  int i = 0, is_has_data = 0;
  if ((mvd.mv_x * mvd.mv_x + mvd.mv_y * mvd.mv_y) > th_v) {
    return;
  }
  //find index of motion vector in maxtrix table
  for (i = 0; i < sizeof(wm_table)/sizeof(WmElem); i++) {
    if (wm_table[i].mv.mv_x == mvd.mv_x && wm_table[i].mv.mv_y == mvd.mv_y){
      is_has_data = 1;
      break;
    }
  }
  //check existence valid data
  if (0 == is_has_data) {
    WM_ERROR("Not found valid data\n");
    return;
  }
  WM_PRINT("Watermark data: %03d\n",wm_table[i].wm_data);

  if (999 == wm_table[i].wm_data && 0 == is_space_or_end) {
    is_space_or_end = 1;
    // if case special tab, it will be decided at next water data
    return;
  } else if (999 == wm_table[i].wm_data && 1 == is_space_or_end) {
    is_space_or_end = 2;
    //end of data from here
    return;
  } else if (999 != wm_table[i].wm_data && 1 == is_space_or_end) {
    fprintf(watermark_file,"%s\n"," ");
    is_space_or_end = 0;
  } else if (2 == is_space_or_end) {
      //end of data
      return;
  }
  //log data to file
  fprintf(watermark_file,"%03d\n",wm_table[i].wm_data);
}

int watermark_read_data(int *r_data)
{
  int temp_data = 0, cnt = 0;
  char data_embed = 0;

  //Getting wartermark data from file.
  while((data_embed = getc(watermark_file)) != EOF) {
    if (data_embed == ' '){
      WM_PRINT("space ....\n");
      *r_data = 999; //special tab
      if (0 == cnt) {
	return 0;
      } else {
        WM_ERROR("Input data error cnt = %d line %d\n",cnt,__LINE__);
	return -1;
      }
    } else if (10 == data_embed /*Line Feed*/){
      //ignore
    } else {
      temp_data = temp_data + (data_embed - '0') * ( (cnt == 0) ? 100 : ((cnt == 1) ? 10 : 1) );
      cnt++;
    }
    //Each data is signalled by 3 charater
    if (3 == cnt){
      break;
    } 
  }

  if (0 == cnt) {
    *r_data = 999; //empty data
  } else if (3 != cnt){
    WM_ERROR("Input data error cnt = %d line %d\n",cnt,__LINE__);
    return -1;
  } else {
    *r_data = temp_data;
    WM_PRINT("read data %d\n",*r_data);
  }
  return 0;
}

#ifdef TEST
main()
{
  MotionVector insert_mv;
  insert_mv.mv_x = 1;
  insert_mv.mv_y = 1;

  watermark_open("wm_file_dec.txt",2);
  
  insert_mv.mv_x = 1;
  insert_mv.mv_y = 1;
  watermark_log_data(insert_mv);

  insert_mv.mv_x = 0;
  insert_mv.mv_y = 1;
  watermark_log_data(insert_mv);

  insert_mv.mv_x = 0;
  insert_mv.mv_y = 0;
  watermark_log_data(insert_mv);

  insert_mv.mv_x = 1;
  insert_mv.mv_y = -1;
  watermark_log_data(insert_mv);

  insert_mv.mv_x = 1;
  insert_mv.mv_y = 0;
  watermark_log_data(insert_mv);

  insert_mv.mv_x = 0;
  insert_mv.mv_y = 0;
  watermark_log_data(insert_mv);

  insert_mv.mv_x = 0;
  insert_mv.mv_y = 0;
  watermark_log_data(insert_mv);

  insert_mv.mv_x = 0;
  insert_mv.mv_y = 0;
  watermark_log_data(insert_mv);
  
}
#endif
