/*************************************************************************
 *
 *	Name:		io.c
 *	Description:	basic input/output operations: read/write frame
 *			and bitstream
 *	Date: 96/04/25	Author: Chu Ching-Wen in N.T.H.U., Taiwan
 *
 *************************************************************************/

#include "globals.h"
#include "ctrl.h"

#define MAX_BUFFER_SIZE 65530	/* maximum buffer size for read or write */

/* definitions for fseek */
#ifndef SEEK_SET
#define SEEK_SET 0	/* seeks from beginning of file */
#define SEEK_CUR 1	/* seeks from current position */
#define SEEK_END 2	/* seeks from end of file */
#endif

/*************************************************************************/
/* public */
/* for image-files (Y, Cb and Cr) IO */
/* 	for encoder */
extern boolean read_and_show_frame(int32 frame_ID, FSTORE *fs);
/* 	for decoder */
extern boolean write_or_show_frame(int32 end_frame_ID, FSTORE *fs);
/* for bit-stream IO */
/* 	for encoder (write_stream) */
extern void open_write_stream(char *filename);
extern void close_write_stream(void);
extern void put_bit(int16 bit);
extern void put_n_bits(int16 n, int32 word);
extern int32 ftell_write_stream(void);
/* 	for decoder (read_stream) */
extern void open_read_stream(char *filename);
extern void close_read_stream(void);
extern int16 get_bit(void);
extern int32 get_n_bits(int16 n);
extern int32 ftell_read_stream(void);
extern boolean eof_read_stream(void);

extern IMAGE *Image;		/* global info. of image */

/*************************************************************************/
/* private */
/* for bit-stream IO */
/* ?_buffer_ptr pointers the start-position in the ?_stream */
/* ?_buffer_end pointers the end-position in the ?_stream */
static byte *write_buffer, *write_buffer_ptr, *write_buffer_end;
static byte *read_buffer, *read_buffer_ptr, *read_buffer_end;
static int32 write_buffer_size;
static int32 read_buffer_size;

static FILE *write_stream;
static FILE *read_stream;
static int16 write_position;	/* 0, ..., 7 */
static int16 read_position;	/* 0, ..., 7 */

/* for bit operations */
/* 0...0001, 0...0010, 0...0100, 0...1000, ... */
static bytes4 bit_set_mask[] = {
	0x00000001,0x00000002,0x00000004,0x00000008,
	0x00000010,0x00000020,0x00000040,0x00000080,
	0x00000100,0x00000200,0x00000400,0x00000800,
	0x00001000,0x00002000,0x00004000,0x00008000,
	0x00010000,0x00020000,0x00040000,0x00080000,
	0x00100000,0x00200000,0x00400000,0x00800000,
	0x01000000,0x02000000,0x04000000,0x08000000,
	0x10000000,0x20000000,0x40000000,0x80000000
};

/* 0...0001, 0...0011, 0...0111, 0...1111, ... */
static bytes4 bits_enable_mask[] = {
	0x00000001,0x00000003,0x00000007,0x0000000F,
	0x0000001F,0x0000003F,0x0000007F,0x000000FF,
	0x000001FF,0x000003FF,0x000007FF,0x00000FFF,
	0x00001FFF,0x00003FFF,0x00007FFF,0x0000FFFF,
	0x0001FFFF,0x0003FFFF,0x0007FFFF,0x000FFFFF,
	0x001FFFFF,0x003FFFFF,0x007FFFFF,0x00FFFFFF,
	0x01FFFFFF,0x03FFFFFF,0x07FFFFFF,0x0FFFFFFF,
	0x1FFFFFFF,0x3FFFFFFF,0x7FFFFFFF,0xFFFFFFFF,
};

/*************************************************************************
 *
 *	Name:		read_and_show_frame()
 *	Description:	read and show the original frame
 *	Input:          current frame ID and pointer to frame store
 *	Return:		TRUE for successful reading,
 *			FALSE if no such files or no capture supported
 *	Side effects:
 *	Date: 96/04/24	Author: Chu Ching-Wen in N.T.H.U., Taiwan
 *
 *************************************************************************/
boolean read_and_show_frame(int32 frame_ID, FSTORE *fs)
{
	DEBUG("read_and_show_frame");
	int32 file_len;
	char filename[MAX_FILENAME_LEN];
	FILE *inp;
	ComponentType type;

	if (Image->read_from_files) {
		/* read from 3 files */
		for (type=0; type<NUMBER_OF_COMPONENTS; type++) {
			/* make input frame filenames */
			sprintf(filename, "%s%ld%s",
				Image->input_frame_prefix, frame_ID,
				Image->frame_suffix[type]);

			/* open the type-th file */
			if ((inp = fopen(filename, "rb")) == NULL) {
				/* End_frame should be reset to frame_ID */
				return FALSE;
			}

			/* check the file length */
			fseek(inp, 0, SEEK_END);
			file_len = ftell(inp);
			rewind(inp);
			if (Image->len[type] != file_len) {
				ERROR_LINE();
				printf("File size is not corret: %s.\n",
					filename);
				exit(ERROR_IO);
			}

			fread((void *) fs->fs[type]->data,
				sizeof(byte), Image->len[type], inp);
			fclose(inp);
		}
	} else {
		/* read from capture */
		/* No capture exists!
		 * <input_frame_prefix> should be specified
		 */
		return FALSE;
	}

	return TRUE;	/* successful read */
}

/*************************************************************************
 *
 *	Name:		write_or_show_frame()
 *	Description:	write or show the designated frame
 *	Input:          the last written frame ID and pointer to frame store
 *	Return:		TRUE for successful writing,
 *			FALSE if no supported display
 *	Side effects:   Start_frame_ID will be increased to end_frame_ID
 *	Date: 96/04/16	Author: Chu Ching-Wen in N.T.H.U., Taiwan
 *
 *************************************************************************/
boolean write_or_show_frame(int32 end_frame_ID, FSTORE *fs)
{
	DEBUG("write_or_show_frame");
	extern int32 Start_frame;
	char filename[MAX_FILENAME_LEN];
	FILE *out;
	ComponentType type;
	static int32 start_frame_ID = -1;

	if (Image->write_to_files) {
		/* set start_frame_ID while first time calling the procedure */
		if (start_frame_ID<0) start_frame_ID = Start_frame;

		while (start_frame_ID<=end_frame_ID) {
			/* write to 3 files */
			for (type=0; type<NUMBER_OF_COMPONENTS; type++) {
				/* make output frame filenames */
				sprintf(filename, "%s%ld%s",
					Image->output_frame_prefix, start_frame_ID,
					Image->frame_suffix[type]);

				/* open the type-th file */
				if ((out = fopen(filename, "wb")) == NULL) {
					ERROR_LINE();
					printf("Cannot open filename %s.\n",
						filename);
					exit(ERROR_IO);
				}

				fwrite((void *) fs->fs[type]->data,
					sizeof(byte), Image->len[type], out);
				fclose(out);
			}
			start_frame_ID++;
		}
		#ifdef X11
		if (Image->display) dither(fs);
		#endif
	} else {
		/* put frames on display */
		#ifdef X11
		if (Image->display) dither(fs);
		#else
		/* Do not suport this display!
		 * <output_frame_prefix> should be specified
		 */
		return FALSE;
		#endif
	}

	return TRUE;
}

/*************************************************************************
 *
 *	Name:		open_write_stream()
 *	Description:	open bitstream file for write
 *	Input:          bitstream filename
 *	Return:		none
 *	Side effects:	allocate memory for write_buffer as large as possible
 *	Date: 96/04/25	Author: Chu Ching-Wen in N.T.H.U., Tainwan
 *
 *************************************************************************/
void open_write_stream(char *filename)
{
	DEBUG("open_write_stream");
	extern int32 Number_frame;
	int32 write_buffer_size_in_byte;

	if ((write_stream = fopen(filename, "w+b")) == NULL) {
		ERROR_LINE();
		printf("Cannot open write_stream file %s\n", filename);
		exit(ERROR_EOF);
	}

	/* allocate write_buffer to fasten file I/O */
	/* suppose under 5 frames/sec and 128k bits/sec: */
	write_buffer_size_in_byte = ((int32) (Number_frame * (128000/8) / 5)
					* sizeof(byte));
	/* maximum buffer size is MAX_BUFFER_SIZE bytes */
	if (write_buffer_size_in_byte>MAX_BUFFER_SIZE)
		write_buffer_size_in_byte = MAX_BUFFER_SIZE;

	write_buffer_size = write_buffer_size_in_byte;
	while (!(write_buffer = (byte *) malloc(write_buffer_size))) {
		write_buffer_size >>= 1;	/* helf size */
		if (write_buffer_size<=0) {
			/* no allocated buffer, we can only use file I/O */
			ERROR_LINE();
			printf("Cannot allocate write_buffer.\n");
			break;
		}
	}

	#ifdef DEBUG_ON
	printf("write_buffer_size = %ld\n", write_buffer_size);
	#endif

	/* initialize write_buffer */
	write_buffer_ptr = write_buffer;
	*write_buffer_ptr = 0;
	write_buffer_end = (byte *) ((int32) write_buffer
			+ write_buffer_size);
	write_position = 7;
}

/*************************************************************************
 *
 *	Name:		close_write_stream()
 *	Description:	close bitstream file for write
 *	Input:		none
 *	Return:		none
 *	Side effects:	free memory for write_buffer
 *	Date: 96/04/25	Author: Chu Ching-Wen in N.T.H.U., Tainwan
 *
 *************************************************************************/
void close_write_stream(void)
{
	DEBUG("close_write_stream");

	/* fill '0' at the tail of one byte */
	if (write_position != 7)
		write_buffer_ptr = (byte *) ((int32) write_buffer_ptr + 1);

	/* clear write_buffer */
	fwrite((void *) write_buffer, sizeof(byte),
		(int32)write_buffer_ptr - (int32)write_buffer, write_stream);

	free(write_buffer);
	fclose(write_stream);
}

#define NEXT_WORD(type) {\
	write_buffer_ptr = (type *) ((int32) write_buffer_ptr + sizeof(type));\
	if (write_buffer_ptr==write_buffer_end) {\
		/* write_buffer is full */\
		/* write out data from write_buffer */\
		fwrite((void *) write_buffer, sizeof(type),\
				write_buffer_size, write_stream);\
		write_buffer_ptr = write_buffer;\
	}\
	*write_buffer_ptr = 0;\
	}

/*************************************************************************
 *
 *	Name:		put_bit()
 *	Description:	put 1 bit to bitstream file for write
 *	Input:		the bit to be put (bit==0: '0', else: '1')
 *	Return:         none
 *	Side effects:	write_position and write_buffer_ptr will be updated
 *	Date: 96/04/25	Author: Chu Ching-Wen in N.T.H.U., Tainwan
 *
 *************************************************************************/
void put_bit(int16 bit)
{
	DEBUG("put_bit");

	if (bit) {
		/* set (write_position in write_buffer = '1' */
		(*write_buffer_ptr) |= bit_set_mask[write_position--];
	} else {
		/* set (write_position in write_buffer = '0' => do nothing */
		write_position--;
	}

	if (write_position<0) {
		/* *write_buffer_ptr is full (8-bits), change to next byte */
		NEXT_WORD(byte);
		write_position = 7;
	}
}

/*************************************************************************
 *
 *	Name:		put_n_bits()
 *	Description:	put n bits (n<=32) to bitstream file for write
 *	Input:		number of bits and bits to be put
 *	Return:         none
 *	Side effects:	write_position and write_buffer_ptr will be updated
 *	Date: 96/04/24	Author: Chu Ching-Wen in N.T.H.U., Tainwan
 *
 *************************************************************************/
void put_n_bits(int16 n, int32 word)
{
	DEBUG("put_n_bits");

	while (n--) {
	if (((bytes4)word&bit_set_mask[n])) {
		/* set (write_position in write_buffer = '1' */
		(*write_buffer_ptr) |= bit_set_mask[write_position--];
	} else {
		/* set (write_position in write_buffer = '0' => do nothing */
		write_position--;
	}

	if (write_position<0) {
		/* *write_buffer_ptr is full (8-bits), change to next byte */
		NEXT_WORD(byte);
		write_position = 7;
	}
}
}

/*************************************************************************
 *
 *	Name:		ftell_write_stream()
 *	Description:	return the position in bits of the write stream
 *	Input:		none
 *	Return:		the position in bits of the write stream
 *	Side effects:
 *	Date: 96/04/24	Author: Chu Ching-Wen in N.T.H.U., Tainwan
 *
 *************************************************************************/
int32 ftell_write_stream(void)
{
	DEBUG("ftell_write_stream");

	return (((ftell(write_stream)
		+ ((int32) write_buffer_ptr - (int32) write_buffer))<<3)
		+ (7 - write_position));
}

/*************************************************************************
 *
 *	Name:		open_read_stream()
 *	Description:	open bitstream file for read
 *	Input:          bitstream filename
 *	Return:		none
 *	Side effects:	allocate memory for write_buffer as large as possible
 *	Date: 96/04/24	Author: Chu Ching-Wen in N.T.H.U., Tainwan
 *
 *************************************************************************/
void open_read_stream(char *filename)
{
	DEBUG("open_read_stream");

	if ((read_stream = fopen(filename, "rb")) == NULL) {
		printf("Cannot open read_stream file %s\n", filename);
		exit(ERROR_EOF);
	}

	/* allocate read_buffer */
	fseek(read_stream, 0, SEEK_END);		/* goto EOF */
	read_buffer_size = ftell(read_stream);	/* get file size */
	rewind(read_stream);
	if (read_buffer_size<=0) {
		ERROR_LINE();
		printf("read_stream file %s is empty.\n", filename);
		exit(ERROR_EOF);
	}

	/* maximum buffer size is MAX_BUFFER_SIZE bytes */
	if (read_buffer_size>MAX_BUFFER_SIZE)
		read_buffer_size = MAX_BUFFER_SIZE;

	while (!(read_buffer=(byte *) malloc(read_buffer_size))) {
		read_buffer_size >>= 1;		/* helf size */
		if (read_buffer_size<=0) {
			ERROR_LINE();
			printf("Cannot allocate read_buffer.\n");
			exit(ERROR_MEMORY);
		}
	}
	#ifdef CTRL_STAT
	printf("read_buffer_size = %d \n", read_buffer_size);
	#endif

	/* initialize read_buffer */
	read_buffer_ptr = read_buffer_end - 1;
	read_position = -1;
}

/*************************************************************************
 *
 *	Name:		close_read_stream()
 *	Description:	close bitstream file for read
 *	Input:		none
 *	Return:		none
 *	Side effects:	free memory for write_buffer
 *	Date: 96/04/16	Author: Chu Ching-Wen in N.T.H.U., Tainwan
 *
 *************************************************************************/
void close_read_stream(void)
{
	DEBUG("close_read_stream");

	fclose(read_stream);
	free(read_buffer);
}

/*************************************************************************
 *
 *	Name:		get_bit()
 *	Description:	get 1 bit from bitstream file for read
 *	Input:		none
 *	Return:         the bit ('1' or '0') in one byte
 *	Side effects:	read_position and read_buffer_ptr will be updated
 *	Date: 96/04/24	Author: Chu Ching-Wen in N.T.H.U., Tainwan
 *
 *************************************************************************/
int16 get_bit(void)
{
	DEBUG("get_bit");

	if (read_position < 0) {
		if ((++read_buffer_ptr)==read_buffer_end) {
			/* out of data in read_buffer */
			/* fill data into read_buffer */
			read_buffer_size = fread((void *) read_buffer,
				sizeof(byte), read_buffer_size, read_stream);
			if (read_buffer_size==0) {
				ERROR_LINE();
				printf("EOF at wrong place in read_stream!\n");
				exit(ERROR_EOF);
			}

			read_buffer_ptr = read_buffer;
			read_buffer_end = read_buffer + read_buffer_size;
		}
		read_position = 7;
	}

	return ((*read_buffer_ptr) & ((int16) bit_set_mask[read_position--])
		? 1 : 0);
}

/*************************************************************************
 *
 *	Name:		get_n_bits()
 *	Description:	get n bits from bitstream file for read
 *	Input:		none
 *	Return:         the n bits (1/0 bitstream) in one word
 *	Side effects:	read_position and read_buffer_ptr will be updated
 *	Date: 96/04/16	Author: Chu Ching-Wen in N.T.H.U., Tainwan
 *
 *************************************************************************/
int32 get_n_bits(int16 n)
{
	DEBUG("get_n_bits");
	int32 word = 0;

	while (n--) {
		word <<= 1;
		if (get_bit())	word |= 0x1;
	}
	return word;
}

/*************************************************************************
 *
 *	Name:		ftell_read_stream()
 *	Description:	return the position in bits of the read stream
 *	Input:		none
 *	Return:		the position in bits of the read stream
 *	Side effects:
 *	Date: 96/04/16	Author: Chu Ching-Wen in N.T.H.U., Tainwan
 *
 *************************************************************************/
int32 ftell_read_stream(void)
{
	DEBUG("ftell_read_stream");

	return ((( ftell(read_stream) -
		  (read_buffer_end-read_buffer_ptr) ) << 3)
		+ (7 - read_position));
}

/*************************************************************************
 *
 *	Name:		eof_read_stream()
 *	Description:	check if the read stream is at EOF or not
 *	Input:		none
 *	Return:		TRUE to indicate the read stream is at EOF,
 *			FALSE to indicate not at end-of-file
 *	Side effects:
 *	Date: 96/04/24	Author: Chu Ching-Wen in N.T.H.U., Tainwan
 *
 *************************************************************************/
boolean eof_read_stream(void)
{
	DEBUG("eof_read_stream");
	boolean eof;
	byte s[1];

	/* if read_buffer not empty => not EOF */
	if (read_buffer_ptr+1<read_buffer_end) return FALSE;

	eof = ((fread((void *)s, 1, 1, read_stream)==0) ? TRUE : FALSE);
	if (!eof) fseek(read_stream, -1, SEEK_CUR);

	return eof;
}

