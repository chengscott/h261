/*************************************************************************
 *
 *	Name:		globals.h
 *	Description:	global difinitions, constants, variables, and
 *			controls for all (DEBUG etc.)
 *	Date: 96/05/09	Author: Chu Ching-Wen in N.T.H.U., Taiwan
 *
 *************************************************************************/

#ifndef GLOBAL_DONE
#define GLOBAL_DONE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/*************************************************************************/
/* system definition */
/*#define DOS		/* codes for PC-DOS (not linux) */
//#define X11 		/* display for X11 on linux */
/* if not defined DOS) => Linux version */

#ifdef DOS
#include <alloc.h>
#define malloc farmalloc
#endif

/*************************************************************************/
/* control for all subroutines */
/*#define DEBUG_ON 	/* debug on (not off) */

/*************************************************************************/
/* variables' types definition */
typedef short int int16;/* 16-bits integer(-32,768 to 32,767) */
typedef long int32;	/* 32-bits integer(-2,147,483,648 to 2,147,483,647) */
typedef unsigned char byte;	/* 1 byte (8 bits) */
typedef unsigned long bytes4;	/* 4 bytes (32 bits) */
typedef enum {FALSE=0, TRUE=1} boolean;	/* for boolean function */
typedef enum {_CIF=0, _QCIF=1, _NTSC=2} ImageType;	/* image types */
typedef enum {_Y=0, _Cb=1, _Cr=2} ComponentType;	/* component types */
#define NUMBER_OF_COMPONENTS 3

/*************************************************************************/
/* H.261 standard */

/* block definition */
#define BLOCKSIZE 64
#define BLOCKWIDTH 8
#define BLOCKHEIGHT 8

/* for MTYPE */
#define INTRA		0
#define INTRA_MQUANT	1
#define INTER		2
#define INTER_MC	4	/* 4 -> 7 to use filter */
#define INTER_MC_TCOEFF	5	/* 5 -> 8 to use filter */
#define MB_NOT_TRANSMIT	10	/* extra MTYPE: do not trams. MB */

/*************************************************************************/
/* CCITT p*64 header info. (H.261 sec.4) */

/* picture layer */
#define PSC 0x10	/* Picture Start Code (0000 0000 0000 0001 0000) */
#define PSC_LENGTH 20	/* LENGTH of PSC */
#define PIC_HEADER struct Picture_Header
PIC_HEADER {
			/* PSC has defined yet (20-bits) */
	int16 TR;	/* Temporal Reference (5-bits) */
	int16 PTYPE;	/* Picture TYPE info. (6-bits) */
	int16 PEI;	/* Picture Extra Insertion info. */
	int16 PSPARE;	/* Picture SPARE info. (8-bits) */
};

/* GOB layer */
#define GBSC 1		/* Group of Block Start Code (0000 0000 0000 0001) */
#define GBSC_LENGTH 16  /* LENGTH of GBSC */
/* NOTE: The first 16-bits in PSC and GBSC are the same.
	 The 4-bits following GBSC must NOT be '0000', because
	 PSC(20 bits) and {GBSC(16-bits)+this 4-bits} can't be the same */
#define GOB_HEADER struct GOB_Header
GOB_HEADER {
			/* GBSC has defined yet (16-bits) */
	int16 GN;	/* Group number in a single GOB (1st: 1) (4-bits) */
	int16 GQUANT;	/* Gob QUANTizer info. (5-bits) */
	int16 GEI;	/* Gob Extra Insertion info. (1-bit)*/
	int16 GSPARE;	/* Gob SPARE info. (8-bits) */
};
#define GBSC_code 35	/* in TABLE 1/H.261 */

/* MB layer */
#define MB_HEADER struct MB_Header
MB_HEADER {
	int16 MBA;	/* MacrBlock Address (VLC) */
	int16 MTYPE;	/* Macroblock coding Type (VLC) */
	int16 MQUANT;	/* Macorblock Quantization factor (5-bits)*/
	int16 MVDH;	/* Motion Vector Data for Horizontal offset (VLC) */
	int16 MVDV;	/* Motion Vector Data for Vertical offset (VLC) */
	int16 CBP;     	/* Coded Block Pattern (5-bits) */
};
#define MBAstuffing 34	/* in TABLE 1/H.261 */

/* block layer */
#define EOB 0		/* in TABLE 5/H.261 */
#define ESCAPE 0x1b01	/* (011011 00000001) in TABLE 5/H.261 */

/*************************************************************************/
/* define for coded & image files of different types */
#define MAX_FILENAME_LEN 50		/* max. length of filenames */
					/* (coded or image files) */
#define STREAM_FILE_SUFFIX ".261"	/* coded filename suffix (?.261) */
#define Y_FILE_SUFFIX ".Y"		/* image filename suffix for Y */
#define Cb_FILE_SUFFIX ".U"		/* image filename suffix for Cb */
#define Cr_FILE_SUFFIX ".V"		/* image filename suffix for Cr */

/*************************************************************************/
/* motion estimation algo. */
/* if (me_algo==%d) => ... */
#define FULL_SEARCH         	0	/* use full_search_ME() */
#define THREE_STEP_SEARCH      	1	/* use three_step_search_ME() */
#define NEW_THREE_STEP_SEARCH	2	/* use new_three_step_search_ME() */

/*************************************************************************/
/* for debug */
/* DEBUG() is the first line in each function for debuging */
/* ERROR_LINE() shows the routine name (given by DEBUG()) & exit-line number */
#ifdef DEBUG_ON
#define DEBUG(name) static char RoutineName[]= name
#define ERROR_LINE() printf("\nF>%s R>%s L>%d \n", \
		__FILE__,RoutineName,__LINE__)
#define PRINT_BLOCK(block) {int16 i; for(i=0;i<64;i++) {if (i%8==0) printf("\n"); printf(" %d ", block[i]);} getchar();}
#else
#define DEBUG(name) char DUMMY
#define ERROR_LINE() ;
#endif

/* error types */
#define ERROR_ARGV 1		/* argument's error */
#define ERROR_BOUNDS 2		/* out of corrent range */
#define ERROR_EOF 3		/* EOF at wrong position */
#define ERROR_EOB 4		/* EOB at wrong position */
#define ERROR_HUFFMAN 5		/* error at huffman-coding */
#define ERROR_IO 6		/* error at file-IO */
#define ERROR_HEADER 7		/* error at header-IO */
#define ERROR_MEMORY 8		/* error at memory-access */
#define ERROR_DISP 9		/* error at display */
#define ERROR_OTHERS 10		/* error at other places */

/*************************************************************************/
/* other struceures definitions */
/* Encoder-Huffman-table */
#define EHUFF struct Encoder_Huffman
EHUFF {
	char table_name[8];
	int16 n;	/* the value */
	int16 *Hlen;	/* length of the code */
	int16 *Hcode;	/* the code */
};

/* Decoder-Huffman-table */
#define DHUFF struct Decoder_Huffman
DHUFF {
	char table_name[8];
	int16 number_of_states;	/* # of used states (start from 1 (state 0)) */
	int16 *next_state[2]; /* next_state[LEFT or RIGHT] from current state */
};

#define MEM struct Memory_Construct
MEM {
	int16 width;
	int16 height;
	int32 memloc;
	unsigned char *data;
};

/* frame store */
#define FSTORE struct Frame_Store
FSTORE {
	MEM *fs[NUMBER_OF_COMPONENTS];
};

/* image infomation */
#define IMAGE struct Image_Information
IMAGE {
	/* image type (= _CIF, _QCIF, or _NTSC) */
	ImageType type;

	/* bitstream file name (?.261) */
	char *Stream_filename;

	/* image height & width */
	int16 Height;
	int16 Width;

	/* height & width of each component (352x288 or ...) */
	int16 height[NUMBER_OF_COMPONENTS];
	int16 width[NUMBER_OF_COMPONENTS];
	int32 len[NUMBER_OF_COMPONENTS];

	boolean display;

	/* frame files */
	boolean read_from_files;
	boolean write_to_files;
	char frame_suffix[NUMBER_OF_COMPONENTS][MAX_FILENAME_LEN];
	/* input frame files */
	char input_frame_prefix[MAX_FILENAME_LEN];
	/* output frame (reconstructed frame) files */
	char output_frame_prefix[MAX_FILENAME_LEN];

};

#define MAKE_STRUCTURE(name, st) {\
		if (!(name = (st *) malloc(sizeof(st)))) {\
			ERROR_LINE();\
			printf("Cannot make a structure.\n");\
			exit(ERROR_MEMORY);\
		}}

/*************************************************************************/
/* replace x%32 by '&0x1f' operation */
#define MOD_32(x)	((x) & (0x1f))

/*************************************************************************/
/* function prototypes */
#include "prototyp.h"

#endif
