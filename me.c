/*************************************************************************
 *
 *	Name:	       	me.c
 *	Description:	motion estimation for a single frame
 *	Date: 96/05/09	Author: Chu Ching-Wen in N.T.H.U., Taiwan
 *
 *************************************************************************/

#include "globals.h"
#include "mytime.h"     /* TIME-type variables definition & function */
#include "thresh.h"	/* threshold values definition */

/*************************************************************************/
/* public */
extern void motion_estimation(void);
extern int16 full_search_ME(MEM *preBLK, MEM *curBLK);
extern int16 three_step_search_ME(MEM *preBLK, MEM *curBLK);
extern int16 new_three_step_search_ME(MEM *preBLK, MEM *curBLK);
extern int16 my_search_ME(MEM *preBLK, MEM *curBLK);

/*************************************************************************/
/* frame stores */
/* frame stores for encoder */
extern FSTORE *ori_frame;	/* original frame in encoder */
extern FSTORE *rec_frame;	/* reconstructed frame in encoder */
/* for motion estimation in encoder */
extern FSTORE *rec_frame_backup;/* save some "uncovered" MBs in encoder
				 * if MVD points to upper or left MB */
extern FSTORE *ref_frame;	/* point to reference frame
				 * rec_frame or rec_frame_backup
				 * it's just a pointer */
/* frame stores for decoder */
extern FSTORE *reco_frame;	/* reconstructed frame in decoder */
extern FSTORE *last_frame;	/* last reconstructed frame in decoder */

/*************************************************************************/
/* to store MB info. of the last frame: MTYPE, MVDH and MVDV */
extern int16 *MTYPE_frame;/* (MVDH_frame[i] == n)  =>
			   *   MTYPE of the i-th MB of last frame is n */
extern int16 *MVDH_frame; /* (MVDH_frame[i] == n)  =>
			   *   MVDH of the i-th MB of last frame is n */
extern int16 *MVDV_frame; /* (MVDH_frame[i] == n)  =>
			   *   MVDV of the i-th MB of last frame is n */
extern int16 *Last_update;/* (Last_update[i] == n) =>
			   *   the i-th MB has not updated in last n frames */

/*************************************************************************/
/* Look-up-table for obtaining memloc in MEM: */
/* Position of the upper-left connor in the GurrentGOB-th GOB for Y-type */
extern int32 YGOB_memloc[12];
/* Position of the upper-left connor in the GurrentGOB-th GOB for Cb, Cr */
extern int32 GOB_memloc[12];

/* Position of the upper-left connor in the Current_MB-th MB for Y-type */
extern int32 YMB_memloc[33];
/* Position of the upper-left connor in the Current_MB-th MB for Cb, Cr */
extern int32 MB_memloc[33];

/* Distance due to MVDV for Y-type */
extern int32 YMVDV_memloc[31];
/* Distance due to MVDV for Cb, Cr */
extern int32 MVDV_memloc[31];

/* (GOB_posX[i], GOB_poxY[i]) is the position of the i-th GOB */
extern int32 GOB_posX[12];
extern int32 GOB_posY[12];

/* (MB_posX[i], MB_poxY[i]) is the position of the i-th MB */
extern int32 MB_posX[33];
extern int32 MB_posY[33];

extern boolean MVD_used[];		/* Motion Vector Data used */
extern int16 Current_GOB;
extern int16 Current_MB;
extern int16 Number_GOB;
extern int16 Number_MB;

extern IMAGE *Image;		/* global info. of image */

/*************************************************************************/
/* private */
static int16 obtain_MTYPE(MEM *pmem, MEM *cmem);
static int16 absolute_error_SB(MEM *preBLK, MEM *curBLK);
static int16 absolute_error_SB_shortcut(MEM *preBLK, MEM *curBLK, int16 bound);

/*************************************************************************/
/* program used */
static int16 CurrentX;
static int16 CurrentY;
static int16 nMB;
static int16 *Last_update_ptr;
static int16 MVDH;	/* Motion Vector Data for Horizontal offset */
static int16 MVDV;	/* Motion Vector Data for Vertical offset */
static int16 AE_zero;	/* AE for the zero-displacement SB (super-block) */
static int16 AE_best;	/* AE (absolute error) for the best-match SB */

/* SET_memloc() sets mem->memloc to the right position by Current_GOB
and Current_MB */
#define SET_memloc(mem, Y_memloc, CbCr_memloc) {\
		mem->fs[_Y]->memloc = Y_memloc;\
		mem->fs[_Cb]->memloc = mem->fs[_Cr]->memloc = CbCr_memloc;\
	}

/*************************************************************************
 *
 *	Name:		motion_estimation()
 *	Description:	apply motion estimation on two frames ((ori_frame
 *			and Y_frame) or (ori_frame and rec_frame)), and
 *                      buffer the referenced macro-blocks with negative
 *			MVDH or MVDV for future use
 *	Input:		none
 *	Return:	       	none
 *	Side effects:	MTYPE_frame, MVDH_frame, MVDV_frame will be changed
 *	Date: 96/04/30	Author: Chu Ching-Wen in N.T.H.U., Taiwan
 *
 *************************************************************************/
void motion_estimation(void)
{
	DEBUG("motion_estimation");
	int32 YGOB_memloc1;
	int32 Y_memloc, CbCr_memloc;
	int16 MTYPE;
	extern boolean Intra_used[];

	#if (CTRL_GET_TIME==GET_ALL_TIME)
	get_time(tME1);
	#endif

	/* set MTYPE and MV for one superblock */
	Last_update_ptr = Last_update;
	for (nMB=Current_GOB=0; Current_GOB<Number_GOB; Current_GOB++) {
		YGOB_memloc1 = YGOB_memloc[Current_GOB];
		for (Current_MB=0; Current_MB<Number_MB;
				Current_MB++,nMB++,Last_update_ptr++) {
			/* get the memory location of current MB */
			ori_frame->fs[_Y]->memloc = Y_memloc
				= YGOB_memloc1 + YMB_memloc[Current_MB];

			/* use rec_frame as previous coded frame store */
			rec_frame->fs[_Y]->memloc = Y_memloc;
			MTYPE_frame[nMB] = MTYPE
			= obtain_MTYPE(rec_frame->fs[_Y], ori_frame->fs[_Y]);

			if (Intra_used[MTYPE]) {
				*Last_update_ptr = 0;
			} else {
				(*Last_update_ptr)++;
			}

			/* if current MB should reference above or left MB,
			 * then we shold buffer the "uncover" MB for future
			 * use... get motion compacated block */
			if (MVD_used[MTYPE]) {
				if ((MVDH<0) || (MVDV<0)) {
					/* obtain new memloc by current MV */
					if (MVDV>=0) {
						Y_memloc +=
						(YMVDV_memloc[MVDV] + MVDH);

						CbCr_memloc
						= GOB_memloc[Current_GOB]
						+ MB_memloc[Current_MB]
						+ (MVDV_memloc[MVDV]
						+ (MVDH>>1));
					} else {
						Y_memloc +=
						(-YMVDV_memloc[-MVDV] + MVDH);

						CbCr_memloc
						= GOB_memloc[Current_GOB]
						+ MB_memloc[Current_MB]
						+ (-MVDV_memloc[-MVDV]
						+ (MVDH>>1));
					}
					//copy_MB(rec_frame, ori_frame);
					SET_memloc(rec_frame,
						Y_memloc, CbCr_memloc);
					SET_memloc(rec_frame_backup,
						Y_memloc, CbCr_memloc);
					copy_MB(rec_frame_backup, rec_frame);
				 }
			}
		}
	}

	#if (CTRL_GET_TIME==GET_ALL_TIME)
	get_time(tME2);
	ntME++;
	ntTOTAL++;
	tME += diff_time(tME2, tME1);
	#endif
}

#define RETURN_MTYPE(MTYPE) {\
		/* nMB for fetch the previous frame's MVs */\
		MVDH_frame[nMB] = MVDV_frame[nMB] = 0;\
		return MTYPE;\
	}

#define RETURN_MTYPE_MV(MTYPE) {\
		/* nMB for fetch the previous frame's MVs */\
		MVDH_frame[nMB] = MVDH;\
		MVDV_frame[nMB] = MVDV;\
		return MTYPE;\
	}

/*************************************************************************
 *
 *	Name:		obtain_MTYPE()
 *	Description:	apply motion estimation for current super-block in
 *			cmem (reference pmem), and obtain MTYPE accordingly
 *	Input:          the pointers to the current and previous Y frames,
 *			the extern rec_frame->fs[_Y] is used for the
 *			decision of MTYPE after MVDs have been obtained
 *	Return:	       	MTYPE of current macro-block
 *	Side effects:	MVDH, MVDV, MVDH_frame, MVDV_frame, AE_zero, and
 *			AE_best will be changed
 *	Date: 96/05/09	Author: Chu Ching-Wen in N.T.H.U., Taiwan
 *
 *************************************************************************/
static int16 obtain_MTYPE(MEM *pmem, MEM *cmem)
{
	DEBUG("obtain_MTYPE");
	extern int16 (*default_me_algo)(MEM *, MEM *);
	#define use_me_algo (*default_me_algo)

	/* set MTYPE and MV for one superblock */
	/* pmem is previous frame: SB start position would not be changed */
	/* cmem is current SB */

	AE_best = AE_zero = absolute_error_SB(pmem, cmem);
	if (WITHOUT_TCOEFF(AE_zero)) {
		/* no MVs and very small TCOEFFs */
		/* do not transmit current MB */
		RETURN_MTYPE(MB_NOT_TRANSMIT);
	}

	/* obtain AE by previous MVDH and MVDV */
	/* the probability that (MVD of current SB is MVD of SB in
	 * the same position in previous frame) is high */
	MVDH = MVDH_frame[nMB];
	MVDV = MVDV_frame[nMB];
	if ((MVDH!=0) || (MVDV!=0)) {
		/* SB at the same position in last frame is INTER+MC */
		pmem->memloc += (((MVDV>=0) ? YMVDV_memloc[MVDV] :
						-YMVDV_memloc[-MVDV]) + MVDH);
		AE_best = absolute_error_SB_shortcut(pmem, cmem, AE_zero);
		if (AE_best>=AE_zero) {
			/* (0, 0) is better than (MVDH, MVDV) */
			MVDV = MVDH = 0;
		}
	}

	/* if previous MV is good enough, we can skip motion estimation */
	if (AE_best>AE_TCOEFF_THRESHOLD) {
		/* obtain MVD */
		/* MV start from (0, 0) */
		CurrentX = GOB_posX[Current_GOB] + MB_posX[Current_MB];
		CurrentY = GOB_posY[Current_GOB] + MB_posY[Current_MB];

		/* choose ME algorithms */
		AE_best = use_me_algo(pmem, cmem);
	}

	/* forced intra */
	if (*Last_update_ptr+(AE_best>>3)>131) RETURN_MTYPE(INTRA);

	/* set MTYPE by AE_best */
	if (WITHOUT_TCOEFF(AE_best)) {
		/* Inter+MC (without TCOEFFs) */
		RETURN_MTYPE_MV(INTER_MC);
	}

	/* set MTYPE only by AE_best and AE_zero */
	if (AE_best<=AE_INTER_THRESHOLD) {
		/* Inter [+MC] (with TCOEFFs) */

		/* check to use MC or not */
		/* prefer to choose AE_zero */
		if (USE_AE_zero(AE_zero, AE_best)) {
			/* Inter (without MC) */
			/* AE_zero is not so bad than AE_best */
			RETURN_MTYPE(INTER);
		}

		/* Inter + MC (with TCOEFFs)*/
		RETURN_MTYPE_MV(INTER_MC_TCOEFF);
	}

	/* Intra */
	RETURN_MTYPE(INTRA);
}

/*************************************************************************
 *
 *	Name:		absoulte_error_SB()
 *	Description:	obatin AE (absoult error) of two super-blocks in
 *			previous and current Y frames
 *	Input:		the pointers to the current and previous Y frames
 *	Return:	       	absoult error of two super-blocks
 *	Side effects:
 *	Date: 96/04/16	Author: Chu Ching-Wen in N.T.H.U., Taiwan
 *
 *************************************************************************/
/* NOTE: pick 64 points only
 *  	*---*---*---*---
 *  	--*---*---*---*-
 *  	*---*---*---*---
 *  	--*---*---*---*-
 *  	*---*---*---*---
 * 	.....
 */
static int16 absolute_error_SB(MEM *preBLK, MEM *curBLK)
{
	DEBUG("absolute_error_SB");
	register int16 i, data, ae, width;
	register unsigned char *p_ptr, *c_ptr;

	width = preBLK->width;
	p_ptr = preBLK->data + preBLK->memloc;
	c_ptr = curBLK->data + curBLK->memloc;
	for (ae=i=0; i<8; i++) {
		/* 1st line sample points: 0, 4, 8, 12 */
		ae += (	abs(*c_ptr - *p_ptr) +
			abs(*(c_ptr+4) - *(p_ptr+4)) +
			abs(*(c_ptr+8) - *(p_ptr+8)) +
			abs(*(c_ptr+12) - *(p_ptr+12)) );
		c_ptr += width;
		p_ptr += width;

		/* 2nd line sample points: 2, 6, 10, 14 */
		ae += (	abs(*(c_ptr+2) - *(p_ptr+2)) +
			abs(*(c_ptr+6) - *(p_ptr+6)) +
			abs(*(c_ptr+10) - *(p_ptr+10)) +
			abs(*(c_ptr+14) - *(p_ptr+14)) );
		c_ptr += width;
		p_ptr += width;
	}
	return ae;
}

/*************************************************************************
 *
 *	Name:		absoulte_error_SB_shortcut()
 *	Description:	obatin AE (absoult error) of two super-blocks in
 *			previous and current Y frames
 *	Input:		the pointers to the current and previous Y frames,
 *			the bound at present (upper-bound)
 *	Return:	       	absoult error of two super-blocks (may not be
 *			complete due to the shortcut)
 *	Side effects:
 *	Date: 96/04/16	Author: Chu Ching-Wen in N.T.H.U., Taiwan
 *
 *************************************************************************/
/* NOTE: pick 64 points only
 *  	*---*---*---*---
 *  	--*---*---*---*-
 *  	*---*---*---*---
 *  	--*---*---*---*-
 *  	*---*---*---*---
 * 	.....
 */
static int16 absolute_error_SB_shortcut(MEM *preBLK, MEM *curBLK, int16 bound)
{
	DEBUG("absolute_error_SB_shortcut");
	register i, data, ae, width;
	register unsigned char *p_ptr, *c_ptr;

	width = preBLK->width;
	p_ptr = preBLK->data + preBLK->memloc;
	c_ptr = curBLK->data + curBLK->memloc;
	for (ae=i=0; i<8; i++) {
		/* 1st line sample points: 0, 4, 8, 12 */
		ae += (	abs(*c_ptr - *p_ptr) +
			abs(*(c_ptr+4) - *(p_ptr+4)) +
			abs(*(c_ptr+8) - *(p_ptr+8)) +
			abs(*(c_ptr+12) - *(p_ptr+12)) );
		if (ae>=bound) return ae;	/* shortcut check */
		c_ptr += width;
		p_ptr += width;

		/* 2nd line sample points: 2, 6, 10, 14 */
		ae += (	abs(*(c_ptr+2) - *(p_ptr+2)) +
			abs(*(c_ptr+6) - *(p_ptr+6)) +
			abs(*(c_ptr+10) - *(p_ptr+10)) +
			abs(*(c_ptr+14) - *(p_ptr+14)) );
		if (ae>=bound) return ae;	/* shortcut check */
		c_ptr += width;
		p_ptr += width;
	}

	return ae;
}

/* DX and DY are positive */
#define P_P(DX, DY) {\
	y = pre_y + (DY);\
	if (y<=max_y) {\
		x = pre_x + (DX);\
		if (x<=max_x) {\
			preBLK->memloc = y * (Image->width[_Y]) + x;\
			ae = absolute_error_SB_shortcut(preBLK, curBLK, AE);\
			if (ae<AE) {\
				AE = ae;\
				new_x = x;\
				new_y = y;\
			}\
		}\
	}}
/* DX is positive and DY is negative */
#define P_N(DX, DY) {\
	y = pre_y + (DY);\
	if (y>=0) {\
		x = pre_x + (DX);\
		if (x<=max_x) {\
			preBLK->memloc = y * (Image->width[_Y]) + x;\
			ae = absolute_error_SB_shortcut(preBLK, curBLK, AE);\
			if (ae<AE) {\
				AE = ae;\
				new_x = x;\
				new_y = y;\
			}\
		}\
	}}
/* DX is negative and DY is positive */
#define N_P(DX, DY) {\
	y = pre_y + (DY);\
	if (y<=max_y) {\
		x = pre_x + (DX);\
		if (x>=0) {\
			preBLK->memloc = y * (Image->width[_Y]) + x;\
			ae = absolute_error_SB_shortcut(preBLK, curBLK, AE);\
			if (ae<AE) {\
				AE = ae;\
				new_x = x;\
				new_y = y;\
			}\
		}\
	}}
/* DX is negative and DY is negative */
#define N_N(DX, DY) {\
	y = pre_y + (DY);\
	if (y>=0) {\
		x = pre_x + (DX);\
		if (x>=0) {\
			preBLK->memloc = y * (Image->width[_Y]) + x;\
			ae = absolute_error_SB_shortcut(preBLK, curBLK, AE);\
			if (ae<AE) {\
				AE = ae;\
				new_x = x;\
				new_y = y;\
			}\
		}\
	}}

/*************************************************************************
 *
 *	Name:	   	full_search_ME()
 *	Description:	apply full search algorithm to obtain AE and MV
 *	Input:		the pointers to the current and previous Y frames
 *	Return:		the found minimum absoult error
 *	Side effects:	MVDH and MVDV may be updated
 *	Date: 96/04/16	Author: Chu Ching-Wen in N.T.H.U., Taiwan
 *
 *************************************************************************/
/* full search:
 * search area: (0, 0) +-15
 * AE_best and (MVDH, MVDV) can be used
 */
int16 full_search_ME(MEM *preBLK, MEM *curBLK)
{
	DEBUG("full_search_ME");
	int16 ae, AE;
	int16 new_x, new_y, pre_x, pre_y;
	int16 x, y, max_x, max_y;
	int16 dx, dy;

	/* x range of the upper-left point: 0, ..., max_x */
	/* y range of the upper-left point: 0, ..., max_y */
	max_x = preBLK->width - 16;
	max_y = preBLK->height - 16;

	AE = AE_best;
	new_x = CurrentX + MVDH;
	new_y = CurrentY + MVDV;

	/* MV start from (0, 0) */
	pre_x = CurrentX;
	pre_y = CurrentY;

	for (dx=1; dx<=15; dx++) {
		/* (x, y) = (+, +) */
		for (dy=0; dy<=15; dy++)
			P_P(dx, dy);

		/* (x, y) = (+, -) */
		for (dy=-1; dy>=-15; dy--)
			P_N(dx, dy);
	}

	for (dx=0; dx>=-15; dx--) {
		/* (x, y) = (-, +) */
		for (dy=0; dy<=15; dy++)
			N_P(dx, dy);

		/* (x, y) = (-, -) */
		for (dy=-1; dy>=-15; dy--)
			N_N(dx, dy);
	}

	MVDH = new_x - CurrentX;
	MVDV = new_y - CurrentY;

	return AE;
}

/* search the 8-points around (pre_x, pre_y) */
#define SEARCH_8_POINTS(WIDTH) {\
	P_P(     0,  WIDTH);\
	P_P( WIDTH,      0);\
	P_N(     0, -WIDTH);\
	N_P(-WIDTH,      0);\
	P_P( WIDTH,  WIDTH);\
	P_N( WIDTH, -WIDTH);\
	N_P(-WIDTH,  WIDTH);\
	N_N(-WIDTH, -WIDTH);\
	}

/*************************************************************************
 *
 *	Name:	   	three_step_search_ME()
 *	Description:	apply 3-step search algorithm to obtain AE and MV
 *	Input:		the pointers to the current and previous Y frames
 *	Return:		the found minimum absoult error
 *	Side effects:	MVDH and MVDV may be updated
 *	Date: 96/04/16	Author: Chu Ching-Wen in N.T.H.U., Taiwan
 *
 *************************************************************************/
/* 3-step search:
 * search area: (0, 0) +-15
 * AE_best and (MVDH, MVDV) are NOT used
 */
int16 three_step_search_ME(MEM *preBLK, MEM *curBLK)
{
	DEBUG("three_step_search_ME");
	int16 ae, AE;
	int16 new_x, new_y, pre_x, pre_y;
	int16 x, y, max_x, max_y;

	/* x range of the upper-left point: 0, ..., max_x */
	/* y range of the upper-left point: 0, ..., max_y */
	max_x = preBLK->width - 16;
	max_y = preBLK->height - 16;

	/* MV start from (0, 0) */
	AE = AE_zero;
	new_x = pre_x = CurrentX;
	new_y = pre_y = CurrentY;

	/* width = 8 */
	SEARCH_8_POINTS(8);
	pre_x = new_x;
	pre_y = new_y;

	/* width = 4 */
	SEARCH_8_POINTS(4);
	pre_x = new_x;
	pre_y = new_y;

	/* width = 2 */
	SEARCH_8_POINTS(2);
	pre_x = new_x;
	pre_y = new_y;

	/* width = 1 */
	SEARCH_8_POINTS(1);

	MVDH = new_x - CurrentX;
	MVDV = new_y - CurrentY;

	return AE;
}

/*************************************************************************
 *
 *	Name:	   	new_three_step_search_ME()
 *	Description:	apply new 3-step search algorithm to obtain AE and MV
 *	Input:		the pointers to the current and previous Y frames
 *	Return:		the found minimum absoult error
 *	Side effects:	MVDH and MVDV may be updated
 *	Date: 96/04/16	Author: Chu Ching-Wen in N.T.H.U., Taiwan
 *
 *************************************************************************/
/* (our) new 3-step search:
 * search area: (MVDH, MVDV) +-(15 - MAX(abs(MVDH), abs(MVDV)))
 * AE_best and (MVDH, MVDV) are used
 */
int16 new_three_step_search_ME(MEM *preBLK, MEM *curBLK)
{
	DEBUG("new_three_step_search_ME");
	int16 ae, AE;
	int16 new_x, new_y, pre_x, pre_y;
	int16 x, y, max_x, max_y;
	int16 mvdh, mvdv;
	int16 search_area;

	/* x range of the upper-left point: 0, ..., max_x */
	/* y range of the upper-left point: 0, ..., max_y */
	max_x = preBLK->width - 16;
	max_y = preBLK->height - 16;

	/* search area = 15 - MAX(abs(MVDH), abs(MVDV)) */
	/* the obtained MVDH and MVDV must be in +-15 */
	mvdh = abs(MVDH);
	mvdv = abs(MVDV);
	search_area = 15 - ((mvdh>mvdv) ? mvdh : mvdv);

	/* MV start from (MVDH, MVDV) */
	AE = AE_best;
	new_x = pre_x = CurrentX + MVDH;
	new_y = pre_y = CurrentY + MVDV;

	/* width = 8 */
	if (search_area&0x08) {
		SEARCH_8_POINTS(8);
		pre_x = new_x;
		pre_y = new_y;
	}

	/* width = 4 */
	if (search_area&0x04) {
		SEARCH_8_POINTS(4);
		pre_x = new_x;
		pre_y = new_y;
	}

	/* width = 2 */
	if (search_area&0x02) {
		SEARCH_8_POINTS(2);
		pre_x = new_x;
		pre_y = new_y;
	}

	/* width = 1 */
	if (search_area&0x01) SEARCH_8_POINTS(1);

	MVDH = new_x - CurrentX;
	MVDV = new_y - CurrentY;

	return AE;
}

/* 3 or 5 checking points around (pre_x, pre_y) */
#define SEARCH_3_or_5_POINTS(MVDH, MVDV, WIDTH) {\
	switch (MVDH) {\
	case WIDTH:\
		switch (MVDV) {\
		case WIDTH: 	/* (WIDTH, WIDTH) */\
			P_N(WIDTH, -WIDTH);\
			P_P(WIDTH, 0);\
			P_P(WIDTH, WIDTH);\
			P_P(0, WIDTH);\
			N_P(-WIDTH, WIDTH);\
			break;\
		case 0:		/* (WIDTH, 0) */\
			P_N(WIDTH, -WIDTH);\
			P_P(WIDTH, 0);\
			P_P(WIDTH, WIDTH);\
			break;\
		default:	/* (WIDTH, -WIDTH) */\
			P_N(WIDTH, -WIDTH);\
			P_P(WIDTH, 0);\
			P_P(WIDTH, WIDTH);\
			P_N(0, -WIDTH);\
			N_N(-WIDTH, -WIDTH);\
		}\
		break;\
	case 0:\
		switch (MVDV) {\
		case WIDTH:		/* (0, WIDTH) */\
			P_P(WIDTH, WIDTH);\
			P_P(0, WIDTH);\
			N_P(-WIDTH, WIDTH);\
			break;\
		default:	/* (0, -WIDTH) */\
			P_N(WIDTH, -WIDTH);\
			P_N(0, -WIDTH);\
			N_N(-WIDTH, -WIDTH);\
		}\
		break;\
	default:	/* -WIDTH */\
		switch (MVDV) {\
		case WIDTH: 	/* (-WIDTH, WIDTH) */\
			N_N(-WIDTH, -WIDTH);\
			N_P(-WIDTH, 0);\
			N_P(-WIDTH, WIDTH);\
			P_P(0, WIDTH);\
			P_P(WIDTH, WIDTH);\
			break;\
		case 0:		/* (-WIDTH, 0) */\
			N_N(-WIDTH, -WIDTH);\
			N_P(-WIDTH, 0);\
			N_P(-WIDTH, WIDTH);\
			break;\
		default:	/* (-WIDTH, -WIDTH) */\
			N_N(-WIDTH, -WIDTH);\
			N_P(-WIDTH, 0);\
			N_P(-WIDTH, WIDTH);\
			P_N(0, -WIDTH);\
			P_N(WIDTH, -WIDTH);\
		}\
	}\
}

/*************************************************************************
 *
 *	Name:	   	NTSS_ME()
 *	Description:	apply NTSS (new three-step search) algorithm in
 *			[1] to obtain AE and MV
 *			NOTE: half-pixel-based version
 *	Input:		the pointers to the current and previous Y frames
 *	Return:		the found minimum absoult error
 *	Side effects:	MVDH and MVDV may be updated
 *	Date: 97/11/25	Author: Ching-Wen Chu in N.T.H.U., Taiwan
 *
 *************************************************************************/
/* [1] R. Li, B, Zeng, and M. L. Liou, "A new three-step search algorithm
 *     for block motion estimation," IEEE Trans. Circuits Syst. Video Tech.,
 *     vol. 4, pp. 438-442, Aug. 1994.
 */
/* half-pixel-based NTSS:
 * search area: (0, 0) +-15
 * AE_best and (MVDH, MVDV) are NOT used
 */
int16 NTSS_ME(MEM *preBLK, MEM *curBLK)
{
	DEBUG("NTSS_ME");
	int16 ae, AE;
	int16 new_x, new_y, pre_x, pre_y;
	int16 x, y, max_x, max_y;

	/* x range of the upper-left point: 0, ..., max_x */
	/* y range of the upper-left point: 0, ..., max_y */
	max_x = preBLK->width - 16;
	max_y = preBLK->height - 16;

	/* MV start from (0, 0) */
	AE = AE_zero;
	new_x = pre_x = CurrentX;
	new_y = pre_y = CurrentY;

	/* width = 8 */
	SEARCH_8_POINTS(1);	/* 8 extra search points (neighbors) */
	SEARCH_8_POINTS(8);
	MVDH = new_x - CurrentX;
	MVDV = new_y - CurrentY;

	/* decision #1 */
	if (MVDH!=0 || MVDV!=0) {
		pre_x = new_x;
		pre_y = new_y;

		/* decision #2 */
		if (abs(MVDH)>1 || abs(MVDV)>1) {
			/* same as in TSS */

			/* width = 4 */
			SEARCH_8_POINTS(4);
			pre_x = new_x;
			pre_y = new_y;

			/* width = 2 */
			SEARCH_8_POINTS(2);
			pre_x = new_x;
			pre_y = new_y;

			/* width = 1 */
			SEARCH_8_POINTS(1);
		} else {
			/* 3 or 5 checking points */
			SEARCH_3_or_5_POINTS(MVDH, MVDV, 1);
		}
		MVDH = new_x - CurrentX;
		MVDV = new_y - CurrentY;
	}

	return AE;
}

/*************************************************************************
 *
 *	Name:	   	my_search_ME()
 *	Description:	apply my search algorithm to obtain AE and MV
 *	Input:		the pointers to the current and previous Y frames
 *	Return:		the found minimum absolute error
 *	Side effects:	MVDH and MVDV may be updated
 *
 *************************************************************************/
/* my search:
 * search area: (0, 0) +-15
 * AE_best and (MVDH, MVDV) can be used
 */
int16 my_search_ME(MEM *preBLK, MEM *curBLK)
{
	DEBUG("my_search_ME");
	int16 ae, AE;
	int16 new_x, new_y, pre_x, pre_y;
	int16 x, y, max_x, max_y;
	int16 dx, dy;

	/* x range of the upper-left point: 0, ..., max_x */
	/* y range of the upper-left point: 0, ..., max_y */
	max_x = preBLK->width - 16;
	max_y = preBLK->height - 16;

	AE = AE_best;
	new_x = CurrentX + MVDH;
	new_y = CurrentY + MVDV;

	/* MV start from (0, 0) */
	pre_x = CurrentX;
	pre_y = CurrentY;

	for (dx=1; dx<=15; dx++) {
		/* (x, y) = (+, +) */
		for (dy=0; dy<=15; dy++) {
			if ((dx + dy + 15) % 2 == 0)
				P_P(dx, dy);
		}

		/* (x, y) = (+, -) */
		for (dy=-1; dy>=-15; dy--) {
			P_N(dx, dy);
		}
	}

	for (dx=0; dx>=-15; dx--) {
		/* (x, y) = (-, +) */
		for (dy=0; dy<=15; dy++) {
			if ((dx + dy + 15) % 2 == 0)
				N_P(dx, dy);
		}

		/* (x, y) = (-, -) */
		for (dy=-1; dy>=-15; dy--) {
			if ((dx + dy + 15) % 2 == 0)
				N_N(dx, dy);
		}
	}

	MVDH = new_x - CurrentX;
	MVDV = new_y - CurrentY;

	return AE;
}