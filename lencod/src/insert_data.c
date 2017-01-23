
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
/* #define TEST */
#define WM_PRINT(...)
#define WM_ERROR printf


FILE *watermark_file;
unsigned char th_v = 0;
unsigned char mode = 0; //0: no effect, 1: forward mv, 2 backward, 3: bi-predict, 4: no embed


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
  WM_PRINT("wm file %s, thres value %d mode %d\n",file_name,thres_value,embed_mode);
  watermark_file = fopen(file_name, "r");
  th_v = thres_value;
  mode = embed_mode;
}

void watermark_close(void)
{
  if (getc(watermark_file) != EOF) {
    WM_ERROR("Existence data, which is not inserted,yet\n");
  }
  fclose(watermark_file);
}

int is_watermark_insert(unsigned char refer_list)
{
  return (mode & (refer_list + 1));
}

int watermark_mv_embed(MotionVector *mvd, int is_move_pos, int get_next)
{
  int i = 0, wm_data = 0, is_insert_data = 0;
    
  if ( (mvd->mv_x * mvd->mv_x + mvd->mv_y * mvd->mv_y) <= th_v ) {
    if (0 != watermark_read_data(&wm_data, is_move_pos, get_next))
    {
      WM_ERROR("%s %d Input data error\n",__func__,__LINE__);  
      return;
    };

    WM_PRINT("wm_data %03d\n",wm_data);
    for (i = 0; i < sizeof(wm_table)/sizeof(WmElem); i++) {
      if (wm_table[i].wm_data == wm_data) {
	is_insert_data = 1;
	break;
      } 
    }
    
    if (0 == is_insert_data) {
      WM_ERROR("Watermark inputted data is not mapping with data in Matrix table\n");
      return;
    }
    
    //Modified motion vector
    mvd->mv_x = wm_table[i].mv.mv_x;
    mvd->mv_y = wm_table[i].mv.mv_y;
    WM_PRINT("inserted vector mv_x %d mvy %d\n",mvd->mv_x,mvd->mv_y);
    return 1;
  } else {
    return 0;
  }
}

void watermark_get_data(const MotionVector mvd)
{

}

int watermark_read_data(int *r_data, int is_move_pos, int get_next)
{
  int temp_data = 0, cnt = 0, temp_gn = get_next;
  char data_embed = 0;
  long cur_pos;

  cur_pos = ftell(watermark_file);

  //Getting wartermark data from file.
  while((data_embed = getc(watermark_file)) != EOF) {
    if (data_embed == ' '){
  	WM_PRINT("space ....\n");
  	*r_data = 999; //special tab
  	if (0 == cnt) {
	  if (!temp_gn) {
	    cnt = 3;
	    temp_data = 999;
	    break;
	  } else {
	    temp_data = 0;
	    temp_gn = 0;
	  }
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
      if (temp_gn) {
	temp_data = 0;
	temp_gn = 0;
	cnt = 0;
      } else {
	break;
      }
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

  // keep position
  if (!is_move_pos) {
    fseek(watermark_file, cur_pos, SEEK_SET);
  }
  return 0;
}

#ifdef TEST
main()
{
  MotionVector insert_mv;
  insert_mv.mv_x = 1;
  insert_mv.mv_y = 1;

  watermark_open("wm_file_enc.txt",2,1);

  watermark_mv_embed(&insert_mv,1,1);
  printf("mvx %d mvy %d\n",insert_mv.mv_x,insert_mv.mv_y);

  watermark_mv_embed(&insert_mv,1,0);
  printf("mvx %d mvy %d\n",insert_mv.mv_x,insert_mv.mv_y);

  watermark_mv_embed(&insert_mv,1,0);
  printf("mvx %d mvy %d\n",insert_mv.mv_x,insert_mv.mv_y);

  watermark_close();
}
#endif
