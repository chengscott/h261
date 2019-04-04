/*************************************************************************
 *
 *	Name:		h261.c
 *	Description:	read/write frame, GOB, and MB header
 *	Date: 96/04/16	Author: Chu Ching-Wen in N.T.H.U., Taiwan
 *
 *************************************************************************/

#include "globals.h"
#include "ctrl.h"	/* codec control: statistics, get time... */

/*************************************************************************/
/* public */
/* for encoder */
extern void write_frame_header(PIC_HEADER *header);
extern void write_GOB_header(GOB_HEADER *header);
extern void write_MB_header(int16 Current_MB, MB_HEADER *header);
/* for decoder */
extern void read_PSC(void);
extern void read_frame_header_tail(PIC_HEADER *header);
extern void read_GBSC(void);
extern int16 read_GN(void);
extern void read_GOB_header_tail(GOB_HEADER *header);
extern int16 read_MB_header(int16 *Current_MB, int16 GQUANT, MB_HEADER *header);

/*************************************************************************/
/* following extern variables are declared in h261.c */
extern boolean MQUANT_used[];
extern boolean CBP_used[];
extern boolean MVD_used[];

/*************************************************************************/
/* following extern variables are declared in huffman.c */
/* for decoder */
extern DHUFF *MBA_Dhuff;
extern DHUFF *MVD_Dhuff;
extern DHUFF *CBP_Dhuff;
extern DHUFF *MTYPE_Dhuff;
/* for encoder */
extern EHUFF *MBA_Ehuff;
extern EHUFF *MVD_Ehuff;
extern EHUFF *CBP_Ehuff;
extern EHUFF *MTYPE_Ehuff;

/*************************************************************************
 *
 *	Name:		write_frame_header()
 *	Description:	write the frame header for each frame
 *	Input:          the pointer to the frame header structure
 *	Return:		none
 *	Side effects:	HeaderBits will be updated if it is enable
 *	Date: 96/04/16	Author: Chu Ching-Wen in N.T.H.U., Taiwan
 *
 *************************************************************************/
void write_frame_header(PIC_HEADER *header)
{
	DEBUG("write_frame_header");
	#ifdef CTRL_HEADER_BITS
	extern int32 HeaderBits;
	#endif

	put_n_bits(PSC_LENGTH, (int32) PSC);	/* Picture Start Code (20) */

	#ifdef CTRL_HEADER_BITS
	HeaderBits += PSC_LENGTH;
	#endif

	put_n_bits(5, (int32) header->TR);	/* Temporal Reference (5) */

	put_n_bits(6, (int32) header->PTYPE);	/* Picture coding Type (6) */

	#ifdef CTRL_HEADER_BITS
	HeaderBits += 11;
	#endif

	if (header->PEI) {
		put_n_bits(1, 1);	/* PSPARE ID (prefix): '1' (1) */
		put_n_bits(8, (int32) header->PSPARE);	/* PSPARE (8) */

		#ifdef CTRL_HEADER_BITS
		HeaderBits += 9;
		#endif
	}

	/* We can write more info. for picture layer HERE. */
	/* Each 8-bits info. followed by '1' (1-bit). */
	/* ... */

	put_n_bits(1, 0);	/* End of extra info. for picture layer: '0' (1) */

	#ifdef CTRL_HEADER_BITS
	HeaderBits++;
	#endif
}

/*************************************************************************
 *
 *	Name:		write_GOB_header()
 *	Description:	write the group-of-block header
 *	Input:          the pointer to the GOB header structure
 *	Return:		none
 *	Side effects:	HeaderBits will be updated if it is enable
 *	Date: 96/04/16	Author: Chu Ching-Wen in N.T.H.U., Taiwan
 *
 *************************************************************************/
void write_GOB_header(GOB_HEADER *header)
{
	DEBUG("write_GOB_header");
	#ifdef CTRL_HEADER_BITS
	extern int32 HeaderBits;
	#endif

	/* Group of Block Start Code (16) */
	put_n_bits(GBSC_LENGTH, (int32) GBSC);

	#ifdef CTRL_HEADER_BITS
	HeaderBits += GBSC_LENGTH;
	#endif

	/* the following 4-bits must NOT be '0000' ( => +1 ) , because
	   PSC(20 bits) and {GBSC(16 bits)+this 4-bits} can't be the same */
	put_n_bits(4, (int32) header->GN);	/* Gob Number (4) */

	put_n_bits(5, (int32) header->GQUANT);

	#ifdef CTRL_HEADER_BITS
	HeaderBits += 9;
	#endif

	if (header->GEI) {
		put_n_bits(1, 1);		/* GSPARE ID (prefix): '1' (1) */
		put_n_bits(8, (int32) header->GSPARE);	/* GSPARE (8) */

		#ifdef CTRL_HEADER_BITS
		HeaderBits += 9;
		#endif
	}

	/* We can write more info. for GOB layer HERE. */
	/* Each 8-bits info. followed by '1' (1-bit). */
	/* ... */

	put_n_bits(1, 0);	/* End of extra info. for GOB layer: '0' (1) */

	#ifdef CTRL_HEADER_BITS
	HeaderBits++;
	#endif
}

/*************************************************************************
 *
 *	Name:		write_MB_header()
 *	Description:	write the macro-block header
 *	Input:          current MB ID and the pointer to the MB header
 *			structure
 *	Return:		none
 *	Side effects:	HeaderBits will be updated if it is enable,
 *			Last_MTYPE, Last_MVDH and Last_MVDV may be updated
 *	Date: 96/04/16	Author: Chu Ching-Wen in N.T.H.U., Taiwan
 *
 *************************************************************************/
void write_MB_header(int16 Current_MB, MB_HEADER *header)
{
	DEBUG("write_MB_header");
	#ifdef CTRL_HEADER_BITS
	extern int32 HeaderBits;
	#endif
	extern int16 Last_MTYPE, Last_MVDH, Last_MVDV;
	int16 WriteMVDH, WriteMVDV;
	int16 bits1, bits2;

	/* MBA (VLC) */
	#if (defined(DEBUG_ON) || defined(CTRL_HEADER_BITS))
	if (!(bits1=put_VLC(header->MBA, MBA_Ehuff))) {
		ERROR_LINE();
		printf("Attempting to write an empty Huffman code.\n");
		exit(ERROR_HEADER);
	}
	#else
	put_VLC(header->MBA, MBA_Ehuff);
	#endif

	#ifdef CTRL_HEADER_BITS
	HeaderBits += bits1;
	#endif

	/* MTYPE (VLC) */
	#if (defined(DEBUG_ON) || defined(CTRL_HEADER_BITS))
	if (!(bits1=put_VLC(header->MTYPE, MTYPE_Ehuff))) {
		ERROR_LINE();
		printf("Attempting to write an empty Huffman code. %d\n",
			header->MTYPE);
		exit(ERROR_HEADER);
	}
	#else
	put_VLC(header->MTYPE, MTYPE_Ehuff);
	#endif

	#ifdef CTRL_HEADER_BITS
	HeaderBits += bits1;
	#endif

	/* following data dependents on MTYPE... */

	/* MQUANT (5-bit) */
	if (MQUANT_used[header->MTYPE])	put_n_bits(5, (int32) header->MQUANT);

	#ifdef CTRL_HEADER_BITS
	HeaderBits += 5;
	#endif

	/* MVD (VLC) */
	if (MVD_used[header->MTYPE]) {
		/* use offset between two MVs to encode */

		if ((Current_MB==0) || (Current_MB==11) || (Current_MB==22) ||
		    (header->MBA!=1) || (!MVD_used[Last_MTYPE]))
			/* use current MV to encode */
			Last_MVDH = Last_MVDV = 0;

		/* horizontal offset */
		WriteMVDH = header->MVDH - Last_MVDH;
		if (WriteMVDH<-16)	WriteMVDH += 32;
		if (WriteMVDH>15) 	WriteMVDH -= 32;

		/* vertical offset */
		WriteMVDV = header->MVDV - Last_MVDV;
		if (WriteMVDV<-16)	WriteMVDV += 32;
		if (WriteMVDV>15)	WriteMVDV -= 32;

		#if (defined(DEBUG_ON) || defined(CTRL_HEADER_BITS))
		if (!(bits1=put_VLC(WriteMVDH&0x1F, MVD_Ehuff)) ||
		    !(bits2=put_VLC(WriteMVDV&0x1F, MVD_Ehuff))) {
			ERROR_LINE();
			printf("Cannot encode motion vectors.\n");
			exit(ERROR_HEADER);
		}
		#else
		put_VLC(WriteMVDH&0x1F, MVD_Ehuff);
		put_VLC(WriteMVDV&0x1F, MVD_Ehuff);
		#endif

		#ifdef CTRL_HEADER_BITS
		HeaderBits += bits1;
		HeaderBits += bits2;
		#endif

		Last_MVDV = header->MVDV;
		Last_MVDH = header->MVDH;
	}
	Last_MTYPE = header->MTYPE;

	/* CBP (VLC) */
	if (CBP_used[header->MTYPE]) {
		#if (defined(DEBUG_ON) || defined(CTRL_HEADER_BITS))
		if (!(bits1=put_VLC(header->CBP, CBP_Ehuff))) {
			ERROR_LINE();
			printf("CBP write error (MTYPE:%d, CBP:%d)\n",
				header->MTYPE, header->CBP);
			exit(ERROR_HEADER);
		}
		#else
		put_VLC(header->CBP, CBP_Ehuff);
		#endif

		#ifdef CTRL_HEADER_BITS
		HeaderBits += bits1;
		#endif
	}
}

/*************************************************************************
 *
 *	Name:		read_PSC()
 *	Description:	read the picture-start-code of frame header
 *	Input:          none
 *	Return:		none
 *	Side effects:	the read position of bitstream file will be updated
 *	Date: 96/04/16	Author: Chu Ching-Wen in N.T.H.U., Taiwan
 *
 *************************************************************************/
void read_PSC(void)
{
	DEBUG("read_PSC");

	#ifdef DEBUG_ON
	int32 psc;

	/* read PSC (20-bits) */
	if ((psc=get_n_bits(PSC_LENGTH))!=PSC) {
		ERROR_LINE();
		printf("Read 0x%x:%ld at PSC place\n", psc, psc);
		exit(ERROR_HEADER);
	}
	#else
	get_n_bits(PSC_LENGTH);
	#endif
}

/*************************************************************************
 *
 *	Name:		read_frame_header_tail()
 *	Description:	read the tail of the frame header (excluding PSC)
 *	Input:		the pointer to the frame header structure
 *	Return:		none
 *	Side effects:	the read position of bitstream file will be updated
 *	Date: 96/04/16	Author: Chu Ching-Wen in N.T.H.U., Taiwan
 *
 *************************************************************************/
void read_frame_header_tail(PIC_HEADER *header)
{
	DEBUG("read_frame_header_tail");

	/* PSC has already been read in (20) */
	header->TR = (int16) get_n_bits(5);   	/* Temporal Reference (5) */
	header->PTYPE = (int16) get_n_bits(6);	/* Picture coding Type (6) */

	if ((header->PEI=get_bit())==0)	return;
	header->PSPARE = (int16) get_n_bits(8);

	/* We can read more info. for picture layer HERE. */
	/* Each 8-bits info. followed by '1' (1-bit). */
	/* ... */

	while (get_bit())	/* read until PEI=='0' */
		get_n_bits(8);
}

/*************************************************************************
 *
 *	Name:		read_GBSC()
 *	Description:	read the group-of-block-start-code of gob header
 *	Input:          none
 *	Return:		none
 *	Side effects:	the read position of bitstream file will be updated
 *	Date: 96/04/16	Author: Chu Ching-Wen in N.T.H.U., Taiwan
 *
 *************************************************************************/
void read_GBSC(void)
{
	DEBUG("read_GBSC");

	#ifdef DEBUG_ON
	int32 gbsc;

	/* read GBSC (the first 16-bits in PSC) */
	if ((gbsc=get_n_bits(GBSC_LENGTH))!=GBSC) {
		ERROR_LINE();
		printf("Read 0x%x:%ld at GBSC place\n", gbsc, gbsc);
		exit(ERROR_HEADER);
	}
	#else
	get_n_bits(GBSC_LENGTH);
	#endif
}

/*************************************************************************
 *
 *	Name:		read_GN()
 *	Description:	read the group number of gob header
 *	Input:          none
 *	Return:		the group number
 *	Side effects:	the read position of bitstream file will be updated
 *	Date: 96/04/16	Author: Chu Ching-Wen in N.T.H.U., Taiwan
 *
 *************************************************************************/
int16 read_GN(void)
{
	DEBUG("read_GN");

	return ((int16) get_n_bits(4));
}

/*************************************************************************
 *
 *	Name:		read_GOB_header_tail()
 *	Description:	read the tail of the GOB header (excluding GBSC+GN)
 *	Input:		the pointer to the GOB header structure
 *	Return:		none
 *	Side effects:	the read position of bitstream file will be updated
 *	Date: 96/04/16	Author: Chu Ching-Wen in N.T.H.U., Taiwan
 *
 *************************************************************************/
void read_GOB_header_tail(GOB_HEADER *header)
{
	DEBUG("read_GOB_header_tail");

	/* GBSC & GN has already been read in (16+4) */
	header->GQUANT = (int16) get_n_bits(5);/* Gob QUANTization info. (5) */

	if ((header->GEI=get_bit())==0)	return;
	header->GSPARE = (int16) get_n_bits(8);

	/* We can read more info. for GOB layer HERE. */
	/* Each 8-bits info. followed by '1' (1-bit). */
	/* ... */

	while (get_bit())	/* read until GEI=='0' */
		get_n_bits(8);
}

/*************************************************************************
 *
 *	Name:		read_MB_header()
 *	Description:	read the macro-block header
 *	Input:		current MB ID and the pointer to the GOB header
 *			structure
 *	Return:		GBSC_code if read a GBSC (end-of-frame occurs),
 *			!GBSC_code for successful reading a MB header
 *	Side effects:	the read position of bitstream file will be updated,
 *			Current_MB will be updated and used here,
 *			Last_MTYPE, Last_MVDH and Last_MVDV may be updated
 *	Date: 96/04/16	Author: Chu Ching-Wen in N.T.H.U., Taiwan
 *
 *************************************************************************/
int16 read_MB_header(int16 *Current_MB, int16 GQUANT, MB_HEADER *header)
{
	DEBUG("read_MB_header");
	extern int16 Last_MTYPE, Last_MVDH, Last_MVDV;
	int16 ReadMVDH,ReadMVDV;

	/* Get rid of stuff bits */
	while ((header->MBA=get_VLC(MBA_Dhuff)) == MBAstuffing);

	/* read a frame header (PSC), not a MB header */
	if (header->MBA==GBSC_code)	return GBSC_code;

	(*Current_MB) += header->MBA;

	/* *MTYPE (VLC) */
	header->MTYPE = get_VLC(MTYPE_Dhuff);

	/* MQUANT (5-bit) */
	header->MQUANT = ((MQUANT_used[header->MTYPE]) ?
			(int16) get_n_bits(5) : GQUANT);

	/* MVD (VLC) */
	if ((*Current_MB==0) || (*Current_MB==11) || (*Current_MB==22) ||
	    (header->MBA!=1) || (!MVD_used[Last_MTYPE]))
		/* decode from current MV */
		header->MVDH = header->MVDV = 0;
	Last_MTYPE = header->MTYPE;

	if (MVD_used[header->MTYPE]) {
		/* decode to the offset of two MVs */
		ReadMVDH = get_VLC(MVD_Dhuff);
		/* ReadMVDH<0 => fill sign bit in left un-used bits */
		if (ReadMVDH&0x10) ReadMVDH |= (int16) 0xFFE0;
		header->MVDH += ReadMVDH;
		if (header->MVDH<-16)	header->MVDH += 32;
		if (header->MVDH>15) 	header->MVDH -= 32;

		ReadMVDV = get_VLC(MVD_Dhuff);
		/* ReadMVDV<0 => fill sign bit in left un-used bits */
		if (ReadMVDV&0x10) ReadMVDV |= (int16) 0xFFE0;
		header->MVDV += ReadMVDV;
		if (header->MVDV<-16)	header->MVDV += 32;
		if (header->MVDV>15)	header->MVDV -= 32;
	}

	/* CBP (VLC) */
	if (CBP_used[header->MTYPE])	header->CBP = get_VLC(CBP_Dhuff);

	return (!GBSC);	/* read well */
}

