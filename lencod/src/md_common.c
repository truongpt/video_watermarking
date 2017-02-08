/*!
 ***************************************************************************
 * \file md_common.c
 *
 * \brief
 *    Mode Decision common functions
 *
 * \author
 *    - Alexis Michael Tourapis    <alexismt@ieee.org>
 * \date
 *    04. October 2008
 **************************************************************************
 */

#include <limits.h>
#include "global.h"
#include "image.h"
#include "macroblock.h"
#include "mc_prediction.h"
#include "insert_data.h"

#define RDO_PRINT(...)
#define BSLICE_PRINT(...)
#define P16x8_PRINT(...)
#define P8x16_PRINT(...)
/* #define B16x8_PRINT printf */
#define B16x8_PRINT(...)
#define B8x16_PRINT(...)
/* #define B8x16_PRINT printf */

/*!
 *************************************************************************************
 * \brief
 *    Copy rdo mv info to 16xN picture mv buffer
 *************************************************************************************
 */
static inline void CopyMVBlock16(PicMotionParams **mv_info, MotionVector **rdo_mv, int list, int block_x, int block_y, int start, int end)
{
  int j, i;

  for (j = start; j < end; j++)
  {
    for (i = 0; i < BLOCK_MULTIPLE; i++)
    {
      mv_info[block_y + j][block_x + i].mv[list] = rdo_mv[j][i];
    }
  }
}


/*!
 *************************************************************************************
 * \brief
 *    Reset corresponding 16xN block picture mv buffer
 *************************************************************************************
 */
static inline void ResetMotionBlock16(PicMotionParams **mv_info, int list, int block_x, int block_y, int start, int end)
{
  int j, i;
  for (j = block_y + start; j < block_y + end; j++)
  {
    for (i = block_x; i < block_x + BLOCK_MULTIPLE; i++)
    {
      mv_info[j][i].mv[list] = zero_mv;
      mv_info[j][i].ref_idx[list] = -1;
      mv_info[j][i].ref_pic[list] = NULL;
    }
  }
}

/*!
 *************************************************************************************
 * \brief
 *    Reset corresponding 16xN block picture mv buffer
 *************************************************************************************
 */
static inline void ResetMVBlock16(PicMotionParams **mv_info, int list, int block_x, int block_y, int start, int end)
{
  int j, i;
  for (j = block_y + start; j < block_y + end; j++)
  {
    for (i = block_x; i < block_x + BLOCK_MULTIPLE; i++)
      mv_info[j][i].mv[list] = zero_mv;
  }
}

/*!
 *************************************************************************************
 * \brief
 *    Reset corresponding 16xN block picture ref buffer (intra)
 *************************************************************************************
 */
static inline void ResetRefBlock16(PicMotionParams **mv_info, int list, int block_x, int block_y, int start, int end)
{
  int j, i;
  for (j = block_y + start; j < block_y + end; j++)
  {
    for (i = block_x; i < block_x + BLOCK_MULTIPLE; ++i)
    {
      mv_info[j][i].ref_idx[list] = -1;
      mv_info[j][i].ref_pic[list] = NULL;
    }
  }
}

/*!
 *************************************************************************************
 * \brief
 *    Copy rdo mv info to 8xN picture mv buffer
 *************************************************************************************
 */
static inline void CopyMVBlock8(PicMotionParams **mv_info, MotionVector **rdo_mv, int list, int block_x, int start, int end, int offset)
{
  int j;
  for (j = start; j < end; j++)
  {
    mv_info[j][block_x    ].mv[list] = rdo_mv[j][offset];
    mv_info[j][block_x + 1].mv[list] = rdo_mv[j][offset + 1];
  }
}

/*!
 *************************************************************************************
 * \brief
 *    Reset corresponding 8xN block picture mv buffer
 *************************************************************************************
 */
static inline void ResetMotionBlock8(PicMotionParams **mv_info, int list, int block_x, int start, int end)
{
  int j;

  for (j = start; j < end; j++)
  {
    mv_info[j][block_x    ].mv[list] = zero_mv;
    mv_info[j][block_x    ].ref_idx[list] = -1;
    mv_info[j][block_x    ].ref_pic[list] = NULL;
    mv_info[j][block_x + 1].mv[list] = zero_mv;
    mv_info[j][block_x + 1].ref_idx[list] = -1;
    mv_info[j][block_x + 1].ref_pic[list] = NULL;
  }
}

/*!
 *************************************************************************************
 * \brief
 *    Reset corresponding 8xN block picture mv buffer
 *************************************************************************************
 */
static inline void ResetMVBlock8(PicMotionParams **mv_info, int list, int block_x, int start, int end)
{
  int j;

  for (j = start; j < end; j++)
  {
    mv_info[j][block_x    ].mv[list] = zero_mv;
    mv_info[j][block_x + 1].mv[list] = zero_mv;
  }
}

/*!
 *************************************************************************************
 * \brief
 *    Reset corresponding 8xN block picture ref buffer (intra)
 *************************************************************************************
 */
static inline void ResetRefBlock8(PicMotionParams **mv_info, int list, int block_x, int start, int end)
{
  int j;
  for (j = start; j < end; j++)
  {
    mv_info[j][block_x    ].ref_idx[list] = -1;
    mv_info[j][block_x    ].ref_pic[list] = NULL;
    mv_info[j][block_x + 1].ref_idx[list] = -1;
    mv_info[j][block_x + 1].ref_pic[list] = NULL;
  }
}

/*!
 *************************************************************************************
 * \brief
 *    Sets motion vectors for a macroblock in a P Slice
 *************************************************************************************
 */
void SetMotionVectorsMBPSlice (Macroblock* currMB, int is_modified_mv)
{
  Slice *currSlice = currMB->p_Slice;
  VideoParameters *p_Vid = currMB->p_Vid;
  PicMotionParams **mv_info = p_Vid->enc_picture->mv_info;

  RD_DATA *rdopt = currSlice->rddata;
  MotionVector ****all_mv  = currSlice->all_mv[LIST_0];
  int  l0_ref, mode8;

  int j,i;
  PixelPos       block[4];
  int step_v = 4, step_h =4, next_wm_data = 0;
  MotionVector predMV;
  MotionVector mvd;
	
  if (currSlice->mb_aff_frame_flag || (currSlice->UseRDOQuant && currSlice->RDOQ_QP_Num > 1))
  {
    memcpy(&rdopt->all_mv[LIST_0][0][0][0][0], &all_mv[0][0][0][0], p_Vid->max_num_references * 9 * MB_BLOCK_PARTITIONS * sizeof(MotionVector));
  }

  if (currMB->mb_type == PSKIP) // Skip mode
  {
    CopyMVBlock16(mv_info, all_mv[0][PSKIP], LIST_0, currMB->block_x, currMB->block_y, 0, 4);
  }
  else if (currMB->mb_type == P16x16) // 16x16
  {
    l0_ref = mv_info[currMB->block_y][currMB->block_x].ref_idx[LIST_0];  
    if (1 == is_modified_mv && (is_watermark_insert(LIST_0))) {
      
      // get neighbor mb for calculating MVP
      get_neighbors(currMB, block, 0, 0, step_h<<2);
      currMB->GetMVPredictor (currMB, block, &predMV, (short) l0_ref, p_Vid->enc_picture->mv_info, LIST_0, 0, 0, step_h<<2, step_v<<2);
      //calculate MVD
      mvd.mv_x = all_mv[l0_ref][P16x16][0][0].mv_x - predMV.mv_x;
      mvd.mv_y = all_mv[l0_ref][P16x16][0][0].mv_y - predMV.mv_y;

      //modified MVD
      RDO_PRINT("original mvd.mv_x %d mvd.mv_y %d\n",mvd.mv_x,mvd.mv_y);
      watermark_mv_embed(&mvd,0,0);
      RDO_PRINT("modified mvd.mv_x %d mvd.mv_y %d\n",mvd.mv_x,mvd.mv_y);
      
      for (j = 0; j < 4; j++)
      {
	for (i = 0; i < BLOCK_MULTIPLE; i++)
	{
          all_mv[l0_ref][P16x16][j][i].mv_x = predMV.mv_x + mvd.mv_x;
          all_mv[l0_ref][P16x16][j][i].mv_y = predMV.mv_y + mvd.mv_y;
         }
      }
      
    }
    
    CopyMVBlock16(mv_info, all_mv[l0_ref][P16x16], LIST_0, currMB->block_x, currMB->block_y, 0, 4);
  }
  else if (currMB->mb_type == P16x8) // 16x8
  {
    next_wm_data = 0;
    step_h = 4;
    step_v = 2;
    //truongpt
    l0_ref = mv_info[currMB->block_y][currMB->block_x].ref_idx[LIST_0];
		
    if (1 == is_modified_mv && (is_watermark_insert(LIST_0))) {
      get_neighbors(currMB, block, 0, 0, step_h<<2);
      currMB->GetMVPredictor (currMB, block, &predMV, (short) l0_ref, p_Vid->enc_picture->mv_info, LIST_0, 0, 0, step_h<<2, step_v<<2);
      //calculate MVD
      mvd.mv_x = all_mv[l0_ref][P16x8][0][0].mv_x - predMV.mv_x;
      mvd.mv_y = all_mv[l0_ref][P16x8][0][0].mv_y - predMV.mv_y;
      P16x8_PRINT("%d predmv mvd.mv_x %d mvd.mv_y %d\n",__LINE__,predMV.mv_x,predMV.mv_y);
			
      //modified MVD
      P16x8_PRINT("original mvd.mv_x %d mvd.mv_y %d\n",mvd.mv_x,mvd.mv_y);
      next_wm_data = watermark_mv_embed(&mvd,0,0);
      P16x8_PRINT("modified mvd.mv_x %d mvd.mv_y %d\n",mvd.mv_x,mvd.mv_y);

      for (j = 0; j < 2; j++)
      {
        for (i = 0; i < BLOCK_MULTIPLE; i++)
        {
          all_mv[l0_ref][P16x8][j][i].mv_x = predMV.mv_x + mvd.mv_x;
          all_mv[l0_ref][P16x8][j][i].mv_y = predMV.mv_y + mvd.mv_y;
          
        }
      }
    }
		
    CopyMVBlock16(mv_info, all_mv[l0_ref][P16x8], LIST_0, currMB->block_x, currMB->block_y, 0, 2);

    //truongpt
    l0_ref = mv_info[currMB->block_y + 2][currMB->block_x].ref_idx[LIST_0];
    if (1 == is_modified_mv && (is_watermark_insert(LIST_0))) {
      get_neighbors(currMB, block, 0, 8, step_h<<2);
      currMB->GetMVPredictor (currMB, block, &predMV, (short) l0_ref, p_Vid->enc_picture->mv_info, LIST_0, 0, 8, step_h<<2, step_v<<2);
      P16x8_PRINT("%d predmv mvd.mv_x %d mvd.mv_y %d\n",__LINE__,predMV.mv_x,predMV.mv_y);
      //calculate MVD
      mvd.mv_x = all_mv[l0_ref][P16x8][2][0].mv_x - predMV.mv_x;
      mvd.mv_y = all_mv[l0_ref][P16x8][2][0].mv_y - predMV.mv_y;

      //modified MVD
      P16x8_PRINT("original mvd.mv_x %d mvd.mv_y %d\n",mvd.mv_x,mvd.mv_y);
      watermark_mv_embed(&mvd,0,next_wm_data);
      P16x8_PRINT("modified mvd.mv_x %d mvd.mv_y %d\n",mvd.mv_x,mvd.mv_y);
			
      for (j = 2; j < 4; j++)
      {
        for (i = 0; i < BLOCK_MULTIPLE; i++)
        {
          all_mv[l0_ref][P16x8][j][i].mv_x = predMV.mv_x + mvd.mv_x;
          all_mv[l0_ref][P16x8][j][i].mv_y = predMV.mv_y + mvd.mv_y;
        }
      }
    }
    CopyMVBlock16(mv_info, all_mv[l0_ref][P16x8], LIST_0, currMB->block_x, currMB->block_y, 2, 4);
  }
  else if (currMB->mb_type == P8x16) // 8x16
  {
    next_wm_data = 0;
    step_h = 2;
    step_v = 4;
    
    l0_ref = mv_info[currMB->block_y][currMB->block_x].ref_idx[LIST_0];
    if (1 == is_modified_mv && (is_watermark_insert(LIST_0)))
    {
      get_neighbors(currMB, block, 0, 0, step_h<<2);
      currMB->GetMVPredictor (currMB, block, &predMV, (short) l0_ref, p_Vid->enc_picture->mv_info, LIST_0, 0, 0, step_h<<2, step_v<<2);
      //calculate MVD
      mvd.mv_x = all_mv[l0_ref][P8x16][0][0].mv_x - predMV.mv_x;
      mvd.mv_y = all_mv[l0_ref][P8x16][0][0].mv_y - predMV.mv_y;
      P8x16_PRINT("%d predmv mvd.mv_x %d mvd.mv_y %d\n",__LINE__,predMV.mv_x,predMV.mv_y);
			
      //modified MVD
      P8x16_PRINT("original mvd.mv_x %d mvd.mv_y %d\n",mvd.mv_x,mvd.mv_y);
      next_wm_data = watermark_mv_embed(&mvd,0,0);
      P8x16_PRINT("modified mvd.mv_x %d mvd.mv_y %d\n",mvd.mv_x,mvd.mv_y);
        
      for (j = 0; j < 4; j++)
      {
        for (i = 0; i < 2; i++)
        {
          all_mv[l0_ref][P8x16][j][i].mv_x = predMV.mv_x + mvd.mv_x;
          all_mv[l0_ref][P8x16][j][i].mv_y = predMV.mv_y + mvd.mv_y;
        
        }
      }
    }		
    CopyMVBlock8(&mv_info[currMB->block_y], all_mv[l0_ref][P8x16], LIST_0, currMB->block_x    , 0, 4, 0);
    
    l0_ref = mv_info[currMB->block_y][currMB->block_x + 2].ref_idx[LIST_0];
    if (1 == is_modified_mv && (is_watermark_insert(LIST_0)))
    {
      get_neighbors(currMB, block, 8, 0, step_h<<2);
      currMB->GetMVPredictor (currMB, block, &predMV, (short) l0_ref, p_Vid->enc_picture->mv_info, LIST_0, 8, 0, step_h<<2, step_v<<2);
      P8x16_PRINT("%d predmv mvd.mv_x %d mvd.mv_y %d\n",__LINE__,predMV.mv_x,predMV.mv_y);
      //calculate MVD
      mvd.mv_x = all_mv[l0_ref][P8x16][0][2].mv_x - predMV.mv_x;
      mvd.mv_y = all_mv[l0_ref][P8x16][0][2].mv_y - predMV.mv_y;

      //modified MVD
      P8x16_PRINT("original mvd.mv_x %d mvd.mv_y %d\n",mvd.mv_x,mvd.mv_y);
      watermark_mv_embed(&mvd,0,next_wm_data);
      P8x16_PRINT("modified mvd.mv_x %d mvd.mv_y %d\n",mvd.mv_x,mvd.mv_y);
      for (j = 0; j < 4; j++)
      {
        for (i = 2; i < 4; i++)
        {
          all_mv[l0_ref][P8x16][j][i].mv_x = predMV.mv_x + mvd.mv_x;
          all_mv[l0_ref][P8x16][j][i].mv_y = predMV.mv_y + mvd.mv_y;
        
        }
      }
    }
    CopyMVBlock8(&mv_info[currMB->block_y], all_mv[l0_ref][P8x16], LIST_0, currMB->block_x + 2, 0, 4, 2);
  } 
  else if (currMB->mb_type == P8x8) // 8x8
  {
    mode8 = currMB->b8x8[0].mode;
    l0_ref = mv_info[currMB->block_y    ][currMB->block_x    ].ref_idx[LIST_0];
    CopyMVBlock8(&mv_info[currMB->block_y], all_mv[l0_ref][mode8], LIST_0, currMB->block_x    , 0, 2, 0);
    
    mode8 = currMB->b8x8[1].mode;
    l0_ref = mv_info[currMB->block_y    ][currMB->block_x + 2].ref_idx[LIST_0];
    CopyMVBlock8(&mv_info[currMB->block_y], all_mv[l0_ref][mode8], LIST_0, currMB->block_x + 2, 0, 2, 2);
    
    mode8 = currMB->b8x8[2].mode;
    l0_ref = mv_info[currMB->block_y + 2][currMB->block_x    ].ref_idx[LIST_0];
    CopyMVBlock8(&mv_info[currMB->block_y], all_mv[l0_ref][mode8], LIST_0, currMB->block_x, 2, 4, 0);

    mode8 = currMB->b8x8[3].mode;
    l0_ref = mv_info[currMB->block_y + 2][currMB->block_x + 2].ref_idx[LIST_0];
    CopyMVBlock8(&mv_info[currMB->block_y], all_mv[l0_ref][mode8], LIST_0, currMB->block_x + 2, 2, 4, 2);
  }
  else // Intra modes
  {
    ResetMotionBlock16 (mv_info, LIST_0, currMB->block_x, currMB->block_y, 0, 4);
  }
}

/*!
 *************************************************************************************
 * \brief
 *    Sets motion vectors for a 16x8 partition in a B Slice
 *************************************************************************************
 */
static void SetMVBSlice16x8(Slice *currSlice, PicMotionParams **motion, Macroblock* currMB, int pos, int *next_wm_data, int is_modified_mv)
{
  int l0_ref, l1_ref, i, j, tmp_next_data = 0;
  int step_v = 2, step_h =4;  
  int pdir = currMB->b8x8[pos].pdir;
  VideoParameters *p_Vid = currMB->p_Vid;
  PixelPos       block[4];
  MotionVector predMV, mvd;
  
  if (pdir == LIST_0) 
  {
    l0_ref = motion[currMB->block_y + pos][currMB->block_x].ref_idx[LIST_0];

    if (1 == is_modified_mv && (is_watermark_insert(LIST_0))) {
      get_neighbors(currMB, block, 0, pos << 2, step_h<<2);
      currMB->GetMVPredictor (currMB, block, &predMV, (short) l0_ref, p_Vid->enc_picture->mv_info, LIST_0, 0, (pos << 2), step_h<<2, step_v<<2);
      mvd.mv_x = currSlice->all_mv [LIST_0][l0_ref][P16x8][pos][0].mv_x - predMV.mv_x;
      mvd.mv_y = currSlice->all_mv [LIST_0][l0_ref][P16x8][pos][0].mv_y - predMV.mv_y;
      B16x8_PRINT("%d predmv mvd.mv_x %d mvd.mv_y %d\n",__LINE__,predMV.mv_x,predMV.mv_y);
      //modified MVD
      B16x8_PRINT("original mvd.mv_x %d mvd.mv_y %d\n",mvd.mv_x,mvd.mv_y);
      tmp_next_data = watermark_mv_embed(&mvd,0, *next_wm_data);
      B16x8_PRINT("modified mvd.mv_x %d mvd.mv_y %d\n",mvd.mv_x,mvd.mv_y);

      for (j = pos; j < pos + 2; j++)
        {
          for (i = 0; i < BLOCK_MULTIPLE; i++)
            {
              currSlice->all_mv [LIST_0][l0_ref][P16x8][j][i].mv_x = predMV.mv_x + mvd.mv_x;
              currSlice->all_mv [LIST_0][l0_ref][P16x8][j][i].mv_y = predMV.mv_y + mvd.mv_y;
            }
        }
    }
    
    CopyMVBlock16(motion, currSlice->all_mv [LIST_0][l0_ref][P16x8], LIST_0, currMB->block_x, currMB->block_y, pos, pos + 2);
    ResetMotionBlock16 (motion, LIST_1, currMB->block_x, currMB->block_y, pos, pos + 2);
  }
  else if (pdir == LIST_1)
  {
    ResetMotionBlock16 (motion, LIST_0, currMB->block_x, currMB->block_y, pos, pos + 2);
    l1_ref = motion[currMB->block_y + pos][currMB->block_x].ref_idx[LIST_1];

    if (1 == is_modified_mv && (is_watermark_insert(LIST_1))) {
      get_neighbors(currMB, block, 0, pos << 2, step_h<<2);
      currMB->GetMVPredictor (currMB, block, &predMV, (short) l1_ref, p_Vid->enc_picture->mv_info, LIST_1, 0, (pos << 2), step_h<<2, step_v<<2);
      mvd.mv_x = currSlice->all_mv [LIST_1][l1_ref][P16x8][pos][0].mv_x - predMV.mv_x;
      mvd.mv_y = currSlice->all_mv [LIST_1][l1_ref][P16x8][pos][0].mv_y - predMV.mv_y;
      B16x8_PRINT("%d predmv mvd.mv_x %d mvd.mv_y %d\n",__LINE__,predMV.mv_x,predMV.mv_y);
      B16x8_PRINT("original mvd.mv_x %d mvd.mv_y %d\n",mvd.mv_x,mvd.mv_y);
      tmp_next_data = watermark_mv_embed(&mvd,0, *next_wm_data);
      B16x8_PRINT("modified mvd.mv_x %d mvd.mv_y %d\n",mvd.mv_x,mvd.mv_y);
    
      for (j = pos; j < pos + 2; j++)
        {
          for (i = 0; i < BLOCK_MULTIPLE; i++)
            {
              currSlice->all_mv [LIST_1][l1_ref][P16x8][j][i].mv_x = predMV.mv_x + mvd.mv_x;
              currSlice->all_mv [LIST_1][l1_ref][P16x8][j][i].mv_y = predMV.mv_y + mvd.mv_y;
            }
        }
    }    
    CopyMVBlock16 (motion, currSlice->all_mv [LIST_1][l1_ref][P16x8], LIST_1, currMB->block_x, currMB->block_y, pos, pos + 2);
  }
  else
  {
    int bipred_me = currMB->b8x8[pos].bipred;
    MotionVector *****all_mv = bipred_me ? currSlice->bipred_mv[bipred_me - 1]: currSlice->all_mv;
    l0_ref = motion[currMB->block_y + pos][currMB->block_x].ref_idx[LIST_0];

    if (1 == is_modified_mv && (is_watermark_insert(LIST_0))) {
      get_neighbors(currMB, block, 0, pos << 2, step_h<<2);
      currMB->GetMVPredictor (currMB, block, &predMV, (short) l0_ref, p_Vid->enc_picture->mv_info, LIST_0, 0, (pos << 2), step_h<<2, step_v<<2);
      mvd.mv_x = all_mv [LIST_0][l0_ref][P16x8][pos][0].mv_x - predMV.mv_x;
      mvd.mv_y = all_mv [LIST_0][l0_ref][P16x8][pos][0].mv_y - predMV.mv_y;

      B16x8_PRINT("%d predmv mvd.mv_x %d mvd.mv_y %d\n",__LINE__,predMV.mv_x,predMV.mv_y);
      //modified MVD
      B16x8_PRINT("original mvd.mv_x %d mvd.mv_y %d\n",mvd.mv_x,mvd.mv_y);
      tmp_next_data = watermark_mv_embed(&mvd,0, *next_wm_data);
      B16x8_PRINT("modified mvd.mv_x %d mvd.mv_y %d\n",mvd.mv_x,mvd.mv_y);
    
      for (j = pos; j < pos + 2; j++)
        {
          for (i = 0; i < BLOCK_MULTIPLE; i++)
            {
              all_mv [LIST_0][l0_ref][P16x8][j][i].mv_x = predMV.mv_x + mvd.mv_x;
              all_mv [LIST_0][l0_ref][P16x8][j][i].mv_y = predMV.mv_y + mvd.mv_y;
            }
        }
    }    
    CopyMVBlock16 (motion, all_mv [LIST_0][l0_ref][P16x8], LIST_0, currMB->block_x, currMB->block_y, pos, pos + 2);
    
    l1_ref = motion[currMB->block_y + pos][currMB->block_x].ref_idx[LIST_1];

    if (1 == is_modified_mv && (is_watermark_insert(LIST_1))) {
      get_neighbors(currMB, block, 0, pos << 2, step_h<<2);
      currMB->GetMVPredictor (currMB, block, &predMV, (short) l1_ref, p_Vid->enc_picture->mv_info, LIST_1, 0, (pos << 2), step_h<<2, step_v<<2);
      mvd.mv_x = all_mv [LIST_1][l1_ref][P16x8][pos][0].mv_x - predMV.mv_x;
      mvd.mv_y = all_mv [LIST_1][l1_ref][P16x8][pos][0].mv_y - predMV.mv_y;
      B16x8_PRINT("%d predmv mvd.mv_x %d mvd.mv_y %d\n",__LINE__,predMV.mv_x,predMV.mv_y);
      B16x8_PRINT("original mvd.mv_x %d mvd.mv_y %d\n",mvd.mv_x,mvd.mv_y);
      tmp_next_data = watermark_mv_embed(&mvd,0, *next_wm_data);
      B16x8_PRINT("modified mvd.mv_x %d mvd.mv_y %d\n",mvd.mv_x,mvd.mv_y);
    
      for (j = pos; j < pos + 2; j++)
        {
          for (i = 0; i < BLOCK_MULTIPLE; i++)
            {
              all_mv [LIST_1][l1_ref][P16x8][j][i].mv_x = predMV.mv_x + mvd.mv_x;
              all_mv [LIST_1][l1_ref][P16x8][j][i].mv_y = predMV.mv_y + mvd.mv_y;
            }
        }
    }    
    CopyMVBlock16 (motion, all_mv [LIST_1][l1_ref][P16x8], LIST_1, currMB->block_x, currMB->block_y, pos, pos + 2);

    if (bipred_me && (currSlice->mb_aff_frame_flag || (currSlice->UseRDOQuant && currSlice->RDOQ_QP_Num > 1)))
    {
      memcpy(currSlice->rddata->all_mv [LIST_0][l0_ref][P16x8][pos], all_mv [LIST_0][l0_ref][P16x8][pos], 2 * BLOCK_MULTIPLE * sizeof(MotionVector));
      memcpy(currSlice->rddata->all_mv [LIST_1][l1_ref][P16x8][pos], all_mv [LIST_1][l1_ref][P16x8][pos], 2 * BLOCK_MULTIPLE * sizeof(MotionVector));
    }
  }

  //Output next data for next inserting
  *next_wm_data = tmp_next_data;
}

/*!
 *************************************************************************************
 * \brief
 *    Sets motion vectors for a 8x16 partition in a B Slice
 *************************************************************************************
 */
static void SetMVBSlice8x16(Slice *currSlice, PicMotionParams **motion, Macroblock* currMB, int pos, int *next_wm_data, int is_modified_mv)
{
  int l0_ref, l1_ref, i, j, tmp_next_data = 0;
  int pdir = currMB->b8x8[pos >> 1].pdir;
  int step_v = 4, step_h = 2;  
  VideoParameters *p_Vid = currMB->p_Vid;
  PixelPos       block[4];
  MotionVector predMV, mvd;
  
  if (pdir == LIST_0) 
  {
    l0_ref = motion[currMB->block_y][currMB->block_x + pos].ref_idx[LIST_0];

    if (1 == is_modified_mv && (is_watermark_insert(LIST_0))) {
      get_neighbors(currMB, block, pos << 2, 0, step_h<<2);
      currMB->GetMVPredictor (currMB, block, &predMV, (short) l0_ref, p_Vid->enc_picture->mv_info, LIST_0, (pos << 2), 0, step_h<<2, step_v<<2);
      mvd.mv_x = currSlice->all_mv [LIST_0][l0_ref][P8x16][0][pos].mv_x - predMV.mv_x;
      mvd.mv_y = currSlice->all_mv [LIST_0][l0_ref][P8x16][0][pos].mv_y - predMV.mv_y;
      B8x16_PRINT("%d predmv mvd.mv_x %d mvd.mv_y %d\n",__LINE__,predMV.mv_x,predMV.mv_y);
      //modified MVD
      tmp_next_data = watermark_mv_embed(&mvd,0, *next_wm_data);
      B8x16_PRINT("modified mvd.mv_x %d mvd.mv_y %d\n",mvd.mv_x,mvd.mv_y);
    
      for (j = 0; j < 4; j++)
        {
          for (i = pos; i < pos + 2; i++)
            {
              currSlice->all_mv [LIST_0][l0_ref][P8x16][j][i].mv_x = predMV.mv_x + mvd.mv_x;
              currSlice->all_mv [LIST_0][l0_ref][P8x16][j][i].mv_y = predMV.mv_y + mvd.mv_y;
            }
        }
    }
    CopyMVBlock8 (&motion[currMB->block_y], currSlice->all_mv [LIST_0][l0_ref][P8x16], LIST_0, currMB->block_x + pos, 0, 4, pos);
    ResetMotionBlock8(&motion[currMB->block_y], LIST_1, currMB->block_x + pos, 0, 4);
  }
  else if (pdir == LIST_1)
  {
    ResetMotionBlock8(&motion[currMB->block_y], LIST_0, currMB->block_x + pos, 0, 4);
    l1_ref = motion[currMB->block_y][currMB->block_x + pos].ref_idx[LIST_1];

    if (1 == is_modified_mv && (is_watermark_insert(LIST_1))) {
      get_neighbors(currMB, block, pos << 2, 0, step_h<<2);
      currMB->GetMVPredictor (currMB, block, &predMV, (short) l1_ref, p_Vid->enc_picture->mv_info, LIST_1, (pos << 2), 0, step_h<<2, step_v<<2);
      mvd.mv_x = currSlice->all_mv [LIST_1][l1_ref][P8x16][0][pos].mv_x - predMV.mv_x;
      mvd.mv_y = currSlice->all_mv [LIST_1][l1_ref][P8x16][0][pos].mv_y - predMV.mv_y;
      B8x16_PRINT("%d predmv mvd.mv_x %d mvd.mv_y %d\n",__LINE__,predMV.mv_x,predMV.mv_y);
      //modified MVD
      tmp_next_data = watermark_mv_embed(&mvd,0, *next_wm_data);
      B8x16_PRINT("modified mvd.mv_x %d mvd.mv_y %d\n",mvd.mv_x,mvd.mv_y);
    
      for (j = 0; j < 4; j++)
        {
          for (i = pos; i < pos + 2; i++)
            {
              currSlice->all_mv [LIST_1][l1_ref][P8x16][j][i].mv_x = predMV.mv_x + mvd.mv_x;
              currSlice->all_mv [LIST_1][l1_ref][P8x16][j][i].mv_y = predMV.mv_y + mvd.mv_y;
            }
        }
    }    
    CopyMVBlock8 (&motion[currMB->block_y], currSlice->all_mv [LIST_1][l1_ref][P8x16], LIST_1, currMB->block_x + pos, 0, 4, pos);
  }
  else
  {
    int bipred_me = currMB->b8x8[pos >> 1].bipred;
    MotionVector *****all_mv = bipred_me ? currSlice->bipred_mv[bipred_me - 1]: currSlice->all_mv;
    l0_ref = motion[currMB->block_y][currMB->block_x + pos].ref_idx[LIST_0];

    if (1 == is_modified_mv && (is_watermark_insert(LIST_0))) {
      get_neighbors(currMB, block, pos << 2, 0, step_h<<2);
      currMB->GetMVPredictor (currMB, block, &predMV, (short) l0_ref, p_Vid->enc_picture->mv_info, LIST_0, (pos << 2), 0, step_h<<2, step_v<<2);
      mvd.mv_x = all_mv [LIST_0][l0_ref][P8x16][0][pos].mv_x - predMV.mv_x;
      mvd.mv_y = all_mv [LIST_0][l0_ref][P8x16][0][pos].mv_y - predMV.mv_y;
      B8x16_PRINT("%d predmv mvd.mv_x %d mvd.mv_y %d\n",__LINE__,predMV.mv_x,predMV.mv_y);
      //modified MVD
      tmp_next_data = watermark_mv_embed(&mvd,0, *next_wm_data);
      B8x16_PRINT("modified mvd.mv_x %d mvd.mv_y %d\n",mvd.mv_x,mvd.mv_y);
    
      for (j = 0; j < 4; j++)
        {
          for (i = pos; i < pos + 2; i++)
            {
              all_mv [LIST_0][l0_ref][P8x16][j][i].mv_x = predMV.mv_x + mvd.mv_x;
              all_mv [LIST_0][l0_ref][P8x16][j][i].mv_y = predMV.mv_y + mvd.mv_y;
            }
        }
    }
    CopyMVBlock8(&motion[currMB->block_y], all_mv [LIST_0][l0_ref][P8x16], LIST_0, currMB->block_x + pos, 0, 4, pos);
    
    l1_ref = motion[currMB->block_y][currMB->block_x + pos].ref_idx[LIST_1];
    //TODO(truongpt)
    if (1 == is_modified_mv && (is_watermark_insert(LIST_1))) {
      get_neighbors(currMB, block, pos << 2, 0, step_h<<2);
      currMB->GetMVPredictor (currMB, block, &predMV, (short) l1_ref, p_Vid->enc_picture->mv_info, LIST_1, (pos << 2), 0, step_h<<2, step_v<<2);
      mvd.mv_x = all_mv [LIST_1][l1_ref][P8x16][0][pos].mv_x - predMV.mv_x;
      mvd.mv_y = all_mv [LIST_1][l1_ref][P8x16][0][pos].mv_y - predMV.mv_y;
      B8x16_PRINT("%d predmv mvd.mv_x %d mvd.mv_y %d\n",__LINE__,predMV.mv_x,predMV.mv_y);
      //modified MVD
      tmp_next_data = watermark_mv_embed(&mvd,0, *next_wm_data);
      B8x16_PRINT("modified mvd.mv_x %d mvd.mv_y %d\n",mvd.mv_x,mvd.mv_y);
    
      for (j = 0; j < 4; j++)
        {
          for (i = pos; i < pos + 2; i++)
            {
              all_mv [LIST_1][l1_ref][P8x16][j][i].mv_x = predMV.mv_x + mvd.mv_x;
              all_mv [LIST_1][l1_ref][P8x16][j][i].mv_y = predMV.mv_y + mvd.mv_y;
            }
        }
    }
    CopyMVBlock8(&motion[currMB->block_y], all_mv [LIST_1][l1_ref][P8x16], LIST_1, currMB->block_x + pos, 0, 4, pos);

    if (bipred_me && (currSlice->mb_aff_frame_flag || (currSlice->UseRDOQuant && currSlice->RDOQ_QP_Num > 1)))
    {
      int j;
      for (j = 0; j < BLOCK_MULTIPLE; j++)
      {
        memcpy(&currSlice->rddata->all_mv [LIST_0][l0_ref][P8x16][j][pos], &all_mv [LIST_0][l0_ref][P8x16][j][pos], 2 * sizeof(MotionVector));
      }
      for (j = 0; j < BLOCK_MULTIPLE; j++)
      {
        memcpy(&currSlice->rddata->all_mv [LIST_1][l1_ref][P8x16][j][pos], &all_mv [LIST_1][l1_ref][P8x16][j][pos], 2 * sizeof(MotionVector));
      }
    }
  }
  //Output next data for next inserting
  *next_wm_data = tmp_next_data;
}

/*!
 *************************************************************************************
 * \brief
 *    Sets motion vectors for a 8x8 partition in a B Slice
 *************************************************************************************
 */
static void SetMVBSlice8x8(Slice *currSlice, PicMotionParams **motion, Macroblock* currMB, int pos_y, int pos_x)
{
  int l0_ref, l1_ref;
  int pos = pos_y + (pos_x >> 1);
  int pdir = currMB->b8x8[pos].pdir;
  int mode = currMB->b8x8[pos].mode;
  int block_y = currMB->block_y + pos_y;
  int block_x = currMB->block_x + pos_x;

  if (pdir == LIST_0) 
  {
    l0_ref = motion[block_y][block_x].ref_idx[LIST_0];
    CopyMVBlock8 (&motion[currMB->block_y], currSlice->all_mv [LIST_0][l0_ref][mode], LIST_0, currMB->block_x + pos_x, pos_y, pos_y + 2, pos_x);
    ResetMotionBlock8 (&motion[currMB->block_y], LIST_1, currMB->block_x + pos_x, pos_y, pos_y + 2);
  }
  else if (pdir == LIST_1)
  {
    ResetMotionBlock8(&motion[currMB->block_y], LIST_0, currMB->block_x + pos_x, pos_y, pos_y + 2);
    l1_ref = motion[block_y][block_x].ref_idx[LIST_1];    
    CopyMVBlock8 (&motion[currMB->block_y], currSlice->all_mv [LIST_1][l1_ref][mode], LIST_1, currMB->block_x + pos_x, pos_y, pos_y + 2, pos_x);
  }
  else if (pdir == BI_PRED)
  {
    int bipred_me = currMB->b8x8[pos].bipred;
    MotionVector *****all_mv = bipred_me ? currSlice->bipred_mv[bipred_me - 1]: currSlice->all_mv;
    l0_ref = motion[block_y][block_x].ref_idx[LIST_0];
    CopyMVBlock8(&motion[currMB->block_y], all_mv [LIST_0][l0_ref][mode], LIST_0, currMB->block_x + pos_x, pos_y, pos_y + 2, pos_x);
    
    l1_ref = motion[block_y][block_x].ref_idx[LIST_1];    
    CopyMVBlock8(&motion[currMB->block_y], all_mv [LIST_1][l1_ref][mode], LIST_1, currMB->block_x + pos_x, pos_y, pos_y + 2, pos_x);

    if (bipred_me && (currSlice->mb_aff_frame_flag || (currSlice->UseRDOQuant && currSlice->RDOQ_QP_Num > 1)))
    {
      memcpy(&currSlice->rddata->all_mv [LIST_0][l0_ref][mode][pos_y    ][pos_x], &all_mv [LIST_0][l0_ref][mode][pos_y    ][pos_x], 2 * sizeof(MotionVector));
      memcpy(&currSlice->rddata->all_mv [LIST_0][l0_ref][mode][pos_y + 1][pos_x], &all_mv [LIST_0][l0_ref][mode][pos_y + 1][pos_x], 2 * sizeof(MotionVector));
      memcpy(&currSlice->rddata->all_mv [LIST_1][l1_ref][mode][pos_y    ][pos_x], &all_mv [LIST_1][l1_ref][mode][pos_y    ][pos_x], 2 * sizeof(MotionVector));
      memcpy(&currSlice->rddata->all_mv [LIST_1][l1_ref][mode][pos_y + 1][pos_x], &all_mv [LIST_1][l1_ref][mode][pos_y + 1][pos_x], 2 * sizeof(MotionVector));
    }
  }
  else // Invalid direct modes (out of range mvs). Adding this here for precaution purposes.
  {
    ResetMotionBlock8(&motion[currMB->block_y], LIST_0, currMB->block_x + pos_x, pos_y, pos_y + 2);
    ResetMotionBlock8(&motion[currMB->block_y], LIST_1, currMB->block_x + pos_x, pos_y, pos_y + 2);
  }
}

void SetMotionVectorsMBISlice (Macroblock* currMB, int is_modified_mv)
{
}

/*!
 *************************************************************************************
 * \brief
 *    Sets motion vectors for a macroblock in a P Slice
 *************************************************************************************
 */
void SetMotionVectorsMBBSlice (Macroblock* currMB, int is_modified_mv)
{
  VideoParameters *p_Vid = currMB->p_Vid;
  Slice *currSlice = currMB->p_Slice;
  PicMotionParams **motion = p_Vid->enc_picture->mv_info;

  PixelPos       block[4];
  int step_v = 4, step_h =4;
  MotionVector predMV, mvd;
  int j,i;  
  int  l0_ref, l1_ref;

  // copy all the motion vectors into rdopt structure
  // Can simplify this by copying the MV's of the best mode (TBD)
  // Should maybe add code to check for Intra only profiles

  if (currSlice->mb_aff_frame_flag || (currSlice->UseRDOQuant && currSlice->RDOQ_QP_Num > 1))
  {
    memcpy(&currSlice->rddata->all_mv[LIST_0][0][0][0][0], &currSlice->all_mv[LIST_0][0][0][0][0], 2 * p_Vid->max_num_references * 9 * MB_BLOCK_PARTITIONS * sizeof(MotionVector));
  }

  if (currMB->mb_type == P16x16) // 16x16
  {
    int pdir = currMB->b8x8[0].pdir;      
    if (pdir == LIST_0) 
    {
      l0_ref = motion[currMB->block_y][currMB->block_x].ref_idx[LIST_0];

      if (1 == is_modified_mv && is_watermark_insert(LIST_0) ) {
        get_neighbors(currMB, block, 0, 0, step_h<<2);
        currMB->GetMVPredictor (currMB, block, &predMV, (short) l0_ref, p_Vid->enc_picture->mv_info, LIST_0, 0, 0, step_h<<2, step_v<<2);
        BSLICE_PRINT("%s predMV.mv_x %d predMV.mv_x %d\n",__func__,predMV.mv_x,predMV.mv_y);

        //calculate original MVD
        mvd.mv_x = currSlice->all_mv [LIST_0][l0_ref][P16x16][0][0].mv_x - predMV.mv_x;
        mvd.mv_y = currSlice->all_mv [LIST_0][l0_ref][P16x16][0][0].mv_y - predMV.mv_y;
        // modify MVD
        BSLICE_PRINT("original mvd.mv_x %d mvd.mv_y %d\n",mvd.mv_x,mvd.mv_y);
        watermark_mv_embed(&mvd,0,0);
        BSLICE_PRINT("modified mvd.mv_x %d mvd.mv_y %d\n",mvd.mv_x,mvd.mv_y);
	
        for (j = 0; j < 4; j++)
        {
          for (i = 0; i < BLOCK_MULTIPLE; i++)
          {
            currSlice->all_mv [LIST_0][l0_ref][P16x16][j][i].mv_x = predMV.mv_x + mvd.mv_x;
            currSlice->all_mv [LIST_0][l0_ref][P16x16][j][i].mv_y = predMV.mv_y + mvd.mv_y;
          }
        }
      }

      CopyMVBlock16 (motion, currSlice->all_mv [LIST_0][l0_ref][P16x16], LIST_0, currMB->block_x, currMB->block_y, 0, 4);
      ResetMotionBlock16(motion, LIST_1, currMB->block_x, currMB->block_y, 0, 4);
    }
    else if (pdir == LIST_1)
    {
      l1_ref = motion[currMB->block_y][currMB->block_x].ref_idx[LIST_1];
      ResetMotionBlock16 (motion, LIST_0, currMB->block_x, currMB->block_y, 0, 4);
      
      if (1 == is_modified_mv && is_watermark_insert(LIST_1)) {
        get_neighbors(currMB, block, 0, 0, step_h<<2);
        currMB->GetMVPredictor (currMB, block, &predMV, (short) l1_ref, p_Vid->enc_picture->mv_info, LIST_1, 0, 0, step_h<<2, step_v<<2);
        BSLICE_PRINT("%s predMV.mv_x %d predMV.mv_x %d\n",__func__,predMV.mv_x,predMV.mv_y);
        //calculate original MVD
        mvd.mv_x = currSlice->all_mv [LIST_1][l1_ref][P16x16][0][0].mv_x - predMV.mv_x;
        mvd.mv_y = currSlice->all_mv [LIST_1][l1_ref][P16x16][0][0].mv_y - predMV.mv_y;
        // modify MVD
        BSLICE_PRINT("original mvd.mv_x %d mvd.mv_y %d\n",mvd.mv_x,mvd.mv_y);
        watermark_mv_embed(&mvd,0,0);
        BSLICE_PRINT("modified mvd.mv_x %d mvd.mv_y %d\n",mvd.mv_x,mvd.mv_y);
        
        for (j = 0; j < 4; j++)
        {
          for (i = 0; i < BLOCK_MULTIPLE; i++)
          {
            currSlice->all_mv [LIST_1][l1_ref][P16x16][j][i].mv_x = predMV.mv_x + mvd.mv_x;
            currSlice->all_mv [LIST_1][l1_ref][P16x16][j][i].mv_y = predMV.mv_y + mvd.mv_y;
          }
        }
      }
      CopyMVBlock16 (motion, currSlice->all_mv [LIST_1][l1_ref][P16x16], LIST_1, currMB->block_x, currMB->block_y, 0, 4);
    }
    else
    {
      int bipred_me = currMB->b8x8[0].bipred;
      MotionVector *****all_mv  = bipred_me ? currSlice->bipred_mv[bipred_me - 1]: currSlice->all_mv;
      int next_wm_data_l1 = 0;
      
      l0_ref = motion[currMB->block_y][currMB->block_x].ref_idx[LIST_0];
      if (1 == is_modified_mv && is_watermark_insert(LIST_0))
        {
          get_neighbors(currMB, block, 0, 0, step_h<<2);
          currMB->GetMVPredictor (currMB, block, &predMV, (short) l0_ref, p_Vid->enc_picture->mv_info, LIST_0, 0, 0, step_h<<2, step_v<<2);
          BSLICE_PRINT("%s predMV.mv_x %d predMV.mv_x %d\n",__func__,predMV.mv_x,predMV.mv_y);
          //calculate original MVD
          mvd.mv_x = currSlice->all_mv [LIST_0][l0_ref][P16x16][0][0].mv_x - predMV.mv_x;
          mvd.mv_y = currSlice->all_mv [LIST_0][l0_ref][P16x16][0][0].mv_y - predMV.mv_y;
          // modify MVD	
          BSLICE_PRINT("original mvd.mv_x %d mvd.mv_y %d\n",mvd.mv_x,mvd.mv_y);
          next_wm_data_l1 = watermark_mv_embed(&mvd,0,0);
          BSLICE_PRINT("modified mvd.mv_x %d mvd.mv_y %d\n",mvd.mv_x,mvd.mv_y);

          for (j = 0; j < 4; j++)
          {
            for (i = 0; i < BLOCK_MULTIPLE; i++)
            {
              all_mv [LIST_0][l0_ref][P16x16][j][i].mv_x = predMV.mv_x + mvd.mv_x;
              all_mv [LIST_0][l0_ref][P16x16][j][i].mv_y = predMV.mv_y + mvd.mv_y;
            }
          }
      }
      CopyMVBlock16 (motion, all_mv [LIST_0][l0_ref][P16x16], LIST_0, currMB->block_x, currMB->block_y, 0, 4);

      // processing for backwar reference
      l1_ref = motion[currMB->block_y][currMB->block_x].ref_idx[LIST_1];      
      if (1 == is_modified_mv && is_watermark_insert(LIST_1)) {
        get_neighbors(currMB, block, 0, 0, step_h<<2);
        currMB->GetMVPredictor (currMB, block, &predMV, (short) l1_ref, p_Vid->enc_picture->mv_info, LIST_1, 0, 0, step_h<<2, step_v<<2);
        BSLICE_PRINT("%s predMV.mv_x %d predMV.mv_x %d\n",__func__,predMV.mv_x,predMV.mv_y);
        BSLICE_PRINT("%s line %d L1\n",__func__,__LINE__);
        //calculate original MVD
        mvd.mv_x = currSlice->all_mv [LIST_1][l1_ref][P16x16][0][0].mv_x - predMV.mv_x;
        mvd.mv_y = currSlice->all_mv [LIST_1][l1_ref][P16x16][0][0].mv_y - predMV.mv_y;
        // modify MVD
        BSLICE_PRINT("original mvd.mv_x %d mvd.mv_y %d\n",mvd.mv_x,mvd.mv_y);
        watermark_mv_embed(&mvd,0,next_wm_data_l1);
        BSLICE_PRINT("modified mvd.mv_x %d mvd.mv_y %d\n",mvd.mv_x,mvd.mv_y);
        
        for (j = 0; j < 4; j++)
        {
          for (i = 0; i < BLOCK_MULTIPLE; i++)
          {
            //truongpt
            all_mv [LIST_1][l1_ref][P16x16][j][i].mv_x = predMV.mv_x + mvd.mv_x;
            all_mv [LIST_1][l1_ref][P16x16][j][i].mv_y = predMV.mv_y + mvd.mv_y;
          }
        }
      }
      CopyMVBlock16 (motion, all_mv [LIST_1][l1_ref][P16x16], LIST_1, currMB->block_x, currMB->block_y, 0, 4);

      // Is this necessary here? Can this be moved somewhere else?
      if (bipred_me && (currSlice->mb_aff_frame_flag || (currSlice->UseRDOQuant && currSlice->RDOQ_QP_Num > 1)))
      {
        memcpy(&currSlice->rddata->all_mv [LIST_0][l0_ref][P16x16][0][0], &all_mv [LIST_0][l0_ref][P16x16][0][0], MB_BLOCK_PARTITIONS * sizeof(MotionVector));
        memcpy(&currSlice->rddata->all_mv [LIST_1][l1_ref][P16x16][0][0], &all_mv [LIST_1][l1_ref][P16x16][0][0], MB_BLOCK_PARTITIONS * sizeof(MotionVector));
      }
    }
  }
  else if (currMB->mb_type == P16x8) // 16x8
  {
    int next_wm_data = 0;
    SetMVBSlice16x8(currSlice, motion, currMB, 0, &next_wm_data, is_modified_mv);
    SetMVBSlice16x8(currSlice, motion, currMB, 2, &next_wm_data, is_modified_mv);
  }
  else if (currMB->mb_type == P8x16) // 16x8
  {
    int next_wm_data = 0;
    SetMVBSlice8x16(currSlice, motion, currMB, 0, &next_wm_data, is_modified_mv);
    SetMVBSlice8x16(currSlice, motion, currMB, 2, &next_wm_data, is_modified_mv);
  }
  else if (currMB->mb_type == P8x8 || currMB->mb_type == BSKIP_DIRECT) // 8x8 & Direct/SKIP
  { 
    SetMVBSlice8x8(currSlice, motion, currMB, 0, 0);
    SetMVBSlice8x8(currSlice, motion, currMB, 0, 2);
    SetMVBSlice8x8(currSlice, motion, currMB, 2, 0);
    SetMVBSlice8x8(currSlice, motion, currMB, 2, 2);
  }
  else // Intra modes
  {
    ResetMotionBlock16 (motion, LIST_0, currMB->block_x, currMB->block_y, 0, 4);
    ResetMotionBlock16 (motion, LIST_1, currMB->block_x, currMB->block_y, 0, 4);
  }
}

/*!
 *************************************************************************************
 * \brief
 *    Copy ImgPel Data from one structure to another (16x16)
 *************************************************************************************
 */
void copy_image_data_16x16(imgpel  **imgBuf1, imgpel  **imgBuf2, int off1, int off2)
{
  int j;
  for(j=0; j<MB_BLOCK_SIZE; j++)
  {
    memcpy(&imgBuf1[j][off1], &imgBuf2[j][off2], MB_BLOCK_SIZE * sizeof (imgpel));
  }
}

/*!
 *************************************************************************************
 * \brief
 *    Copy ImgPel Data from one structure to another (8x8)
 *************************************************************************************
 */
void copy_image_data_8x8(imgpel  **imgBuf1, imgpel  **imgBuf2, int off1, int off2)
{
  int j;
  for(j = 0; j < BLOCK_SIZE_8x8; j++)
  {
    memcpy(&imgBuf1[j][off1], &imgBuf2[j][off2], BLOCK_SIZE_8x8 * sizeof (imgpel));
  }
}

/*!
 *************************************************************************************
 * \brief
 *    Copy ImgPel Data from one structure to another (4x4)
 *************************************************************************************
 */
void copy_image_data_4x4(imgpel  **imgBuf1, imgpel  **imgBuf2, int off1, int off2)
{
  int j;
  for(j = 0; j < BLOCK_SIZE; j++)
  {
    memcpy(&imgBuf1[j][off1], &imgBuf2[j][off2], BLOCK_SIZE * sizeof (imgpel));
  }
}

/*!
 *************************************************************************************
 * \brief
 *    Copy ImgPel Data from one structure to another (8x8)
 *************************************************************************************
 */
void copy_image_data(imgpel  **imgBuf1, imgpel  **imgBuf2, int off1, int off2, int width, int height)
{
  int j;
  for(j = 0; j < height; j++)
  {
    memcpy(&imgBuf1[j][off1], &imgBuf2[j][off2], width * sizeof (imgpel));
  }
}

/*
*************************************************************************************
* \brief
*    for an 8x8 sub-macroblock: initialization of RD_8x8DATA
*************************************************************************************
*/
void ResetRD8x8Data(VideoParameters *p_Vid, RD_8x8DATA *rd_data)
{
  int block;
  p_Vid->giRDOpt_B8OnlyFlag = TRUE;

  rd_data->mb_p8x8_cost = 0;
  rd_data->cbp8x8 = 0;
  rd_data->cbp_blk8x8 = 0;
  rd_data->cnt_nonz_8x8 = 0;

  for (block = 0; block < 4; block++)
  {
    rd_data->smb_p8x8_cost[block] = DISTBLK_MAX;
    rd_data->smb_p8x8_rdcost[block] = DISTBLK_MAX;
    rd_data->part[block].mode = -1;
  }
}

/*!
*************************************************************************************
* \brief
*    set the range of chroma prediction mode
*************************************************************************************
*/
void set_chroma_pred_mode(Macroblock *currMB, RD_PARAMS enc_mb, int *mb_available, char chroma_pred_mode_range[2])
{
  VideoParameters *p_Vid = currMB->p_Vid;
  InputParameters *p_Inp = currMB->p_Inp;

  if ((p_Vid->yuv_format != YUV400) && (p_Inp->separate_colour_plane_flag == 0))
  {
    Slice *currSlice = currMB->p_Slice;
    // precompute all new chroma intra prediction modes
    currSlice->intra_chroma_prediction(currMB, &mb_available[0], &mb_available[1], &mb_available[2]);

    if (p_Inp->FastCrIntraDecision) 
    {          
      currSlice->intra_chroma_RD_decision(currMB, &enc_mb);

      chroma_pred_mode_range[0] = currMB->c_ipred_mode;
      chroma_pred_mode_range[1] = currMB->c_ipred_mode;
    }
    else 
    {
      chroma_pred_mode_range[0] = DC_PRED_8;
      chroma_pred_mode_range[1] = PLANE_8;
    }
  }
  else
  {
    chroma_pred_mode_range[0] = DC_PRED_8;        
    chroma_pred_mode_range[1] = DC_PRED_8;
  }
}
