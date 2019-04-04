/*************************************************************************
 *
 *	Name:		memory.c
 *	Description:	basic memory allocation/reading/writing operations
 *	Date: 96/04/29	Author: Chu Ching-Wen in N.T.H.U., Taiwan
 *
 *************************************************************************/

#include "globals.h"

/*************************************************************************/
/* public */
extern MEM *make_MEM(int16 width, int16 height);
extern MEM *make_MEM_ptr(int16 width, int16 height);
extern void free_MEM(MEM *mem);
extern FSTORE *make_FS(int16 width, int16 height);
extern FSTORE *make_FS_ptr(int16 width, int16 height);
extern void free_FS(FSTORE *fs);
extern IMAGE *make_IMAGE(void);
extern void alloc_mem_encoder(void);
extern void alloc_mem_decoder(void);
extern void copy_FS(FSTORE *fs_des, FSTORE *fs_src);
extern void copy_block(MEM *des, MEM *src);
extern void copy_MB(FSTORE *des, FSTORE *src);
extern void disturb_MB(FSTORE *des, FSTORE *src1, FSTORE *src2);
extern void read_MB(int16 MBbuf[6][64], FSTORE *Fs);
extern void write_block(MEM *mem, int16 *block);
extern void load_residual(MEM *mem, int16 *block, boolean with_filter);
extern void save_residual(MEM *mem, int16 *block, boolean with_filter);

extern IMAGE *Image;		/* global info. of image */

/*************************************************************************/
/* Look-up-table for obtaining memloc in MEM: */

/* Position of the upper-left connor in the GN-th GOB for Y-type */
/* The first entry [0] is useless => [1] is the first actural entry */
extern int32 YGOB_memloc[12+1];
/* Position of the upper-left connor in the GN-th GOB for Cb, Cr */
/* The first entry [0] is useless => [1] is the first actural entry */
extern int32 GOB_memloc[12+1];

/* Position of the upper-left connor in the CurrentMB-th MB for Y-type */
extern int32 YMB_memloc[33];
/* Position of the upper-left connor in the CurrentMB-th MB for Cb, Cr */
extern int32 MB_memloc[33];

/* Position of the upper-left connor in the Current_B-th block */
extern int32 B_memloc[6];

/* Distance due to MVDV for Y-type */
extern int32 YMVDV_memloc[31];
/* Distance due to MVDV for Cb, Cr */
extern int32 MVDV_memloc[31];

extern int16 Current_B;

/*************************************************************************/
/* private */
static void loop_filter(unsigned char *memloc, int16 *output, int16 mem_width);

/*************************************************************************
 *
 *	Name:		make_MEM()
 *	Description:	make a MEM structure to store one conponent of frame
 *	Input:          the width and height of frame
 *			(the size of data is (width * height))
 *	Return:		the pointer to the constructed MEM structure
 *	Side effects:   the constructed structure will have initial value
 *	Date: 96/04/16	Author: Chu Ching-Wen in N.T.H.U., Taiwan
 *
 *************************************************************************/
MEM *make_MEM(int16 width, int16 height)
{
	DEBUG("make_MEM");
	MEM *mem;
	int32 size = (int32) width * height * sizeof(unsigned char);

	MAKE_STRUCTURE(mem, MEM);
	mem->width = width;
	mem->height = height;
	if (!(mem->data=(unsigned char *) malloc(size))) {
		ERROR_LINE();
		printf("Cannot allocate data storage for MEM.\n");
		exit(ERROR_MEMORY);
	}
	mem->memloc = 0;
	memset(mem->data, 0, size);

	return mem;
}

/*************************************************************************
 *
 *	Name:		make_MEM_ptr()
 *	Description:	make a pointer to a MEM structure
 *			(we do not allocate memory for mem->data)
 *	Input:          the width and height of frame
 *	Return:		the pointer to the constructed MEM structure
 *	Side effects:   the constructed structure will have initial value
 *	Date: 96/04/16	Author: Chu Ching-Wen in N.T.H.U., Taiwan
 *
 *************************************************************************/
MEM *make_MEM_ptr(int16 width, int16 height)
{
	DEBUG("make_MEM_ptr");
	MEM *mem;

	MAKE_STRUCTURE(mem, MEM);
	mem->width = width;
	mem->height = height;
	mem->memloc = 0;

	return mem;
}

/*************************************************************************
 *
 *	Name:		free_MEM()
 *	Description:	free a MEM structure
 *	Input:		the pointer to the constructed MEM structure
 *	Return:         none
 *	Side effects:   the memory of mem will be free
 *	Date: 96/04/29	Author: Chu Ching-Wen in N.T.H.U., Taiwan
 *
 *************************************************************************/
void free_MEM(MEM *mem)
{
	DEBUG("free_MEM");

	free(mem->data);
	free(mem);
}

/*************************************************************************
 *
 *	Name:		make_FS()
 *	Description:	make a FSTORE structure to store a frame
 *	Input:          the width and height of Y of frame store
 *	Return:		the pointer to the constructed FSTORE structure
 *	Side effects:   the constructed structure will have initial value
 *	Date: 96/04/29	Author: Chu Ching-Wen in N.T.H.U., Taiwan
 *
 *************************************************************************/
FSTORE *make_FS(int16 width, int16 height)
{
	DEBUG("make_FS");
	FSTORE *fs;
	int16 width_2 = width >> 1;
	int16 height_2 = height >> 1;

	MAKE_STRUCTURE(fs, FSTORE);
	fs->fs[_Y] = make_MEM(width, height);
	fs->fs[_Cb] = make_MEM(width_2, height_2);
	fs->fs[_Cr] = make_MEM(width_2, height_2);

	return fs;
}

/*************************************************************************
 *
 *	Name:		make_FS_ptr()
 *	Description:	make a pointer to a FSTORE structure
 *			(we do not allocate memory for fs->fs[]->data)
 *	Input:          the width and height of Y of frame store
 *	Return:		the pointer to the constructed FSTORE structure
 *	Side effects:   the constructed structure will have initial value
 *	Date: 96/04/29	Author: Chu Ching-Wen in N.T.H.U., Taiwan
 *
 *************************************************************************/
FSTORE *make_FS_ptr(int16 width, int16 height)
{
	DEBUG("make_FS_ptr");
	FSTORE *fs;
	int16 width_2 = width >> 1;
	int16 height_2 = height >> 1;

	MAKE_STRUCTURE(fs, FSTORE);
	fs->fs[_Y] = make_MEM_ptr(width, height);
	fs->fs[_Cb] = make_MEM_ptr(width_2, height_2);
	fs->fs[_Cr] = make_MEM_ptr(width_2, height_2);

	return fs;
}

/*************************************************************************
 *
 *	Name:		free_FS()
 *	Description:	free a FSTORE structure
 *	Input:		the pointer to the constructed FSTORE structure
 *	Return:		none
 *	Side effects:   the memory of fs will be free
 *	Date: 96/04/29	Author: Chu Ching-Wen in N.T.H.U., Taiwan
 *
 *************************************************************************/
void free_FS(FSTORE *fs)
{
	DEBUG("free_FS");

	free_MEM(fs->fs[_Y]);
	free_MEM(fs->fs[_Cb]);
	free_MEM(fs->fs[_Cr]);
	free(fs);
}

/*************************************************************************
 *
 *	Name:		make_IMAGE()
 *	Description:	make a IMAGE structure
 *	Input:		none
 *	Return:		the pointer to the constructed IMAGE structure
 *	Side effects:   the constructed structure will have initial value
 *	Date: 96/04/16	Author: Chu Ching-Wen in N.T.H.U., Taiwan
 *
 *************************************************************************/
IMAGE *make_IMAGE(void)
{
	DEBUG("make_structures");
	IMAGE *image;
	ComponentType type;

	/* make an image structure and installs it as the current image */
	MAKE_STRUCTURE(image, IMAGE);
	image->Stream_filename = NULL;
	image->Height = image->Width = 0;

	/* display image after or before codec */
	#ifdef X11
	image->display = TRUE;
	#else
	image->display = FALSE;
	#endif

	/* input frame files and output frame files (reconstructed files) */
	image->read_from_files = image->write_to_files = FALSE;
	*image->input_frame_prefix = *image->output_frame_prefix = '\0';
	for (type=0; type<NUMBER_OF_COMPONENTS; type++) {
		image->height[type] = image->width[type] = 0;
		*image->frame_suffix[type] = '\0';
	}

	return image;
}

/*************************************************************************
 *
 *	Name:		alloc_mem_encoder()
 *	Description:	allocate memory structures for encoder
 *	Input:		none
 *	Return:		none
 *	Side effects:   the constructed structures will have initial value
 *	Date: 96/04/16	Author: Chu Ching-Wen in N.T.H.U., Taiwan
 *
 *************************************************************************/
void alloc_mem_encoder(void)
{
	DEBUG("alloc_mem_encoder");
	extern int16 Number_GOB;
	extern int16 Number_MB;
	extern FSTORE *ori_frame, *rec_frame, *rec_frame_backup, *ref_frame;
	extern int16 *MTYPE_frame, *MVDH_frame, *MVDV_frame, *Last_update;
	extern int16 Size_frame;

	/* make structure with size of (# of MB in a frame) */
	Size_frame = Number_GOB * Number_MB * sizeof(int16);
	MTYPE_frame = (int16 *) malloc(Size_frame);
	MVDH_frame = (int16 *) malloc(Size_frame);
	MVDV_frame = (int16 *) malloc(Size_frame);
	Last_update = (int16 *) malloc(Size_frame);
	if ((!MTYPE_frame) || (!MVDH_frame) || (!MVDV_frame) || (!Last_update)) {
		ERROR_LINE();
		printf("Cannot allocate structure.\n");
		exit(ERROR_MEMORY);
	}

	/* make frame stores */
	ori_frame = make_FS(Image->width[_Y], Image->height[_Y]);
	rec_frame = make_FS(Image->width[_Y], Image->height[_Y]);
	rec_frame_backup = make_FS(Image->width[_Y], Image->height[_Y]);
	ref_frame = make_FS_ptr(Image->width[_Y], Image->height[_Y]);
}

/*************************************************************************
 *
 *	Name:		alloc_mem_decoder()
 *	Description:	allocate memory structures for decoder
 *	Input:		none
 *	Return:		none
 *	Side effects:   the constructed structures will have initial value
 *	Date: 96/04/16	Author: Chu Ching-Wen in N.T.H.U., Taiwan
 *
 *************************************************************************/
void alloc_mem_decoder(void)
{
	DEBUG("alloc_mem_decoder");
	extern int16 Number_GOB;
	extern int16 Number_MB;
	extern FSTORE *last_frame, *reco_frame;
	extern int16 *MTYPE_frame, *MVDH_frame, *MVDV_frame;
	extern int16 Size_frame;

	/* make frame stores */
	last_frame = make_FS(Image->width[_Y], Image->height[_Y]);
	reco_frame = make_FS(Image->width[_Y], Image->height[_Y]);
}

/*************************************************************************
 *
 *	Name:		copy_FS()
 *	Description:	copy a frame store (fs_src) to the other (fs_des)
 *	Input:		the pointers to the source and destination frame
 *			stores
 *	Return:		none
 *	Side effects:	entries of fs_des will be changed
 *	Date: 96/04/16	Author: Chu Ching-Wen in N.T.H.U., Taiwan
 *
 *************************************************************************/
void copy_FS(FSTORE *fs_des, FSTORE *fs_src)
{
	DEBUG("copy_FS");

	memcpy(fs_des->fs[_Y]->data, fs_src->fs[_Y]->data, Image->len[_Y]);
	memcpy(fs_des->fs[_Cb]->data, fs_src->fs[_Cb]->data,Image->len[_Cb]);
	memcpy(fs_des->fs[_Cr]->data, fs_src->fs[_Cr]->data, Image->len[_Cr]);
}

/*************************************************************************
 *
 *	Name:		copy_block()
 *	Description:	copy a blocke (in src) to the other (in des)
 *	Input:		the pointers to the source and destination block
 *	Return:		none
 *	Side effects:	entries of des will be changed
 *	Date: 96/04/16	Author: Chu Ching-Wen in N.T.H.U., Taiwan
 *
 *************************************************************************/
void copy_block(MEM *des, MEM *src)
{
	DEBUG("copy_block");
	int16 i;
	unsigned char *des_ptr, *src_ptr;

	des_ptr = des->data + des->memloc + B_memloc[Current_B];
	src_ptr = src->data + src->memloc + B_memloc[Current_B];
	for (i=0; i<8; i++) {
		memcpy(des_ptr, src_ptr, 8*sizeof(unsigned char));
		des_ptr += des->width;
		src_ptr += src->width;
	}
}

/*************************************************************************
 *
 *	Name:		copy_MB()
 *	Description:	copy a macro-block (in src) to the other (in des)
 *	Input:		the pointers to the source and destination frame
 *			stores
 *	Return:		none
 *	Side effects:	one macro-block in des will be changed
 *	Date: 96/04/16	Author: Chu Ching-Wen in N.T.H.U., Taiwan
 *
 *************************************************************************/
void copy_MB(FSTORE *des, FSTORE *src)
{
	DEBUG("copy_MB");
	int16 i;
	unsigned char *des_ptr, *src_ptr;

	des_ptr = des->fs[_Y]->data + des->fs[_Y]->memloc;
	src_ptr = src->fs[_Y]->data + src->fs[_Y]->memloc;
	for (i=0; i<16; i++) {
		memcpy(des_ptr, src_ptr, 16*sizeof(unsigned char));
		des_ptr += Image->width[_Y];
		src_ptr += Image->width[_Y];
	}

	des_ptr = des->fs[_Cb]->data + des->fs[_Cb]->memloc;
	src_ptr = src->fs[_Cb]->data + src->fs[_Cb]->memloc;
	for (i=0; i<8; i++) {
		memcpy(des_ptr, src_ptr, 8*sizeof(unsigned char));
		des_ptr += Image->width[_Cb];
		src_ptr += Image->width[_Cb];
	}

	des_ptr = des->fs[_Cr]->data + des->fs[_Cr]->memloc;
	src_ptr = src->fs[_Cr]->data + src->fs[_Cr]->memloc;
	for (i=0; i<8; i++) {
		memcpy(des_ptr, src_ptr, 8*sizeof(unsigned char));
		des_ptr += Image->width[_Cr];
		src_ptr += Image->width[_Cr];
	}
}

/*************************************************************************
 *
 *	Name:		read_MB()
 *	Description:	read a macro-block from a frame store (Fs) to an
 *			interger array (MBbuf)
 *	Input:		the pointers to the source (frame store) and
 *			the destination (integer array)
 *	Return:		none
 *	Side effects:	MBbuf[][] will be stored
 *	Date: 96/04/16	Author: Chu Ching-Wen in N.T.H.U., Taiwan
 *
 *************************************************************************/
void read_MB(int16 MBbuf[6][64], FSTORE *Fs)
{
	DEBUG("read_MB");
	int16 i, j, dist;
	unsigned char *loc0, *loc1;
	int16 *block0, *block1;

	/* Y */
	loc0 = Fs->fs[_Y]->data + Fs->fs[_Y]->memloc;
	dist = Fs->fs[_Y]->width - 16;

	/* for Y1, Y2 blocks */
	block0 = MBbuf[0];
	block1 = MBbuf[1];
	for (i=0; i<8; i++) {
		for (j=0; j<8; j++)	*(block0++) = *(loc0++);
		for (j=0; j<8; j++)	*(block1++) = *(loc0++);
		loc0 += dist;
	}
	/* for Y3, Y4 blocks */
	block0 = MBbuf[2];
	block1 = MBbuf[3];
	for (i=0; i<8; i++) {
		for (j=0; j<8; j++)	*(block0++) = *(loc0++);
		for (j=0; j<8; j++)	*(block1++) = *(loc0++);
		loc0 += dist;
	}

	/* Cb and Cr */
	loc0 = Fs->fs[_Cb]->data + Fs->fs[_Cb]->memloc;
	loc1 = Fs->fs[_Cr]->data + Fs->fs[_Cr]->memloc;
	dist = Fs->fs[_Cb]->width - 8;

	block0 = MBbuf[4];
	block1 = MBbuf[5];
	for (i=0; i<8; i++) {
		for (j=0; j<8; j++) {
			*(block0++) = *(loc0++);
			*(block1++) = *(loc1++);
		}
		loc0 += dist;
		loc1 += dist;
	}
}

/*************************************************************************
 *
 *	Name:		write_MB()
 *	Description:	write a macro-block from a block to a MEM
 *			structure
 *	Input:		the pointers to a block and a MEM structure
 *	Return:		none
 *	Side effects:	one block in mem will be changed
 *	Date: 96/04/16	Author: Chu Ching-Wen in N.T.H.U., Taiwan
 *
 *************************************************************************/
void write_block(MEM *mem, int16 *block)
{
	DEBUG("write_block");
	int16 i, j;
	unsigned char *loc;

	loc = mem->data + mem->memloc + B_memloc[Current_B];
	for (i=0; i<BLOCKHEIGHT; i++) {
		for (j=0; j<BLOCKWIDTH; j++,block++,loc++)
			*loc =  *block;
		loc += (mem->width - BLOCKWIDTH);
	}
}

/*************************************************************************
 *
 *	Name:		load_residual()
 *	Description:	load a residual-block (the difference of two blocks),
 *			and re-store into a block
 *	Input:		the pointers to a block and a MEM structure
 *			(where the other block is), boolean to indicate
 *			using filter
 *	Return:		none
 *	Side effects:	*block will be updated
 *	Date: 96/04/16	Author: Chu Ching-Wen in N.T.H.U., Taiwan
 *
 *************************************************************************/
void load_residual(MEM *mem, int16 *block, boolean with_filter)
{
	DEBUG("load_residual");
	int16 i, j;
	unsigned char *loc;
	int16 temp[64];
	int16 *ptr;

	loc = mem->data + mem->memloc + B_memloc[Current_B];
	if (with_filter) {
		/* Filter MC */
		loop_filter(loc, temp, mem->width);

		for (ptr=temp,i=0; i<BLOCKHEIGHT; i++)
			for (j=0; j<BLOCKWIDTH; j++,block++,ptr++)
				*block -= *ptr;
	} else {
		/* no Filter used */
		for (i=0; i<BLOCKHEIGHT; i++) {
			for (j=0; j<BLOCKWIDTH; j++,block++,loc++)
				*block -= *loc;
			loc += (mem->width - BLOCKWIDTH);
		}
	}
}

/*************************************************************************
 *
 *	Name:		save_residual()
 *	Description:	save a block onto the other in MEM structure
 *	Input:		the pointers to a block and a MEM structure
 *			(where the other block is), boolean to indicate
 *			using filter
 *	Return:		none
 *	Side effects:	one block in mem will be updated
 *	Date: 96/04/16	Author: Chu Ching-Wen in N.T.H.U., Taiwan
 *
 *************************************************************************/
void save_residual(MEM *mem, int16 *block, boolean with_filter)
{
	DEBUG("save_residual");
	int16 i, j;
	unsigned char *loc;
	int16 temp[64];
	int16 *ptr;

	loc = mem->data + mem->memloc + B_memloc[Current_B];
	if (with_filter) {
		/* Filter MC */
		loop_filter(loc, temp, mem->width);

		for (ptr=temp,i=0; i<BLOCKHEIGHT; i++)
			for (j=0; j<BLOCKWIDTH; j++,block++,ptr++)
				*block += *ptr;
	} else {
		/* no Filter used */
		for (i=0; i<BLOCKHEIGHT; i++) {
			for (j=0; j<BLOCKWIDTH; j++,block++,loc++)
				*block += *loc;
			loc += (mem->width - BLOCKWIDTH);
		}
	}
}

/*************************************************************************
 *
 *	Name:		loop_filter()
 *	Description:	obatin a block going thru the loop filter
 *	Input:		the pointers to a block in a MEM structure and
 *			the output block, and the width of MEM structure
 *	Return:		none
 *	Side effects:	the output block will be stored
 *	Date: 96/04/16	Author: Chu Ching-Wen in N.T.H.U., Taiwan
 *
 *************************************************************************/
static void loop_filter(unsigned char *memloc, int16 *output, int16 mem_width)
{
	DEBUG("loop_filter");
	int16 i, j;
	int16 temp[64];
	int16 *ptr, *ptr1, *ptr2, *ptr3;

	/* filter coeff: 1/4, 1/2, 1/4 	except at block edges */
	/*		 0, 1, 0 	for block edges */

	/* 1-D filter: handle with each row (horizontal line) */
	/* each value should multiply 4 for integer calculation */
	/* => filter coeff: 1, 2, 1 	except at block edges */
	/*		    0, 4, 0	for block edges */
	for (ptr=temp,i=0; i<BLOCKHEIGHT; i++) {
		/* at block edge */
		*(ptr++) = (*(memloc++) << 2);

		/* not at block edge */
		for (j=1; j<BLOCKWIDTH-1; j++,ptr++,memloc++)
			*ptr = *(memloc-1) + (*memloc<<1) + *(memloc+1);

		/* at block edge */
		*(ptr++) = (*(memloc++) << 2);
		memloc += (mem_width - BLOCKWIDTH);
	}

	/* 1-D filter: handle with each column (vertical line) */
	/* ptr1 is last row, ptr2 is curent row, ptr3 is next row */
	/* => filter coeff: 1/4, 1/2, 1/4 	except at block edges */
	/*		    0, 1, 0		for block edges */
	for (ptr=output,ptr1=ptr2=temp,ptr3=temp+(BLOCKWIDTH<<1),i=0;
			i<BLOCKHEIGHT; i++) {
		if ((i==0)||(i==7))
			/* at block edge */
			for (j=0; j<BLOCKWIDTH; j++)
				*(ptr++) = *(ptr2++);
		else
			/* not at block edge */
			for (j=0; j<BLOCKWIDTH; j++,ptr++,ptr1++,ptr2++,ptr3++)
				*ptr = ((*ptr1 + (*ptr2<<1) + *ptr3) >> 2);
	}

	/* (*output)/=4, causion with rounding ( >0.5 => 1) */
	for (ptr=output,i=0; i<BLOCKHEIGHT; i++)
		for (j=0; j<BLOCKWIDTH; j++,ptr++)
			*ptr = (*ptr>>2) + ( ((*ptr)&2) ? 1 : 0 );
}

