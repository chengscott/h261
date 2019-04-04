/*************************************************************************
 *
 *	Name:		mytime.h
 *	Description:	timer setting for running program
 *	Date: 96/04/16	Author: Chu Ching-Wen in N.T.H.U., Taiwan
 *
 *************************************************************************/

#ifndef TIME_DONE
#define TIME_DONE

#include <time.h>
#include "ctrl.h"	/* set timer or not ? */

#include <sys/timeb.h>
#define TIME    struct timeb
#define get_time(t)     ftime(&t)
#define diff_time(t2, t1)       (((long)t2.time*1000+t2.millitm)-(t1.time*1000+t1.millitm))
#define TIME_UNIT 	((double) 1/1000)

#if ((!defined(DOS)) || defined(MAIN))
double tTIME_COST;	/* the cost of {get_time(t1),get_time(t2),diff_time()} */
TIME tTOTAL1, tME1, tDCT1, tQUAN1;
TIME tIDCT1, tIQUAN1;
TIME tTOTAL2, tME2, tDCT2, tQUAN2;
TIME tIDCT2, tIQUAN2;
long tTOTAL, tME, tDCT, tQUAN;
long tIDCT, tIQUAN;
long ntTOTAL, ntME, ntDCT, ntQUAN;
long ntIDCT, ntIQUAN;
#else
extern double tTIME_COST;	/* the cost of {get_time(t1),get_time(t2),diff_time()} */
extern TIME tTOTAL1, tME1, tDCT1, tQUAN1, tIO1;
extern TIME tIDCT1, tIQUAN1;
extern TIME tTOTAL2, tME2, tDCT2, tQUAN2;
extern TIME tIDCT2, tIQUAN2;
extern long tTOTAL, tME, tDCT, tQUAN;
extern long tIDCT, tIQUAN;
extern long ntTOTAL, ntME, ntDCT, ntQUAN;
extern long ntIDCT, ntIQUAN;
#endif

#endif

