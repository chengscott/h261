/*************************************************************************
 *
 *	Name:		h261.c
 *	Description:	main program of H.261 encoder/decoder
 *	Date: 97/12/05	Author: Chu Ching-Wen in N.T.H.U., Taiwan
 *
 *************************************************************************/

#define MAIN	/* main() is here */

#include "globals.h"
#include "ctrl.h"	/* codec controls: statistics, get time... */
#include "mytime.h"     /* TIME-type variables definition & function */
#include "thresh.h"	/* threshold values definition */

/*************************************************************************/
/* public */
extern void main(int argc, char **argv);

IMAGE *Image = NULL;		/* global info. of image */

/*************************************************************************/
/* for display in X11 system (openwin) */
#ifdef X11
boolean expand = FALSE;
#endif

/*************************************************************************/
/* coding control (according to MTYPE) (TABLE 2/H.261) */
boolean MQUANT_used[] = {0,1,0,1,0,0,1,0,0,1,0};/* Quantization used */
boolean MVD_used[] =    {0,0,0,0,1,1,1,1,1,1,0};/* Motion Vector Data used */
boolean CBP_used[] =    {0,0,1,1,0,1,1,0,1,1,0};/* CBP used in coding */
boolean TCOEFF_used[] = {1,1,1,1,0,1,1,0,1,1,0};/* Transform coeff. coded */
boolean Intra_used[] =  {1,1,0,0,0,0,0,0,0,0,0};/* Intra coded macroblock */
boolean Filter_used[] = {0,0,0,0,0,0,0,1,1,1,0};/* Filter flags */

/*************************************************************************/
/* system definitions */
int16 Current_B = 0;	/* Current Block ID */
int16 Number_MB = 0;	/* # of MacroBlock in a GOB */
int16 Current_MB = 0;	/* Current MB ID */
int16 Number_GOB = 0;	/* # of Gloup Of Block in a frame */
int16 Current_GOB = 0;	/* Current GOB ID (1st: 0 => differ from GN) */

int32 Current_frame = 0;/* Current Frame ID */
int32 Start_frame = 0;	/* Start (the first) Frame ID */
int32 Number_frame = 0;	/* # of frame */
int32 End_frame = 999;	/* The last Frame ID */
int32 Frame_skip = 1;	/* code one per FramesSkip frames (skip others) */

/* default bits_per_frame is under 64k bits/sec (bps), 15 frames/sec (fps) */
double Bit_rate = 64000.0;
double Frame_rate = 15.0;

/* last MB's info. (for checking to use DPCM or not) */
int16 Last_MVDH = 0;
int16 Last_MVDV = 0;
int16 Last_MTYPE = 0;

/*************************************************************************/
/* for statistics */
int32 First_frame_bits = 0;	/* Bits for First Frame */
int32 Total_bits = 0;		/* Total (Last) Bits for coded frame */

#ifdef CTRL_HEADER_BITS
int32 HeaderBits = 0;
#endif

#ifdef CTRL_STAT_MTYPE
int32 MTYPE_count[11] = {0,0,0,0,0,0,0,0,0,0,0};
int32 nMTYPE_mc = 0;
int32 nMTYPE_not = 0;
#endif

/*************************************************************************/
/* frame stores */
/* frame stores for encoder */
FSTORE *ori_frame = NULL;	/* original frame in encoder */
FSTORE *rec_frame = NULL;	/* reconstructed frame in encoder */
/* for motion estimation in encoder */
FSTORE *rec_frame_backup = NULL;/* save some "uncovered" MBs in encoder
				 * if MVD points to upper or left MB */
FSTORE *ref_frame = NULL;	/* point to reference frame
				 * rec_frame or rec_frame_backup
				 * it's just a pointer */
/* frame stores for decoder */
FSTORE *reco_frame = NULL;	/* reconstructed frame in decoder */
FSTORE *last_frame = NULL;	/* last reconstructed frame in decoder */

/*************************************************************************/
/* to store MB info. of the last frame: MTYPE, MVDH and MVDV */
int16 *MTYPE_frame;     /* (MVDH_frame[i] == n)  =>
			 *   MTYPE of the i-th MB of last frame is n */
int16 *MVDH_frame;	/* (MVDH_frame[i] == n)  =>
			 *   MVDH of the i-th MB of last frame is n */
int16 *MVDV_frame;	/* (MVDH_frame[i] == n)  =>
			 *   MVDV of the i-th MB of last frame is n */
int16 *Last_update;	/* (Last_update[i] == n) =>
			 *   the i-th MB has not updated in last n frames */
int16 Size_frame;	/* the size of the above arrays */

/*************************************************************************/
/* Look-up-table for obtaining memloc in MEM: */
/* Position of the upper-left connor in the GurrentGOB-th GOB for Y-type */
int32 YGOB_memloc[12];
/* Position of the upper-left connor in the GurrentGOB-th GOB for Cb, Cr */
int32 GOB_memloc[12];

/* Position of the upper-left connor in the Current_MB-th MB for Y-type */
int32 YMB_memloc[33];
/* Position of the upper-left connor in the Current_MB-th MB for Cb, Cr */
int32 MB_memloc[33];

/* Position of the upper-left connor in the Current_B-th block */
int32 B_memloc[6];

/* Distance due to MVDV for Y-type */
int32 YMVDV_memloc[31];
/* Distance due to MVDV for Cb, Cr */
int32 MVDV_memloc[31];

/* (GOB_posX[i], GOB_poxY[i]) is the position of the i-th GOB */
int32 GOB_posX[12];
int32 GOB_posY[12];

/* (MB_posX[i], MB_poxY[i]) is the position of the i-th MB */
int32 MB_posX[33];
int32 MB_posY[33];

/*************************************************************************/
/* different motion-estimation algo. */
/* use_me_algo(): default to full_search_ME */
int16 (*default_me_algo)(MEM *, MEM *) = three_step_search_ME;
#define use_me_algo (*default_me_algo)

/*************************************************************************/
/* private */
/* H.261 encoder */
static void H261_encoder(void);
static int16 obtain_GQUANT(int32 GQUANT, int32 remainder_size);
static void encode_I_frame(void);
static void encode_P_frame(void);
static void encode_intra_MB(void);
static void encode_inter_MB(void);
static void write_inter_MB(void);
/* H.261 decoder */
static void H261_decoder(void);
static void decode_frame(void);
static void decode_MB(void);
/* shared functions for encoder and decoder */
static void set_image_type(void);
static void help(void);
static void help1(void);

/*************************************************************************/
/* for default option and block type defition {Y1, Y2, Y3, Y4, Cb, Cr} */
static boolean use_decoder = FALSE;	/* use H261_decoder (not encoder) */
static ComponentType Block_type[]
		= {_Y, _Y, _Y, _Y, _Cb, _Cr};	/* for each block in a MB */
static char *image_frame_suffix[NUMBER_OF_COMPONENTS]
		= {Y_FILE_SUFFIX, Cb_FILE_SUFFIX, Cr_FILE_SUFFIX};

/*************************************************************************/
/* CCITT p*64 header info. (H.261 sec.4) */
static PIC_HEADER *pic_header;	/* picture header defined in globals.h */
static GOB_HEADER *gob_header;	/* GOB header defined in globals.h */
static MB_HEADER *mb_header;	/* MB header defined in globals.h */

/* very-often-used variables in header */
static int16 MTYPE;	/* Macro-block TYPE */
static int16 MVDH;	/* Motion Vector Data for Horizontal offset */
static int16 MVDV;	/* Motion Vector Data for Vertical offset */
static int16 Last_MB;	/* for obtaining MBA */

/*************************************************************************/
/* temporary integer storage for block operation (e.g. DCT, quantization) */
static int16 MBbuf[6][64];	/* buffers of macroclock (6 blocks) */

/*************************************************************************/
/* for rate control in encoder (by changing GQUANT) */
/* default bits_per_frame is under 64k bits/sec (bps), 15 frames/sec (fps) */
static int32 bits_per_frame, buffer_size;
static double bit_per_pixel = 0.0;

/*************************************************************************/
/* use_DCT() and use_IDCT() */
/* 2D DCT and IDCT (nomal version) */
/* default to normal version */
static void (*default_DCT)(int16 *, int16*) = DCT;
static void (*default_IDCT)(int16 *, int16 *) = IDCT;
#define use_DCT (*default_DCT)
#define use_IDCT (*default_IDCT)

/*************************************************************************/
/* for PTYPE and PSPARE */
#define CIF_PTYPE 0x04		/* PTYPE pattern for image type CIF */
/* in PSPARE: the left 4-bits indicates NTSC type */
#define NTSC_PSPARE 0xf0	/* PSPARE pattern for image type NTSC */
#define P64_NTSC_PSPARE 0x8c	/* PSPARE pattern for NTSC of p64 codec */

/*************************************************************************/
/* set mem->memloc to the right position by Current_GOB and Current_MB */
#define SET_memloc(mem, Y_memloc, CbCr_memloc) {\
		mem->fs[_Y]->memloc = Y_memloc;\
		mem->fs[_Cb]->memloc = mem->fs[_Cr]->memloc = CbCr_memloc;\
	}

/*************************************************************************/
/* for command and arguments checking */
static char *command;	/* command = argv[0] (for help()) */
/* too-few-argument checking corr. to some arguments(eg. -a, -b) */
#define CHECK_NEXT_ARGV(c) \
	if (i+1>=argc) {\
		help();\
		printf("Too few argument after -%c.\n", c);\
		exit(ERROR_ARGV);\
	}
/* out-of-range checking corr. to some arguments */
#define CLIP_ARGV(opt, val, min, max) \
	if ((val)<(min)) {\
		printf("Out of range: -%c %d<%d, change to %d\n", opt, (int)val, (int)min, (int)min);\
		val = min;\
	} else if (((max)>(min)) && ((val>max))) {\
		printf("Out of range: -%c %d>%d, change to %d\n", opt, (int)val, (int)max, (int)max);\
		val = max;\
	}

/*************************************************************************
 *
 *	Name:		main()
 *	Description:	parse the command line and set parameters accordingly
 *	Input:          the number of arguments and the arguments
 *	Return:	       	none
 *	Side effects:	exit while error occurs
 *	Date: 96/05/09	Author: Chu Ching-Wen in N.T.H.U., Taiwan
 *
 *************************************************************************/
void main(int argc, char **argv)
{
	DEBUG("main");
	int16 i, s;

	/* Initialization */
	command = argv[0];
	Image = make_IMAGE();
	MAKE_STRUCTURE(pic_header, PIC_HEADER);
	MAKE_STRUCTURE(gob_header, GOB_HEADER);
	MAKE_STRUCTURE(mb_header, MB_HEADER);

	pic_header->PTYPE =	pic_header->PEI = pic_header->PSPARE =
	gob_header->GEI = gob_header->GSPARE = 0;

	Image->type = _QCIF;	/* default image type QCIF */
	Image->display = FALSE;
	for (i=1; i<argc; i++) {
		if (!strcmp("-QCIF", argv[i])) {
			Image->type = _QCIF;
		} else if (!strcmp("-CIF", argv[i])) {
			Image->type = _CIF;
		} else if (!strcmp("-NTSC", argv[i])) {
			Image->type = _NTSC;
		} else if (*(argv[i]) == '-') {
			switch (*(++argv[i])) {
			case 'H':	/* help */
			case 'h':
				if (((++i)<argc) && (atol(argv[i])!=0))
					help1();
				else	help();
				exit(0);
#ifdef X11
			case 'E':	/* expand display window by 4 */
			case 'e':
				expand = TRUE;
			case 'W':	/* open a window to display */
			case 'w':
				Image->display = TRUE;
				break;
#endif
			/* for decoder */
			case 'D':	/* choose to decode, not to encode */
			case 'd':	/* and set stream filename */
				CHECK_NEXT_ARGV(*argv[i]);
				use_decoder = TRUE;
				Image->Stream_filename = argv[++i];
				break;

			/* for encoder */
			case 'I':	/* input frame files' prefix */
			case 'i':
				CHECK_NEXT_ARGV(*argv[i]);
				Image->read_from_files = TRUE;
				strcpy(Image->input_frame_prefix, argv[++i]);
				break;
			case 'B':	/* last frame ID */
			case 'b':
				CHECK_NEXT_ARGV(*argv[i]);
				End_frame = atol(argv[++i]);
				CLIP_ARGV(*argv[i-1], End_frame, 0, -1);
				break;
			case 'P':	/* bit rate in bit/pixel */
			case 'p':
				/* bit/pixel */
				CHECK_NEXT_ARGV(*argv[i]);
				bit_per_pixel = atof(argv[++i]);
				break;
			case 'R':	/* bit rate and frame rate */
			case 'r':
				/* bit rate */
				CHECK_NEXT_ARGV(*argv[i]);
				Bit_rate = atof(argv[++i]);
				CLIP_ARGV(*argv[i-1], Bit_rate, 1.0, -1.0);
				Bit_rate *= 1000;

				/* frame rate */
				CHECK_NEXT_ARGV(*argv[i-1]);
				Frame_rate = atof(argv[++i]);
				CLIP_ARGV(*argv[i-1], Frame_rate, 1.0, -1.0);
				break;
			case 'K':	/* encode one per FramesSkip frames */
			case 'k':
				CHECK_NEXT_ARGV(*argv[i]);
				Frame_skip = atol(argv[++i]);
				CLIP_ARGV(*argv[i-1], Frame_skip, 1, 31);
				break;
			case 'M':	/* set motion estimation algo. */
			case 'm':
				CHECK_NEXT_ARGV(*argv[i]);
				switch (atol(argv[++i])) {
				case FULL_SEARCH:
					default_me_algo = full_search_ME;
					break;
				case THREE_STEP_SEARCH:
					default_me_algo = three_step_search_ME;
					break;
				case NEW_THREE_STEP_SEARCH:
					default_me_algo = new_three_step_search_ME;
					break;
				case MY_SEARCH:
					default_me_algo = my_search_ME;
					break;
				default:
					printf("Out of range: -%c %s, change to %d\n",
						*argv[i-1], argv[i], THREE_STEP_SEARCH);
					default_me_algo = three_step_search_ME;
				}
				break;
			case 'S':	/* output stream filename setting */
			case 's':
				CHECK_NEXT_ARGV(*argv[i]);
				Image->Stream_filename = argv[++i];
				break;

			/* for both encoder and decoder */
			case 'A':	/* start frame ID */
			case 'a':
				CHECK_NEXT_ARGV(*argv[i]);
				Start_frame = atol(argv[++i]);
				CLIP_ARGV(*argv[i-1], Start_frame, 0, -1);
				break;
			case 'O':	/* output frame files' prefix */
			case 'o':
				CHECK_NEXT_ARGV(*argv[i]);
				Image->write_to_files = TRUE;
				strcpy(Image->output_frame_prefix, argv[++i]);
				break;
			case 'Z':	/* frame files' suffixes setting */
			case 'z':
				/* Y frame files */
				CHECK_NEXT_ARGV(*argv[i]);
				strcpy(Image->frame_suffix[_Y],
					argv[++i]);

				/* Cb frame files */
				CHECK_NEXT_ARGV(*argv[i-1]);
				strcpy(Image->frame_suffix[_Cb],
					argv[++i]);

				/* Cr frame files */
				CHECK_NEXT_ARGV(*argv[i-2]);
				strcpy(Image->frame_suffix[_Cr],
					argv[++i]);
				break;
			default:
				help();
				printf("Illegal option: -%s.\n", argv[i]);
				exit(ERROR_ARGV);
			}
		} else {
			help();
			printf("Illegal argument: %s.\n", argv[i]);
			exit(ERROR_ARGV);
		}
	}

	/* set stream filename if not specified */
	if (Image->Stream_filename==NULL) {
		if (*Image->input_frame_prefix=='\0') {
			help();
			printf("<bitstream filename> should be specified.\n");
			exit(ERROR_ARGV);
		}

		/* set bitstream file name by input_frame_prefix */
		if (!(Image->Stream_filename = (char *) malloc(sizeof(char) *
				(strlen(Image->input_frame_prefix) + 6)))) {
			ERROR_LINE();
			printf("Can't allocate string for Stream_filename.\n");
			exit(ERROR_MEMORY);
		}
		sprintf(Image->Stream_filename, "%s%s",
			Image->input_frame_prefix, STREAM_FILE_SUFFIX);
	}

	/* set frame file suffixes if not specified */
	if (Image->read_from_files || Image->write_to_files) {
		/* copy suffix of frame filename if not specified */
		for(i=0; i<NUMBER_OF_COMPONENTS; i++) {
			if (*Image->frame_suffix[i]=='\0') {
				strcpy(Image->frame_suffix[i], image_frame_suffix[i]);
			}
		}
	}

	/* set image type : PTYPE */
	switch (Image->type) {
	case _NTSC:		/* NTSC */
		pic_header->PTYPE |= CIF_PTYPE;

		/* Since NTSC type is not included in H.261 standard */
		/* so we need to add extra info. here for decoder use. */
		pic_header->PEI = 1;
		pic_header->PSPARE |= NTSC_PSPARE;
		break;
	case _CIF:		/* CIF */
		pic_header->PTYPE |= CIF_PTYPE;
		break;
	case _QCIF:		/* QCIF */
		pic_header->PTYPE |= 0x00;
		break;
	default:
		ERROR_LINE();
		printf("Image Type not supported: %d\n", Image->type);
		exit(ERROR_ARGV);
	}

	#ifdef CTRL_GET_TIME
	tTIME_COST = get_time_cost();
	#endif

	if (use_decoder) {
		/* DECODER */
		H261_decoder();
	} else {
		/* ENCODER */
		Number_frame = End_frame - Start_frame + 1;
		if (Number_frame<=0) {
			ERROR_LINE();
			printf("Need positive number of frames.\n");
			exit(ERROR_ARGV);
		}

		H261_encoder();
	}
}

/*************************************************************************
 *
 *	Name:	       	H261_encoder()
 *	Description:	encode the sequence of frame(s)
 *	Input:          none
 *	Return:	       	none
 *	Side effects:	exit while error occurs
 *	Date: 96/05/09	Author: Chu Ching-Wen in N.T.H.U., Taiwan
 *
 *************************************************************************/
static void H261_encoder(void)
{
	DEBUG("Encoder");
	int32 target_bits;

	/* initialization */
	init_VLC();
	set_image_type();
#ifdef X11
	/* init display after we have set image type */
	if (Image->display) {
		if (!init_display("")) exit(ERROR_DISP);
		init_dither();
	}
#endif

	/* make frame store after we have set image type */
	alloc_mem_encoder();
	open_write_stream(Image->Stream_filename);

	/* set rate control parameter */
	if (bit_per_pixel!=0.0) {
		/* [-p] (bit/pixel) override [-r] (kbits/sec) */
		Bit_rate = Frame_rate * bit_per_pixel
			* Image->Width * Image->Height;
	}
	bits_per_frame = (int32) (Frame_skip * Bit_rate / Frame_rate);

	#ifdef CTRL_GET_TIME
	tTOTAL = 0;
	#if (CTRL_GET_TIME==GET_ALL_TIME)
	tDCT = tME = tQUAN = 0;
	tIDCT = tIQUAN = 0;
	#endif
	#endif

	Total_bits = 0;

	/* the 1st frame must be I-frame */
	Current_frame = Start_frame;
	if (!read_and_show_frame(Current_frame, ori_frame)) {
		help();
		if (Image->read_from_files)
			printf("No image file(s): %s.\n",
				Image->input_frame_prefix);
		else
			printf("No capture exists! <input_frame_prefix> should be specified.\n");
		exit(ERROR_ARGV);
	}

	/* print decoder info before processing the 1st frame */
	print_codec_info(use_decoder);

	/* save the reconstructed (coded) frame in rec_frame */
	encode_I_frame();

	/* write out the 1st frame */
	write_or_show_frame(Current_frame, rec_frame);
	First_frame_bits = ftell_write_stream();

	#ifdef CTRL_PSNR
	statistics(ori_frame, rec_frame);
	#endif
	#ifdef CTRL_FRAME_INFO
	print_frame_info(Current_frame);
	printf("\tGQUANT = %d\n", gob_header->GQUANT);
	#endif

	Current_frame += Frame_skip;

	/* encode other frames */
	buffer_size = bits_per_frame << 4;
	target_bits = First_frame_bits;	/* exculding the 1st frame */
	while ((Current_frame<=End_frame)) {

		/* read a frame (3 files) */
		if (!read_and_show_frame(Current_frame, ori_frame)) break;

		/* set timer to obtain total time */
		#ifdef CTRL_GET_TIME
		get_time(tTOTAL1);
		#endif

		/* save the reconstructed frame in rec_frame */
		#ifdef CTRL_ALL_INTRA
		encode_I_frame();
		#else	/* not CTRL_ALL_INTRA */
		encode_P_frame();
		#endif

		/* total time excludes I/O time */
		#ifdef CTRL_GET_TIME
		get_time(tTOTAL2);
		ntTOTAL++;
		tTOTAL += diff_time(tTOTAL2, tTOTAL1);
		#endif

		/* write out frame(s) till Current_frame */
		write_or_show_frame(Current_frame, rec_frame);

		#ifdef CTRL_PSNR
		statistics(ori_frame, rec_frame);
		#endif
		#ifdef CTRL_FRAME_INFO
		print_frame_info(Current_frame);
		printf("\tGQUANT = %d\n", gob_header->GQUANT);
		#endif

		/* change GQUANT by current buffer remains */
		target_bits += bits_per_frame;
		gob_header->GQUANT =
			obtain_GQUANT(gob_header->GQUANT, target_bits - ftell_write_stream());

		Current_frame += Frame_skip;
	}
	/* reset End_frame if un-normal break */
	if (Current_frame<=End_frame) {
		/* kbhit or no frame_files */
		End_frame = Current_frame - Frame_skip;
		Number_frame = End_frame - Start_frame + 1;
		printf("The last frame ID is %ld.\n", End_frame);
	}

	/* write out the rest frame(s) after the last coded frame */
	write_or_show_frame(End_frame, rec_frame);

	/* use a extra frame-header as EOF-code */
	pic_header->TR = MOD_32(End_frame);
	write_frame_header(pic_header);

	Total_bits = ftell_write_stream();

	/* we always print info. for the last frame */
	print_sequence_info(use_decoder);
	close_write_stream();
}

/*************************************************************************
 *
 *	Name:	       	obtain_GQUANT()
 *	Description:	obtain GQUANT by quantizer and remainder_size
 *	Input:         	GQUANT is the present GQUANT, and 
 *			remainder_size insicates the remaiander size of the
 *  			buffer
 *	Return:	       	the quantizer (GQUNAT)
 *	Side effects:	none
 *	Date: 97/12/05	Author: Chu Ching-Wen in N.T.H.U., Taiwan
 *
 *************************************************************************/
static int16 obtain_GQUANT(int32 GQUANT, int32 remainder_size)
{
	DEBUG("obtain_GQUANT");
    
    float delta,temp;

	/* MODIFY CODES HERE */
 
	delta = (float) (buffer_size-remainder_size) / buffer_size;
	//printf("buffer_size = %ld,\t\t remainder_size = %ld\n", buffer_size, remainder_size);
	printf("delta=%f\t\t",delta);
    if (delta<0)           delta=0;
     else if (delta>1)   delta=1;
    temp = (float)(31*delta);
	printf("GQUANT = %2ld --> ", GQUANT);
	GQUANT=ceil(temp);
	printf("%2ld\n", GQUANT);

	/* buffer is under-flow of toleratable range (buffer_size) ? */
	if (remainder_size>=buffer_size) {
		/* => we can produce more bits for the next frame */
		/* we should decrease GQUANT..... */
		printf("Buffer is underflow ! (%ld)\n", remainder_size);
	} else 
	/* buffer is over-flow of toleratable range (buffer_size) ? */
	if (-remainder_size>=buffer_size) {
		/* => we should produce less bits for the next frame */
		/* we should increase GQUANT..... */
		printf("Buffer is overflow ! (%ld)\n", remainder_size);
	}
	return GQUANT;
}

/*************************************************************************
 *
 *	Name:	       	encode_I_frame()
 *	Description:	encode a single intra frame (all MBs are intra)
 *	Input:          none
 *	Return:	       	none
 *	Side effects:   entries of rec_frame will be changed
 *	Date: 96/04/29	Author: Chu Ching-Wen in N.T.H.U., Taiwan
 *
 *************************************************************************/
static void encode_I_frame(void)
{
	DEBUG("encode_I_frame");
	int16 *block;
	int32 YGOB_memloc1, GOB_memloc1;
	int32 Y_memloc, CbCr_memloc;

	/* write picture header (PSC TR PTYPE [PEI PSPARE])*/
	pic_header->TR = MOD_32(Current_frame);
	write_frame_header(pic_header);

	memset(MTYPE_frame, INTRA, Size_frame);
	memset(MVDH_frame, 0, Size_frame);
	memset(MVDV_frame, 0, Size_frame);
	memset(Last_update, 0, Size_frame);
	MTYPE = INTRA;

	/* quantization stepsize for intra is unchanged for each blocks */
	gob_header->GQUANT = DEFAULT_QUANTIZER;

	/* start to encode each GOB ... */
	for (Current_GOB=0; Current_GOB<Number_GOB; Current_GOB++) {
		/* write GOB header (GBSC GN GQUANT [GEI GSPARE]) */
		/* get gob_header->GN */
		gob_header->GN = ((Image->type==_QCIF) ?
				((Current_GOB<<1) + 1) : (Current_GOB + 1));
		write_GOB_header(gob_header);

		YGOB_memloc1 = YGOB_memloc[Current_GOB];
		GOB_memloc1 = GOB_memloc[Current_GOB];

		/* start to encode each MB ... */
		Last_MB = -1;
		for (Current_MB=0; Current_MB<Number_MB; Current_MB++
				#ifdef CTRL_STAT_MTYPE
				, MTYPE_count[MTYPE]++
				#endif
				) {
			/* get the memory location of current MB
			 * consult FIGURE 6, 8/H.261,
			 * set memloc in ori_frame->fs, rec_frame->fs and rec_frame_backup->fs */
			Y_memloc = YGOB_memloc1 + YMB_memloc[Current_MB];
			CbCr_memloc = GOB_memloc1 + MB_memloc[Current_MB];

			SET_memloc(ori_frame, Y_memloc, CbCr_memloc);

			/* current MB --> encoder --> decoder : for next frame */
			SET_memloc(rec_frame, Y_memloc, CbCr_memloc);

			/* read current MB from ori_frame to MBbuf
			 * for "block operation" (DCT etc.) */
			read_MB(MBbuf, ori_frame);

			encode_intra_MB();
		}/* end of one MB */
	}/* end of one GOB */
}

/* set ref_frame->fs to Fs->fs (just a pointer assignment) */
#define SET_ref_frame_2(Fs) {\
		ref_frame->fs[_Y]->data = Fs->fs[_Y]->data;\
		ref_frame->fs[_Cb]->data = Fs->fs[_Cb]->data;\
		ref_frame->fs[_Cr]->data = Fs->fs[_Cr]->data;\
	}

/*************************************************************************
 *
 *	Name:	       	encode_P_frame()
 *	Description:	encode a single inter frame using motion estimation
 *	Input:          none
 *	Return:	       	none
 *	Side effects:   entries of rec_frame will be changed
 *	Date: 96/04/29	Author: Chu Ching-Wen in N.T.H.U., Taiwan
 *
 *************************************************************************/
static void encode_P_frame(void)
{
	DEBUG("encode_P_frame");
	int16 nMB;
	int32 YGOB_memloc1, GOB_memloc1;
	int32 cur_Y_memloc, cur_CbCr_memloc;
	int32 ref_Y_memloc, ref_CbCr_memloc;
	boolean buffer_used;

	/* write picture header (PSC TR PTYPE [PEI PSPARE])*/
	pic_header->TR = MOD_32(Current_frame);
	write_frame_header(pic_header);

	motion_estimation();

	/* start to encode each GOB ... */
	for (nMB=Current_GOB=0; Current_GOB<Number_GOB; Current_GOB++) {
		Last_MTYPE = Last_MVDH = Last_MVDV = 0;

		/* write GOB header (GBSC GN GQUANT [GEI GSPARE]) */
		/* get gob_header->GN */
		gob_header->GN = ((Image->type==_QCIF) ?
				((Current_GOB<<1) + 1) : (Current_GOB + 1));
		write_GOB_header(gob_header);

		YGOB_memloc1 = YGOB_memloc[Current_GOB];
		GOB_memloc1 = GOB_memloc[Current_GOB];

		/* start to encode each MB ... */
		Last_MB = -1;
		for (Current_MB=0; Current_MB<Number_MB; Current_MB++,nMB++
				#ifdef CTRL_STAT_MTYPE
				, MTYPE_count[MTYPE]++
				#endif
				) {
			/* ME for superblock between Y_frame and MBbuf,
			 * it defines MTYPE and MV (MVDH, MVDV)
			 * NOTE: MTYPE may be changed according to CBP */
			MTYPE = MTYPE_frame[nMB];

			/* We first skip backgrond MB... not trans. */
			if (MTYPE==MB_NOT_TRANSMIT) {
				/* no MV and no TCOEFF */
				/* do not transmit current MB */
				continue;
			}

			cur_Y_memloc = ref_Y_memloc
				= YGOB_memloc1 + YMB_memloc[Current_MB];
			cur_CbCr_memloc = ref_CbCr_memloc
				= GOB_memloc1 + MB_memloc[Current_MB];

			SET_memloc(rec_frame, cur_Y_memloc, cur_CbCr_memloc);
			SET_memloc(ori_frame, cur_Y_memloc, cur_CbCr_memloc);

			/* read current MB from ori_frame to MBbuf
			 * for "block operation" (DCT etc.) */
			read_MB(MBbuf, ori_frame);

			/* process each block in current MB */

			/* Intra */
			if (Intra_used[MTYPE]) {
				encode_intra_MB();
				continue;
			}

			/* Inter */
			buffer_used = 0;
			if (MVD_used[MTYPE]) {
				MVDH = mb_header->MVDH = MVDH_frame[nMB];
				MVDV = mb_header->MVDV = MVDV_frame[nMB];

				/* use the "uncover" frame */
				if ((MVDH<0) || (MVDV<0))
					buffer_used = 1;

				/* the reference frames
				 * (ref_frame and Y_frame) may move (MVDH, MVDV) */
				/* obtain memloc by current MV */
				if (MVDV>=0) {
					ref_Y_memloc +=
					(YMVDV_memloc[MVDV] + MVDH);
					ref_CbCr_memloc +=
					(MVDV_memloc[MVDV] + (MVDH>>1));
				} else {
					ref_Y_memloc +=
					(-YMVDV_memloc[-MVDV] + MVDH);
					ref_CbCr_memloc +=
					(-MVDV_memloc[-MVDV] + (MVDH>>1));
				}
			}

			if (buffer_used) {
				SET_ref_frame_2(rec_frame_backup);
			} else {
				SET_ref_frame_2(rec_frame);
			}

			/* reference MB : for residual */
			SET_memloc(ref_frame, ref_Y_memloc, ref_CbCr_memloc);

			if (TCOEFF_used[MTYPE]) {
				/* MC with residual (TCOEFF) */

				/* obtain CBP and coded-residual (TCOEFF) */
				encode_inter_MB();

				/* MTYPE may be changed to (without residual)
				 * due to small enough coded-residual */
				if (mb_header->CBP==0) {
					/* TCOEFFs<CBP_THRESHOLD */
					/* => do not use CBP and TCOEFF */
					if (MVD_used[MTYPE]) {
						MTYPE = INTER_MC;

						#ifdef CTRL_STAT_MTYPE
						nMTYPE_mc++;
						#endif
					} else {
						MTYPE = MB_NOT_TRANSMIT;

						#ifdef CTRL_STAT_MTYPE
						nMTYPE_not++;
						#endif

						continue;
					}
				}
			}

			/* write out MB data */
			/* at least need to trans. MB header */
			/* MBA : current MacroBlock Address */
			mb_header->MBA = Current_MB - Last_MB;
			mb_header->MTYPE = MTYPE;
			write_MB_header(Current_MB, mb_header);
			Last_MB = Current_MB;

			if (TCOEFF_used[MTYPE]) {
				/* Inter with residual
				 * (residual is not too small) */
				write_inter_MB();
			} else {
				/* MC without residual */
				/* write header only (with MV) */
				/* MTYPE = 4 or 7 */
				copy_MB(rec_frame, ref_frame);
			}
		}/* end of one MB */
	}/* end of one GOB */
}

/*************************************************************************
 *
 *	Name:	       	encode_intra_MB()
 *	Description:	encode each block in current MB using intra type
 *	Input:          none
 *	Return:	       	none
 *	Side effects:	entries of rec_frame will be changed
 *	Date: 96/04/29	Author: Chu Ching-Wen in N.T.H.U., Taiwan
 *
 *************************************************************************/
static void encode_intra_MB(void)
{
	DEBUG("encode_intra_MB");
	int16 *block;
	ComponentType Btype;	/* for block type (= _Y, _U or _V) */

	/* MBA : current MacroBlock Address */
	mb_header->MBA = Current_MB - Last_MB;
	mb_header->MTYPE = MTYPE;
	write_MB_header(Current_MB, mb_header);
	Last_MB = Current_MB;

	/* for each block data */
	/* obtain TCOEFF */
	for (Current_B=0; Current_B<6; Current_B++) {
		Btype = Block_type[Current_B];
		block = MBbuf[Current_B];

		/* encode MBbuf[][] */
		use_DCT(block, block);
		quantize(Intra_used[MTYPE], block, gob_header->GQUANT);
		transfer_TCOEFF(1, block); /* 1 ==> Intra_used */

		/* reconstruct block: */
		/* a "small decoder" in the encoder */
		Iquantize(Intra_used[MTYPE], block, gob_header->GQUANT);
		use_IDCT(block, block);
		clip_reconstructed_block(block);

		/* write out data to rec_frame for next frame's rec_frame */
		write_block(rec_frame->fs[Btype], block);
	}
}

/*************************************************************************
 *
 *	Name:	       	encode_inter_MB()
 *	Description:	encode each block's residual in current MB and obatin
 *			the CBP (but MBbuf has NOT written to the bitstream)
 *	Input:          none
 *	Return:	       	none
 *	Side effects:	entries of MBbuf and mb_header->CBP will be changed
 *	Date: 96/04/16	Author: Chu Ching-Wen in N.T.H.U., Taiwan
 *
 *************************************************************************/
static void encode_inter_MB(void)
{
	DEBUG("encode_inter_MB");
	int16 i;
	int16 *block;
	int16 CBPmask = 0x20; 	/* (10 0000) */
	ComponentType Btype;	/* for block type (= _Y, _U or _V) */

	/* for each block data */
	/* obtain CBP and TCOEFF */
	for (mb_header->CBP=Current_B=0; Current_B<6; Current_B++,CBPmask>>=1) {
		Btype = Block_type[Current_B];
		block = MBbuf[Current_B];

		/* load residual block into block[] */
		load_residual(ref_frame->fs[Btype], block,
				Filter_used[MTYPE]);

		/* encode MBbuf[][] */
		use_DCT(block, block);
		if (quantize(Intra_used[MTYPE], block, gob_header->GQUANT)) {
			/* set current block into CBP */
			mb_header->CBP |= CBPmask;
		} 
	}
}

/*************************************************************************
 *
 *	Name:	       	write_inter_MB()
 *	Description:	write out the encoded-MB to the bitstream
 *	Input:          none
 *	Return:	       	none
 *	Side effects:	entries of MBbuf and rec_frame will be changed
 *	Date: 96/04/16	Author: Chu Ching-Wen in N.T.H.U., Taiwan
 *
 *************************************************************************/
static void write_inter_MB(void)
{
	DEBUG("write_inter_MB");
	int16 *block;
	int16 CBPmask = 0x20;	/* (10 0000) */
	ComponentType Btype;	/* for block type (= _Y, _U or _V) */

	/* for each block data */
	for (Current_B=0; Current_B<6; Current_B++,CBPmask>>=1) {
		Btype = Block_type[Current_B];
		block = MBbuf[Current_B];

		/* encode MBbuf[][]: intra block or residual block */
		if (mb_header->CBP&CBPmask) {
			/* CBP => current block should be transfered */
			transfer_TCOEFF(0, block); /* 0 ==> !Intra_used */

			/* reconstruct block: */
			/* a 'small decoder' in the encoder */
			Iquantize(Intra_used[MTYPE], block, gob_header->GQUANT);
			use_IDCT(block, block);

			/* save residual block to block[] */
			save_residual(ref_frame->fs[Btype], block,
					Filter_used[MTYPE]);
			clip_reconstructed_block(block);

			/* write out data to rec_frame */
			write_block(rec_frame->fs[Btype], block);
		} else {
			/* for motion estimation of the next frame */
			copy_block(rec_frame->fs[Btype], ref_frame->fs[Btype]);
		}
	}
}

/*************************************************************************
 *
 *	Name:	       	H261_decoder()
 *	Description:	decode the sequence of frame(s)
 *	Input:          none
 *	Return:	       	none
 *	Side effects:	exit while error occurs
 *	Date: 96/05/09	Author: Chu Ching-Wen in N.T.H.U., Taiwan
 *
 *************************************************************************/
static void H261_decoder(void)
{
	DEBUG("H261_decoder");
	int16 first_TR;

	/* initialization */
	init_VLD();
	open_read_stream(Image->Stream_filename);

	/* decode the 1st frame header for image type definition */
	read_PSC();
	read_frame_header_tail(pic_header);

	if (eof_read_stream()) {
		printf("No image data in the bitstream file: %s.\n",
			Image->Stream_filename);
		exit(ERROR_EOF);
	}

	/* set image type : CIF or QCIF or NTSC */
	if ((pic_header->PTYPE&CIF_PTYPE)==CIF_PTYPE) {
		if (pic_header->PEI &&
		   ((pic_header->PSPARE&NTSC_PSPARE)==NTSC_PSPARE ||
		     pic_header->PSPARE==P64_NTSC_PSPARE))
			Image->type = _NTSC;
		else
			Image->type = _CIF;
	} else {
		Image->type = _QCIF;
	}
	set_image_type();

#ifdef X11
	/* init display after we have set image type */
	if (Image->display) {
		if (!init_display("")) exit(ERROR_DISP);
		init_dither();
	}
#endif

	/* check the display */
	if ((!Image->write_to_files) && (!Image->display)) {
		help();
		printf("Do not suport this display! <output_frame_prefix> should be specified.\n");
		exit(ERROR_ARGV);
	}

	/* make frame store after we have set image type */
	alloc_mem_decoder();

	/* print decoder info before processing the 1st frame */
	print_codec_info(use_decoder);

	/* set suitable functions for decoding the 1st frame */
	default_IDCT = IDCT;

	/* decode the 1st frame */
	#ifdef CTRL_STAT_MTYPE
	MTYPE_count[MB_NOT_TRANSMIT] += (Number_GOB * Number_MB);
	#endif
	Current_frame = Start_frame;
	first_TR = pic_header->TR - Start_frame;
	decode_frame();

	/* here we have got a PSC after decode_frame() */
	First_frame_bits = ftell_read_stream() - PSC_LENGTH;

	/* set suitable functions for decoding other frames */
	default_IDCT = IDCT;

	#ifdef CTRL_GET_TIME
	tTOTAL = 0;
	#if (CTRL_GET_TIME==GET_ALL_TIME)
	tDCT = tME = tQUAN = 0;
	tIDCT = tIQUAN = 0;
	#endif
	#endif

	/* write the 1st frame and decode & write other frames */
	while (TRUE) {
		/* write out frame(s) till Current_frame */
		write_or_show_frame(Current_frame, reco_frame);

		/* here we have got a PSC after decode_frame() */
		read_frame_header_tail(pic_header);

		/* here we have got a frame header */
		/* but we skip checking PTYPE */

		if (eof_read_stream())	break;	/* the only break */

		/* set Current_frame = pic_header->TR */
		do {
			Current_frame++;

			#ifdef CTRL_STAT_MTYPE
			MTYPE_count[MB_NOT_TRANSMIT]
				+= (Number_GOB * Number_MB);
			#endif
		} while (pic_header->TR != MOD_32(Current_frame+first_TR));

		/* set timer to obtain total time */
		#ifdef CTRL_GET_TIME
		get_time(tTOTAL1);
		#endif

		/* decode the pic_header->TR's frame */
		copy_FS(last_frame, reco_frame);
		decode_frame();
		/* here we have got a PSC after decode_frame() */

		/* total time excludes I/O time */
		#ifdef CTRL_GET_TIME
		get_time(tTOTAL2);
		ntTOTAL++;
		tTOTAL += diff_time(tTOTAL2, tTOTAL1);
		#endif
	}

	/* the last TR indicates End_frame */
	if (pic_header->TR != MOD_32(Current_frame+first_TR)) {
		/* Current_frame != End_frame == pic_header->TR */
		/* => pic_header->TR's frame is a skip frame */

		/* set Current_frame = pic_header->TR */
		do {
			Current_frame++;

			#ifdef CTRL_STAT_MTYPE
			MTYPE_count[MB_NOT_TRANSMIT]
				+= (Number_GOB * Number_MB);
			#endif
		} while (pic_header->TR != MOD_32(Current_frame+first_TR));

		/* write out the rest frame(s) after the last coded frame */
		write_or_show_frame(Current_frame, reco_frame);
	}
	End_frame = Current_frame;
	Number_frame = End_frame - Start_frame + 1;
	printf("The last frame ID is %ld.\n", End_frame);

	Total_bits = ftell_read_stream();
	print_sequence_info(use_decoder);
	close_read_stream();
}

/*************************************************************************
 *
 *	Name:	       	decode_frame()
 *	Description:	decode the pic_header->TR's frame
 *	Input:          none
 *	Return:	       	none
 *	Side effects:   entries of rec_frame will be changed
 *	Date: 96/04/16	Author: Chu Ching-Wen in N.T.H.U., Taiwan
 *
 *************************************************************************/
static void decode_frame(void)
{
	DEBUG("decode_frame");
	int16 nGOB, nMB;

	/* here we have read the frame header */

	/* decode each GOB in pic_header->TR's frame */
	read_GBSC();
	while ((gob_header->GN=read_GN())!=0) {
		/* not PSC */
		/* obtain Current_GOB by gob_header->GN */
		Current_GOB = ((Image->type==_QCIF) ?
				((gob_header->GN-1)>>1) : gob_header->GN-1);
		#ifdef DEBUG_ON
		if (Current_GOB>Number_GOB) {
			ERROR_LINE();
			printf("GN read error: GN=%d, Current_GOB=%d>%d\n",
				gob_header->GN, Current_GOB, Number_GOB);
			exit(ERROR_BOUNDS);
		}
		#endif

		/* here we have got GBSC and GN */
		read_GOB_header_tail(gob_header);

		/* decode each MB in the GOB */
		Last_MTYPE = Last_MVDH = Last_MVDV = 0;
		Current_MB = -1;
		while (read_MB_header(&Current_MB, gob_header->GQUANT,
				mb_header)!=GBSC_code) {
			MTYPE = mb_header->MTYPE;
			MVDH = mb_header->MVDH;
			MVDV = mb_header->MVDV;
			#ifdef DEBUG_ON
			if (Current_MB>=Number_MB) {
				ERROR_LINE();
				printf("MB out of range: %d >= %d.\n",
					Current_MB, Number_MB);
				exit(ERROR_BOUNDS);
			}
			#endif
			decode_MB();

			#ifdef CTRL_STAT_MTYPE
			MTYPE_count[MTYPE]++;
			MTYPE_count[MB_NOT_TRANSMIT]--;
			#endif
		}
	}
	/* we have got a PSC here */
}

/*************************************************************************
 *
 *	Name:	       	decode_MB()
 *	Description:	decode each block in current MB
 *	Input:          none
 *	Return:	       	none
 *	Side effects:	entries of rec_frame will be changed
 *	Date: 96/04/29	Author: Chu Ching-Wen in N.T.H.U., Taiwan
 *
 *************************************************************************/
static void decode_MB(void)
{
	DEBUG("decode_MB");
	int32 Y_memloc, CbCr_memloc;
	int16 CBPmask = 0x20;	/* (10 0000) */
	int16 *block;
	ComponentType Btype;	/* for block type (= _Y, _U or _V) */
	int16 old_stepsize;

	/* here we have read the MB header */

	/* set memloc for writing into frame stores */
	Y_memloc = YGOB_memloc[Current_GOB] + YMB_memloc[Current_MB];
	CbCr_memloc = GOB_memloc[Current_GOB] + MB_memloc[Current_MB];

	/* to save current decoded MB */
	SET_memloc(reco_frame, Y_memloc, CbCr_memloc);

	/* reference MB */
	if (MVD_used[MTYPE] && ((MVDH!=0)||(MVDV!=0))) {
		/* obtain memloc by current MV for reference frame */
		if (MVDV>=0) {
			Y_memloc += (YMVDV_memloc[MVDV] + MVDH);
			CbCr_memloc += (MVDV_memloc[MVDV] + (MVDH>>1));
		} else {
			Y_memloc += (-YMVDV_memloc[-MVDV] + MVDH);
			CbCr_memloc += (-MVDV_memloc[-MVDV] + (MVDH>>1));
		}
	}
	SET_memloc(last_frame, Y_memloc, CbCr_memloc);

	if (!TCOEFF_used[MTYPE]) {
		/* without TCOEFF */
		if (MVD_used[MTYPE])
			copy_MB(reco_frame, last_frame);
	} else {
		/* with TCOEFF */
		/* for blocks specified by CBP */
		if (Intra_used[MTYPE])
			mb_header->CBP = 0x3f;	/* all blocks */
		for (Current_B=0; Current_B<6; Current_B++,CBPmask>>=1) {
			Btype = Block_type[Current_B];
			block = MBbuf[Current_B];

			if (mb_header->CBP&CBPmask) {
				/* receive TCOEFF */
				Itransfer_TCOEFF(Intra_used[MTYPE], block);
				if (MQUANT_used[MTYPE]) {
					Iquantize(Intra_used[MTYPE], block, mb_header->MQUANT);
				} else {
					Iquantize(Intra_used[MTYPE], block, gob_header->GQUANT);
				}
				use_IDCT(block, block);

				if (!Intra_used[MTYPE]) {
					/* save residual block to block[] */
					save_residual(last_frame->fs[Btype],
						block, Filter_used[MTYPE]);
				}
				clip_reconstructed_block(block);

				/* write 1 block to reco_frame */
				write_block(reco_frame->fs[Btype], block);
			} else if (MVD_used[MTYPE]) {
				copy_block(reco_frame->fs[Btype],
					last_frame->fs[Btype]);
			}
		}
	}
}

/*************************************************************************
 *
 *	Name:		set_image_type()
 *	Description:	set the Image parameters for CCITT coding
 *	Input:          none
 *	Return:		none
 *	Side effects:	entries of Image, ?_memloc, ?_posX and ?_posY
 *			will be changed
 *	Date: 96/04/16	Author: Chu Ching-Wen in N.T.H.U., Taiwan
 *
 *************************************************************************/
static void set_image_type(void)
{
	DEBUG("set_image_type");
	ComponentType type;
	int16 i, j;
	int32 last;

	switch (Image->type) {
	case _NTSC:	/* Parameters for NTSC type */
		Number_GOB = 10;
		Number_MB = 33;
		Image->Width = Image->width[_Y] = 352;
		Image->Height = Image->height[_Y] = 240;
		Image->width[_Cb] = Image->width[_Cr] = 176;
		Image->height[_Cb] = Image->height[_Cr] = 120;
		break;
	case _CIF:	/* Parameters for CIF type */
		Number_GOB = 12;
		Number_MB = 33;
		Image->Width = Image->width[_Y] = 352;
		Image->Height = Image->height[_Y] = 288;
		Image->width[_Cb] = Image->width[_Cr] = 176;
		Image->height[_Cb] = Image->height[_Cr] = 144;
		break;
	case _QCIF:	/* Parameters for QCIF type */
		Number_GOB = 3;
		Number_MB = 33;
		Image->Width = Image->width[_Y] = 176;
		Image->Height = Image->height[_Y] = 144;
		Image->width[_Cb] = Image->width[_Cr] = 88;
		Image->height[_Cb] = Image->height[_Cr] = 72;
		break;
	default:
		ERROR_LINE();
		printf("Unknown image type: %d\n", Image->type);
		exit(ERROR_OTHERS);
	}

	for (type=0; type<NUMBER_OF_COMPONENTS; type++)
		Image->len[type] = (int32) Image->width[type]
				* Image->height[type] * sizeof(unsigned char);

	/* Row-major in MEM->data => Image->width[] should be set */

	/* set GOB_memloc for Y, Cb and Cr */
	if (Image->type==_QCIF) {
		YGOB_memloc[0] = GOB_memloc[0] = 0;
		GOB_posX[0] = GOB_posY[0] = 0;
		for (Current_GOB=1; Current_GOB<Number_GOB; Current_GOB++) {
			YGOB_memloc[Current_GOB] = YGOB_memloc[Current_GOB-1]
					+ (Image->width[_Y] * 48);
			GOB_memloc[Current_GOB] = GOB_memloc[Current_GOB-1]
					+ (Image->width[_Cb] * 24);

			GOB_posX[Current_GOB] = 0;
			GOB_posY[Current_GOB] = GOB_posY[Current_GOB-1] + 48;
		}
	} else {
		YGOB_memloc[0] = GOB_memloc[0] = 0;
		YGOB_memloc[1] = 176;
		GOB_memloc[1] = 88;

		GOB_posX[0] = GOB_posY[0] = 0;
		GOB_posX[1] = 176;
		GOB_posY[1] = 0;
		for (Current_GOB=2; Current_GOB<Number_GOB; Current_GOB+=2) {
			/* (Current_GOB)-th GOB */
			YGOB_memloc[Current_GOB] = YGOB_memloc[Current_GOB-2]
					+ (Image->width[_Y] * 48);
			GOB_memloc[Current_GOB] = GOB_memloc[Current_GOB-2]
					+ (Image->width[_Cb] * 24);
			GOB_posX[Current_GOB] = 0;
			GOB_posY[Current_GOB] = GOB_posY[Current_GOB-2] + 48;

			/* (Current_GOB+1)-th GOB */
			YGOB_memloc[Current_GOB+1] = YGOB_memloc[Current_GOB]
					+ 176;
			GOB_memloc[Current_GOB+1] = GOB_memloc[Current_GOB]
					+ 88;
			GOB_posX[Current_GOB+1] = 176;
			GOB_posY[Current_GOB+1] = GOB_posY[Current_GOB];
		}
	}

	/* set MB_memloc for Y */
	Current_MB = 0;
	last = 0;
	for (i=0; i<3; i++) {
		MB_posX[Current_MB] = 0;
		MB_posY[Current_MB] = i * 16;
		YMB_memloc[Current_MB++] = last;
		for (j=1; j<11; j++,Current_MB++) {
			YMB_memloc[Current_MB] = YMB_memloc[Current_MB-1] + 16;
			MB_posX[Current_MB] = j * 16;
			MB_posY[Current_MB] = i * 16;
		}
		last += (Image->width[_Y] * 16);
	}
	/* set MB_memloc for Cb and Cr */
	Current_MB = last = 0;
	for (i=0; i<3; i++) {
		MB_memloc[Current_MB++] = last;
		for (j=1; j<11; j++,Current_MB++)
			MB_memloc[Current_MB] = MB_memloc[Current_MB-1] + 8;
		last += (Image->width[_Cb] * 8);
	}
	Current_MB = 0;

	/* set B_memloc for Y, Cb and Cr */
	B_memloc[0] = 0;
	B_memloc[1] = B_memloc[0] + 8;
	B_memloc[2] = B_memloc[0] + (Image->width[_Y] * 8);
	B_memloc[3] = B_memloc[1] + (Image->width[_Y] * 8);
	B_memloc[4] = 0;
	B_memloc[5] = 0;
	Current_B = 0;

	/* set MVDV_memloc for Y (MVDV should be abs(MVDV))*/
	YMVDV_memloc[0] = 0;
	for (MVDV=1; MVDV<31; MVDV++)
		YMVDV_memloc[MVDV] = YMVDV_memloc[MVDV-1]
					+ Image->width[_Y];
	/* set MVDV_memloc for Cb and Cr (MVDV should be abs(MVDV))*/
	/* NOTE: MVDV instead of MVDV/2 */
	MVDV_memloc[0] = 0;
	for (MVDV=1; MVDV<31; MVDV+=2) {
		MVDV_memloc[MVDV] = MVDV_memloc[MVDV-1];
		MVDV_memloc[MVDV+1] = MVDV_memloc[MVDV-1]
					+ Image->width[_Cb];
	}
	MVDV = MVDH = 0;
}

/*************************************************************************
 *
 *	Name:		help()
 *	Description:	print (level 0) help information about program
 *	Input:          none
 *	Return:		none
 *	Side effects:
 *	Date: 96/05/09	Author: Chu Ching-Wen in N.T.H.U., Taiwan
 *
 *************************************************************************/
static void help(void)
{
	DEBUG("help");

#ifdef X11
	printf("Usage: %s [-QCIF -CIF -NTSC] [-a -b -d -e -h -i -k -o -r -s -w -z] \n",
		command);
	printf("\t-w            open a window to display          {DEFAULT: no window}\n");
	printf("\t-e            expand display window by 2        {DEFAULT: no expansion}\n");
#else
	printf("Usage: %s [-QCIF -CIF -NTSC] [-a -b -d -h -i -k -o -r -s -z] \n",
		command);
#endif
	printf("\t-h [<n>]      the degree of help infomation. (set <n> for more)\n");
	printf("\t-a <n>        the first file ID is <n>.         {DEFAULT: 0}\n");
	printf("\t-o <output_frame_file_prefix>                   {DEFAULT: to display}\n");
	printf("\t-z <Y_suffix> <Cb_suffix> <Cr_suffix>           {DEFAULT: %s %s %s}\n",
		Y_FILE_SUFFIX, Cb_FILE_SUFFIX, Cr_FILE_SUFFIX);
	printf("Decoder Options:\n");
	printf("\t-d <bitstream_filename>\n");
	printf("Encoder Options:\n");
	printf("\t-i <input_frame_file_prefix>                    {DEFAULT: from capture}\n");
	printf("\t-s <bitstream_filename>             {DEFAULT: <input_frame_prefix>%s}\n",
		STREAM_FILE_SUFFIX);
	printf("\t-b <n>        the last file ID is <n>.          {DEFAULT: 999}\n");
	printf("\t-r <n1> <n2>  rate: <n1> kbps, <n2> fps.        {DEFAULT: %.2f %.2f}\n",
		Bit_rate/1000, Frame_rate);
	printf("\t-p <n>        bit rate under <n> bit/pixel.     {DEFAULT: no use}\n");
	printf("\t-m <n>        set motion estimation algorithm.  {DEFAULT: %d}\n",
		THREE_STEP_SEARCH);
	printf("\t-k <n>        encode one frame per <n> frames.  {DEFAULT: 1}\n");
	printf("\t-QCIF -CIF -NTSC    picture type                {DEFAULT:-QCIF}\n");
	printf("\t                    QCIF: 176x144, CIF: 352x288, NTSC: 352x240\n");

	printf("Encoder Example: \t%s -i salesman -s sales.261\n",
		command);
	printf("Decoder Example: \t%s -d sales.261 -o reconstructed_salesman\n",
		command);
	printf("\n");
}

/*************************************************************************
 *
 *	Name:		help1()
 *	Description:	print (level 1) help information about program
 *	Input:          none
 *	Return:		none
 *	Side effects:
 *	Date: 96/04/16	Author: Chu Ching-Wen in N.T.H.U., Taiwan
 *
 *************************************************************************/
static void help1(void)
{
	DEBUG("help1");

	printf("\nUsage: %s [-QCIF -CIF -NTSC] [-a -b -d -h -i -k -o -s -z] \n",
		command);
	printf("Encoder Options:\n");

	/* motion estimation algo. */
	printf("\t-m <n>        set motion estimation algorithm. {DEFAULT: %d}\n",
		THREE_STEP_SEARCH);
	printf("\t\t-m %d     use full_search\n",	FULL_SEARCH);
	printf("\t\t-m %d     use three_step_search\n", THREE_STEP_SEARCH);
	printf("\t\t-m %d     use new_three_step_search\n", NEW_THREE_STEP_SEARCH);
	printf("\t\t-m %d     use my_search\n", MY_SEARCH);
	printf("\n");
}
