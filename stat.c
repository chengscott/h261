/*************************************************************************
 *
 *	Name:		stat.c
 *	Description:	obtain and show statistics
 *	Date: 96/05/11	Author: Chu Ching-Wen in N.T.H.U., Taiwan
 *
 *************************************************************************/

#include "globals.h"
#include "ctrl.h"	/* codec control: statistics, get time... */
#include "mytime.h"     /* TIME-type variables definition & function */
#include "thresh.h"	/* threshold values definition */

/*************************************************************************/
/* public */
extern void statistics(FSTORE *ref_fs, FSTORE *fs);
extern void print_time(void);
extern void print_codec_info(boolean decoder);
extern void print_frame_info(int32 Current_frame);
extern void print_sequence_info(boolean decoder);
extern double get_time_cost(void);

extern IMAGE *Image;		/* global info. of image */

/*************************************************************************/
/* private */
static double psnr(MEM *ref_mem, MEM *mem);

#ifdef CTRL_STAT_MTYPE
static int32 total_MTYPE_count[11] = {0,0,0,0,0,0,0,0,0,0,0};
static int32 total_nMTYPE_mc = 0;
static int32 total_nMTYPE_not = 0;
#endif

#ifdef CTRL_PSNR
static double psnr_y, psnr_cb, psnr_cr;
static double sum_psnr[NUMBER_OF_COMPONENTS] = {0, 0, 0};
#endif

/*************************************************************************
 *
 *	Name:		statistics()
 *	Description:	print the psnr for Y, Cb and Cr of current frame
 *	Input:		the pointers to the reference and current frames
 *	Return:	       	none
 *	Side effects:
 *	Date: 96/04/16	Author: Chu Ching-Wen in N.T.H.U., Taiwan
 *
 *************************************************************************/
void statistics(FSTORE *ref_fs, FSTORE *fs)
{
	DEBUG("statistics");

	#ifdef CTRL_PSNR
	sum_psnr[_Y] += (psnr_y = psnr(ref_fs->fs[_Y], fs->fs[_Y]));
	sum_psnr[_Cb] += (psnr_cb = psnr(ref_fs->fs[_Cb], fs->fs[_Cb]));
	sum_psnr[_Cr] += (psnr_cr = psnr(ref_fs->fs[_Cr], fs->fs[_Cr]));
	printf("Y-PSNR: %f\n", psnr_y);
	#endif
}

/*************************************************************************
 *
 *	Name:		psnr()
 *	Description:	obtain the psnr of the MEM structure
 *	Input:		the pointers to the reference and current MEM
 *			structure
 *	Return:	       	the psnr
 *	Side effects:
 *	Date: 96/04/16	Author: Chu Ching-Wen in N.T.H.U., Taiwan
 *
 *************************************************************************/
static double psnr(MEM *ref_mem, MEM *mem)
{
	DEBUG("psnr");
	int32 i, n;
	double squared;
	unsigned char *cptr, *rptr;

	n = mem->width * mem->height;
	squared = 0.0;
	for (i=0,rptr=ref_mem->data,cptr=mem->data; i<n; i++,rptr++,cptr++)
		squared += (double) (((*cptr)-*(rptr)) * ((*cptr)-(*rptr)));

	if (squared) {
		return (10 * log10( (65025.0 * (double) n) / squared));
	} else {
		return 99.99;
	}
}

#define ACTUAL_TIME(t, nt) ((double) TIME_UNIT * (((double) (t)) - (double) ((nt)*tTIME_COST)) )

/*************************************************************************
 *
 *	Name:		print_time()
 *	Description:	print the processing time (total or each parts)
 *	Input:		none
 *	Return:		none
 *	Side effects:
 *	Date: 96/04/16	Author: Chu Ching-Wen in N.T.H.U., Taiwan
 *
 *************************************************************************/
void print_time(void)
{
	DEBUG("print_time");

	#ifdef CTRL_GET_TIME
	printf("\tTotal time: %.2f sec\n", ACTUAL_TIME(tTOTAL, ntTOTAL));

	#if (CTRL_GET_TIME==GET_ALL_TIME)

	printf("\tME: %.2f sec\t", ACTUAL_TIME(tME, ntME));
	printf("DCT: %.2f sec\t", ACTUAL_TIME(tDCT,ntDCT));
	printf("QUAN: %.2f sec\n", ACTUAL_TIME(tQUAN, ntQUAN));

	printf("\tIDCT: %.2f sec\t", ACTUAL_TIME(tIDCT, ntIDCT));
	printf("IQUAN: %.2f sec\n", ACTUAL_TIME(tIQUAN, ntIQUAN));
	#endif

	#endif
}

/*************************************************************************
 *
 *	Name:		print_codec_info()
 *	Description:	print the chosen options and the threshold values
 *	Input:		boolean to indicate using decoder
 *	Return:		none
 *	Side effects:
 *	Date: 96/05/11	Author: Chu Ching-Wen in N.T.H.U., Taiwan
 *
 *************************************************************************/
void print_codec_info(boolean decoder)
{
	extern int32 Start_frame, End_frame;
	extern double Bit_rate, Frame_rate;

	if (decoder) {
		/* show info. of decoder */
		printf("Decode %s --> ", Image->Stream_filename);
		if (Image->write_to_files) {
			printf("%s%ld%s %s%ld%s %s%ld%s, .....\n",
				Image->output_frame_prefix,
				Start_frame, Image->frame_suffix[_Y],
				Image->output_frame_prefix,
				Start_frame, Image->frame_suffix[_Cb],
				Image->output_frame_prefix,
				Start_frame, Image->frame_suffix[_Cr]);
		} else {
			printf("display .....\n");
		}
	} else {
		/* show info. of encoder */
		printf("Encode %s <-- ", Image->Stream_filename);
		if (Image->read_from_files) {
			printf("%s%ld%s %s%ld%s %s%ld%s\n",
				Image->input_frame_prefix,
				Start_frame, Image->frame_suffix[_Y],
				Image->input_frame_prefix,
				Start_frame, Image->frame_suffix[_Cb],
				Image->input_frame_prefix,
				Start_frame, Image->frame_suffix[_Cr]);
			if (Start_frame!=End_frame) {
				printf("\t\t\t....., %s%ld%s %s%ld%s %s%ld%s\n",
					Image->input_frame_prefix,
					End_frame, Image->frame_suffix[_Y],
					Image->input_frame_prefix,
					End_frame, Image->frame_suffix[_Cb],
					Image->input_frame_prefix,
					End_frame, Image->frame_suffix[_Cr]);
			}
		} else {
			printf("captured frames\n");
		}

		#ifdef CTRL_SHOW_THRESH
		/* print threshold values */
		printf("\tThreshold values :\n");
		printf("\t\tCBP_THRESHOLD: %d\n", CBP_THRESHOLD);
		printf("\t\tAE_TCOEFF_THRESHOLD: %d\n", AE_TCOEFF_THRESHOLD);
		printf("\t\tAE_INTER_THRESHOLD:  %d\n", AE_INTER_THRESHOLD);
		printf("\t\tDEFAULT_QUANTIZER: %d\n", DEFAULT_QUANTIZER);
		#endif

		/* print target bit rate and frame rate*/
		printf("\tBit rate   (target): %.2f kbps\n", Bit_rate/1000);
		printf("\tFrame rate (target): %.2f fps\n", Frame_rate);
		printf("\tCompression rate (target): %.4f bit/pixel\n",
			Bit_rate / (Frame_rate*Image->Height*Image->Width));
	}
}

/*************************************************************************
 *
 *	Name:		print_frame_info()
 *	Description:	print info. of coding/decoding current frame
 *			after codec process
 *	Input:		current frame ID
 *	Return:		none
 *	Side effects:	counters of MTYPE will be updated
 *	Date: 96/04/16	Author: Chu Ching-Wen in N.T.H.U., Taiwan
 *
 *************************************************************************/
void print_frame_info(int32 Current_frame)
{
	DEBUG("print_frame_info");
	#ifdef CTRL_STAT_MTYPE
	extern int32 MTYPE_count[], nMTYPE_mc, nMTYPE_not;
	int16 i;
	#endif
	extern int32 Start_frame, Total_bits;
	static int32 last_bits = 0;

	/* show bits for frame */
	Total_bits = ftell_write_stream();
	printf("Frame %ld:\t%ld bits\n", Current_frame,
		Total_bits - last_bits);
	last_bits = Total_bits;

	/* show MTYPE info. */
	#ifdef CTRL_STAT_MTYPE
	printf("\tMTYPE:");
	for (i=0; i<11; i++) 	printf("%6d", i);
	printf("\n\t   MB:");
	for (i=0; i<11; i++) {
		printf("%6ld", MTYPE_count[i]);
		total_MTYPE_count[i] += MTYPE_count[i];
		MTYPE_count[i] = 0;
	}
	printf("\tMTYPE from %d to %d: %ld\t\tMTYPE from %d to %d: %ld\n\n",
		INTER_MC_TCOEFF, INTER_MC, nMTYPE_mc,
		INTER, MB_NOT_TRANSMIT, nMTYPE_not);
	total_nMTYPE_mc += nMTYPE_mc;
	total_nMTYPE_not += nMTYPE_not;
	nMTYPE_mc = nMTYPE_not = 0;
	#endif

	#ifdef CTRL_PSNR
	printf("\tPSNR: \t%2.2f (Y) \t%2.2f (Cb) \t%2.2f (Cr)\n",
		psnr_y, psnr_cb, psnr_cr);
	#endif
}

/*************************************************************************
 *
 *	Name:		print_sequence_info()
 *	Description:	print info. of coding/decoding all frames
 *			after codec process
 *	Input:		boolean to indicate using decoder
 *	Return:		none
 *	Side effects:	counters of MTYPE will be updated
 *	Date: 96/05/01	Author: Chu Ching-Wen in N.T.H.U., Taiwan
 *
 *************************************************************************/
void print_sequence_info(boolean decoder)
{
	DEBUG("print_sequence_info");
	#ifdef CTRL_HEADER_BITS
	extern int32 HeaderBits;
	#endif
	#ifdef CTRL_STAT_MTYPE
	extern int32 MTYPE_count[], nMTYPE_mc, nMTYPE_not;
	extern int16 Number_GOB, Number_MB;
	int16 i;
	#endif
	#ifdef CTRL_GET_TIME
	double atime;
	#endif
	extern int32 First_frame_bits, Total_bits;
	extern int32 Start_frame, End_frame, Number_frame, Frame_skip;
	extern double Bit_rate, Frame_rate;
	int32 number_frame, image_bits;

	printf("----------------------------------------\n");
	#ifdef CTRL_HEADER_BITS
	if (!decoder) printf("Bits for header: %ld bits\n", HeaderBits);
	#endif

	#ifdef CTRL_PSNR
	number_frame = (int32) ((Number_frame-1) / Frame_skip) + 1;
	if (!decoder) printf("Average PSNR of coded frames: \t%2.2f (Y) \t%2.2f (Cb) \t%2.2f (Cr)\n",
		sum_psnr[_Y] / number_frame, 
		sum_psnr[_Cb] / number_frame,
		sum_psnr[_Cr] / number_frame);
	#endif

	/* print frames info. excluding the first frame */
	printf("Bits for the first frame %ld: %ld\n", Start_frame,
		First_frame_bits);
	number_frame = Number_frame - 1;

	if (number_frame>0) {
		if (number_frame==1) {
			/* only one frame */
			printf("Frame %ld:\n", Start_frame+1);
		} else {
			/* info. for total frames (except the first one) */
			printf("Frames %ld to %ld (%ld frames):  [excluding the fitst frame: %ld]\n",
				Start_frame+1, End_frame, number_frame,
				First_frame_bits);
		}

		#ifdef CTRL_STAT_MTYPE

		/* sub. the 1st frame's intra MBs */
		MTYPE_count[INTRA] -= (Number_GOB * Number_MB);

		/* add. the skip frames' MBs in encoder */
		if (!decoder) {
			MTYPE_count[MB_NOT_TRANSMIT] +=
			(Number_GOB * Number_MB) *
			(number_frame - (int16) (number_frame/Frame_skip));
		}

		printf("\tMTYPE:");
		for (i=0; i<11; i++) 	printf("%6d", i);
		printf("\n\t   MB:");
		for (i=0; i<11; i++) {
			printf("%6ld", total_MTYPE_count[i]+=MTYPE_count[i]);
			MTYPE_count[i] = 0;
		}
		printf("\n");
		if (!decoder) {
			printf("\tMTYPE from %d to %d: %ld\t\tMTYPE from %d to %d: %ld\n\n",
				INTER_MC_TCOEFF, INTER_MC,
				total_nMTYPE_mc+=nMTYPE_mc,
				INTER, MB_NOT_TRANSMIT,
				total_nMTYPE_not+=nMTYPE_not);
		}

		#endif

		Total_bits -= First_frame_bits;
		printf("\tTotal bits: %ld\n", Total_bits);

		#ifdef CTRL_GET_TIME
		print_time();
		atime = ACTUAL_TIME(tTOTAL, ntTOTAL);
		printf("\tBit rate  : %.2f kbps   \t(%.2f kbps under %.2f fps)\n",
			(double) Total_bits / 1000 / atime,
			((double) Total_bits / number_frame) * Frame_rate / 1000,
			(double) Frame_rate);
		printf("\tFrame rate: %.2f fps    \t(%.2f fps under %.2f kbps)\n",
			(double) number_frame / atime,
			(double) number_frame * Bit_rate / Total_bits,
			(double) Bit_rate / 1000);
		#endif

		/* print compression rate */
		image_bits = (int32) (number_frame *
				Image->Height * Image->Width * 1.5 * 8);
		printf("\tCompression rate:   %.2f times\t(%ld / %ld)\n",
			(double) image_bits/Total_bits, image_bits, Total_bits);
		printf("\t                :   %.4f bit/piexl\n", 
			(double) Total_bits
			/ (number_frame * Image->Width * Image->Height));
	}

	printf("----------------------------------------\n");
}

/*************************************************************************
 *
 *	Name:		get_time_cost()
 *	Description:	obtain the time cost for one pair of get_time() call
 *	Input:		none
 *	Return:		the time for one pair of get_time() call
 *	Side effects:	counters of MTYPE will be updated
 *	Date: 96/04/16	Author: Chu Ching-Wen in N.T.H.U., Taiwan
 *
 *************************************************************************/
double get_time_cost(void)
{
	DEBUG("get_time_cost");

	int32 test_time = 1000;
	int32 i;

	tTOTAL = 0;
	for (i=0; i<test_time; i++) {
		get_time(tTOTAL1);
		get_time(tTOTAL2);
		tTOTAL += diff_time(tTOTAL2, tTOTAL1);
	}

	return ((double)tTOTAL / test_time);
}

