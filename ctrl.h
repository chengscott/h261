/*************************************************************************
 *
 *	Name:		ctrl.h
 *	Description:	codec controls: statistics, all intra, get time ...
 *			controls are just for partial ?.c, not for all
 *			(controls for all are in globals.h (DEBUG etc.)
 *	Date: 96/05/11	Author: Chu Ching-Wen in N.T.H.U., Taiwan
 *
 *************************************************************************/

#ifndef CTRL_DONE
#define CTRL_DONE

/*************************************************************************/
/* get time info. or not */
#define GET_TOTAL_TIME	0	/* get total time or not */
#define GET_ALL_TIME	1	/* get times of every parts or not */
#define CTRL_GET_TIME GET_TOTAL_TIME	/* h261.c dct.c codec.c stat.c */
/*#define CTRL_GET_TIME GET_ALL_TIME	/* h261.c dct.c codec.c stat.c */

/*************************************************************************/
/* show thresholds or not */
#define CTRL_SHOW_THRESH	/* h261.c stat.c */

/*************************************************************************/
/* show MTYPE statistic */
/*#define CTRL_STAT_MTYPE   	/* h261.c stat.c */

/*************************************************************************/
/* show frame info.: bits and MTYPE statistic info for each frame */
/*#define CTRL_FRAME_INFO   	/* h261.c stat.c */

/*************************************************************************/
/* count header bits or not */
/*#define CTRL_HEADER_BITS  	/* header.c stat.c */

/*************************************************************************/
/* run statistics() (obtain psnr for each frame) or not */
/*#define CTRL_PSNR		/* h261.c stat.c */

/*************************************************************************/
/* all intra mode or not */
/*#define CTRL_ALL_INTRA   	/* h261.c */

#endif
