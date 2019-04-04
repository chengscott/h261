/*************************************************************************
 *
 *	Name:		thresh.h
 *	Description:	threshold values definitions (for h261.c, me.c and
 *			codec.c)
 *	Date: 96/05/09	Author: Chu Ching-Wen in N.T.H.U., Taiwan
 *
 *************************************************************************/

#ifndef THRESH_DONE
#define THRESH_DONE

/*************************************************************************/
/* in h261.c (for encoder only) */
/* default quantization step-size for TCOEFF of intra (ac) and inter
 * (Range: stepsize: 2... 62, quantizer: 1... 31)
 */
#define DEFAULT_QUANTIZER 4

/*************************************************************************/
/* in codec.c */
/* DCT coefficient thresholds for coding blocks
 * if (sum of the abs(TCOEFFs) in this block) > CBP_THRESHOLD
 *		=> this block should be transmitted
 * CBP_THRESHOLD is smaller => better image-qulity but more bits generated
 */
#define CBP_THRESHOLD  3    	/* abs threshold before we use CBP */

/*************************************************************************/
/* in me.c */
/* threshold for setting MTYPE */
/* 1. with or without TCOEFF: compared with AE */
	/* if (WITHOUT_TCOEFF(AE)) => no TCOEFFs
	 *	CASE 1:  (AE==AE_zero) 	=> MB not trans.
	 *	CASE 2:  (AE==AE_best) 	=> MTYPE = INTER_MC
	 */
	#define AE_TCOEFF_THRESHOLD 100
		/* 16 points (within 64 picked points) changed
		 * within the limit 100
		 */
	#define WITHOUT_TCOEFF(ae) ((ae) <= AE_TCOEFF_THRESHOLD)

/* 2. inter[+MC]+TCOEFF or intra: compared with AE */
	#define AE_INTER_THRESHOLD ((int16) (650+AE_TCOEFF_THRESHOLD))
			/* if (AE<=%d)	=> inter[+MC]+TCOEFF
			 * else      	=> intra
			 */

/* 3. inter with or without MC: compared with AE */
	/* if (USE_AE_zero(AE_zero, AE_best))
	 *		=> inter+TCOEFF		(MTYPE = INTER)
	 * else		=> inter+MC+TCOEFF	(MTYPE = INTER_MC)
	 */
	#define USE_AE_zero(ae_zero, ae_best) \
		((ae_zero) <= (ae_best) + (ae_best>>3))
        /* (ae_zero) <= (ae_best)*1.125 */
#endif

