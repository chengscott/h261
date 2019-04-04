/*************************************************************************
 *
 *	Name:		codec.c
 *	Description:	basic coding/decoding operations (quantization etc)
 *	Date: 96/04/19	Author: Chu Ching-Wen in N.T.H.U., Taiwan
 *
 *************************************************************************/

#include <stdarg.h>
#include "globals.h"
#include "mytime.h"     /* TIME-type variables definition & function */
#include "thresh.h"	/* threshold values definition */

/*************************************************************************/
/* public */
/* for encoder */
extern boolean quantize(boolean intra_used, int16 *block, int16 quantizer);
extern void transfer_TCOEFF(boolean intra_used, int16 *block);
/* for decoder */
extern void Iquantize(boolean intra_used, int16 *block, int16 quantizer);
extern void Itransfer_TCOEFF(boolean intra_used, int16 *block);
extern void clip_reconstructed_block(int16 *block);

extern DHUFF *T1_Dhuff;
extern DHUFF *T2_Dhuff;
extern EHUFF *T1_Ehuff;
extern EHUFF *T2_Ehuff;

/*************************************************************************/
/* private */
/* for zig-zag scan */
static int16 zigzag_index[] =
	{0,  1,  8, 16,  9,  2,  3, 10,
	17, 24, 32, 25, 18, 11,  4,  5,
	12, 19, 26, 33, 40, 48, 41, 34,
	27, 20, 13,  6,  7, 14, 21, 28,
	35, 42, 49, 56, 57, 50, 43, 36,
	29, 22, 15, 23, 30, 37, 44, 51,
	58, 59, 52, 45, 38, 31, 39, 46,
	53, 60, 61, 54, 47, 55, 62, 63};

/* bound n in the range (min, max) */
#define BOUND(n, min, max) {\
	if ((n)<(min))	(n) = (min);\
	else if ((n)>(max))	(n) = (max);\
	}

/*************************************************************************
 *
 *	Name:		quantize()
 *	Description:	quantize block with step-size (2 * quantizer)
 *	Input:          the block to be quantized, boolean to indicate intra
 *			block, and the quantizer
 *	Return:		TRUE to indicate this block should be transmitted,
 *			FALSE to indicate otherwise
 *	Side effects:	entries of the block will be changed (quantized)
 *	Date: 96/04/29	Author: Chu Ching-Wen in N.T.H.U., Taiwan
 *
 *************************************************************************/
boolean quantize(boolean intra_used, int16 *block, int16 quantizer)
{
	DEBUG("quantize");
	va_list argu_ptr;
	int16 *mptr;
	int16 sum;
	int16 stepsize = quantizer << 1;

	#if (CTRL_GET_TIME==GET_ALL_TIME)
	get_time(tQUAN1);
	#endif

	if (intra_used) {
		/* dc term for intra */
		*block = ((*block<0) ? (*block+3) : (*block+4)) >> 3;
/*		*block = ((*block<0) ? (*block-4) : (*block+4)) / 8; */

		/* ac terms for intra */
		mptr = block + 1;
		for (; mptr<block+BLOCKSIZE; mptr++) {
			if (*mptr>0)
				*mptr = (*mptr + 1) / stepsize;
			else if (*mptr<0)
				*mptr = (*mptr - 1) / stepsize;
		}
		#if (CTRL_GET_TIME==GET_ALL_TIME)
		get_time(tQUAN1);
		#endif

		return TRUE;	/* always return TRUE for intra block */
	} else {
		/* dc_quantizer == ac_quantizer == quantizer */
		mptr = block;
		if (quantizer&1) {	/* Odd */
			for (sum=0; mptr<block+BLOCKSIZE; mptr++) {
				sum += abs(*mptr /= stepsize);
			}
		} else {		/* Even */
			for (sum=0; mptr<block+BLOCKSIZE; mptr++) {
				if (*mptr>0) {
					*mptr = (*mptr + 1) / stepsize;
					sum += (*mptr);
				} else if (*mptr<0) {
					*mptr = (*mptr - 1) / stepsize;
					sum -= (*mptr);
				}
			}
		}

		#if (CTRL_GET_TIME==GET_ALL_TIME)
		get_time(tQUAN1);
		#endif

		return (sum>CBP_THRESHOLD);
	}
}

/*************************************************************************
 *
 *	Name:		tramsfer_TCOEFF()
 *	Description:	transfer TCOEFF (block) out to the bitstream
 * 			according to suitable huffman (VLC) table
 *	Input:          the block to be transfered, boolean to indicate
 *			intra block
 *	Return:		none
 *	Side effects:	some entries of the block may be changed (cliped)
 *	Date: 96/04/16	Author: Chu Ching-Wen in N.T.H.U., Taiwan
 *
 *************************************************************************/
void transfer_TCOEFF(boolean intra_used, int16 *block)
{
	DEBUG("transfer_TCOEFF");
	int16 run;	/* (run) # of Zero term between two NonZero terms */
	int16 *level;	/* (*level)  a NonZero term */
	int16 code;	/* code = [run|abs(*level)]
				where run is in the right 6-bits,
				  and abs(*level) is in the left 8-bits */
	int16 i;

	if (intra_used) {
		/* dc term : always put 8 bits for that */
		BOUND(*block, 1, 254);
		code = ((*block==128) ? 255 : *block);
		put_n_bits(8, (int32) code);
		i = 1;
	} else {
		/* there exists a non-zero coefficient in the block,
		 * thus the EOB cannot occur as the first element and
		 * we can use T2 enocde-huffman-table for it
		 */
		/* find the first term and use T2 Encode-Huffman-table */
		/* *level is a Zero term */
		for (i=run=0; *(level=block+zigzag_index[i++])==0; run++);

		/* *level is a NonZero term */
		BOUND(*level, -127, 127);
		code = abs(*level) | (run << 8);
		if ((code==ESCAPE) || (!put_VLC(code, T2_Ehuff))) {
			/* not in T2 Encode-Huffman-table */
			put_VLC(ESCAPE, T2_Ehuff);
			put_n_bits(6, (int32) run);
			put_n_bits(8, (int32) *level);
		} else {	/* in T2 */
			/* put the sign-bit to the output stream */
			put_n_bits(1, *level<0);
		}
	}

	/* find other terms in T1 Encode-Huffman-table */
	for (run=0; i<BLOCKSIZE;) {
		if (*(level=block+zigzag_index[i++])==0) {
			/* *level is Zero term */
			run++;
		} else {
			/* *level is NonZero term */
			BOUND(*level, -127, 127);
			code = abs(*level) | (run << 8);
			if ((code==ESCAPE) || (!put_VLC(code, T1_Ehuff))) {
				/* not in T1 Encode-Huffman-table */
				put_VLC(ESCAPE, T1_Ehuff);
				put_n_bits(6, (int32) run);
				put_n_bits(8, (int32) *level);
			} else {	/* in T1 */
				/* put the sign-bit to the output stream */
				put_n_bits(1, *level<0);
			}
			run = 0;
		}
	}
	put_VLC(EOB, T1_Ehuff);	/* add EOB finally */
}

/*************************************************************************
 *
 *	Name:		Iquantize()
 *	Description:	inverse-quantize block with step-size (2 * quantizer)
 *	Input:          the block to be inverse-quantized, boolean to
 *			indicate intra block
 *	Return:		none
 *	Side effects:	entries of the block will be inverse-quantized
 *	Date: 96/04/29	Author: Chu Ching-Wen in N.T.H.U., Taiwan
 *
 *************************************************************************/
void Iquantize(boolean intra_used, int16 *block, int16 quantizer)
{
	DEBUG("Iquantize");
	va_list argu_ptr;
	int16 *mptr;

	#if (CTRL_GET_TIME==GET_ALL_TIME)
	get_time(tIQUAN1);
	#endif

	if (intra_used) {
		/* dc term for intra */
		*block = (*block << 3);     	/* * 8 */
		mptr = block + 1;
	} else {
		/* dc_stepsize == ac_stepsize == 2 * quantizer */
		mptr = block;
	}

	for (; mptr<block+BLOCKSIZE; mptr++) {
		if (*mptr > 0)
			*mptr = (((*mptr<<1) + 1) * quantizer) - 1;
		else if (*mptr < 0)
			*mptr = (((*mptr<<1) - 1) * quantizer) + 1;
	}

	#if (CTRL_GET_TIME==GET_ALL_TIME)
	get_time(tIQUAN2);
	ntIQUAN++;
	ntTOTAL++;
	tIQUAN += diff_time(tIQUAN2, tIQUAN1);
	#endif
}

/*************************************************************************
 *
 *	Name:		Itramsfer_TCOEFF()
 *	Description:	inverse-transfer TCOEFF (block) from the bitstream
 * 			according to suitable huffman (VLD) table
 *	Input:          the block to be stored, boolean to indicate
 *			intra block
 *	Return:		none
 *	Side effects:	entries of the block will be changed
 *	Date: 96/04/16	Author: Chu Ching-Wen in N.T.H.U., Taiwan
 *
 *************************************************************************/
void Itransfer_TCOEFF(boolean intra_used, int16 *block)
{
	DEBUG("Itransfer_TCOEFF");
	int16 i, run, level;

	memset(block, 0, sizeof(int16)*BLOCKSIZE);

	if (intra_used) {
		/* dc term */
		level = (int16) get_n_bits(8);
		if (level==255) level = 128;
		*block = level;
		i = 1;
	} else {
		/* there exists a non-zero coefficient in the block,
		 * thus the EOB cannot occur as the first element and
		 * we can use T2 enocde-huffman-table for it
		 */
		/* find the first term in T2 Encode-Huffman-table */
		run = get_VLC(T2_Dhuff);
		#ifdef DEBUG_ON
		if (!run) {                  	/* find nothing (EOF) */
			ERROR_LINE();
			printf("Bad EOF in CBP block.\n");
			exit(ERROR_EOF);
		} else
		#endif
		if (run==ESCAPE) {
			run = (int16) get_n_bits(6);
			level = (int16) get_n_bits(8);
		} else {
			level = run & 0xFF;		/* the left 8-bits in run */
			run = run >> 8;			/* the right 6-bits in run */
			if (get_bit())	level = -level;	/* the sign-bit of level */
		}
		/* level<0 => fill sign bit in left un-used bits of level */
		if (level&0x80) level |= (int16) 0xFF00;

		i = run;
		block[zigzag_index[i++]] = level;
	}

	/* find other terms in T1 Encode-Huffman-table */
	while (i<BLOCKSIZE) {
		run = get_VLC(T1_Dhuff);
		if (!run) {                  	/* find nothing (EOF) */
			return;
		} else if (run==ESCAPE) {
			run = (int16) get_n_bits(6);
			level = (int16) get_n_bits(8);
		} else {
			level = run & 0xFF;	/* the left 8-bits in run */
			run = run >> 8;		/* the right 6-bits in run */
			if (get_bit()) level = -level;	/* the sign-bit of level */
		}
		/* level<0 => fill sign bit in left un-used bits of level */
		if (level&0x80) level |= (int16) 0xFF00;

		i += run;
		block[zigzag_index[i++]] = level;
	}

	/* find EOB */
	#ifdef DEBUG_ON
	if ((run=get_VLC(T1_Dhuff)) != EOB) {
		ERROR_LINE();
		printf("EOB lost, found 0x%x at run length %d.\n", run, i);
		exit(ERROR_EOB);
	}
	#else
	get_VLC(T1_Dhuff);
	#endif
}

/*************************************************************************
 *
 *	Name:		clip_reconstructed_block()
 *	Description:	clip the block so that no entry has a value
 *			greater than 255 or less than 0
 *	Input:          the block to be cliped
 *	Return:
 *	Side effects:	some entries of the block may be changed (cliped)
 *	Date: 96/04/16	Author: Chu Ching-Wen in N.T.H.U., Taiwan
 *
 *************************************************************************/
void clip_reconstructed_block(int16 *block)
{
	DEBUG("clip_reconstructed_frame");
	register int16 *mptr;

	for (mptr=block; mptr<block+BLOCKSIZE; mptr++)
		BOUND(*mptr, 0, 255);
}

