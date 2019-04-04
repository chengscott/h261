/*************************************************************************
 *
 *	Name:		prototyp.h
 *	Description:	prototypes of all extern functions
 *	Date: 96/04/29	Author: Chu Ching-Wen in N.T.H.U., Taiwan
 *
 *************************************************************************/

#ifndef PROTOTYPE_DONE
#define PROTOTYPE_DONE

/*************************************************************************/
/* h261.c */
extern void main(int argc, char **argv);

/*************************************************************************/
/* me.c */
extern void motion_estimation(void);
extern int16 full_search_ME(MEM *preBLK, MEM *curBLK);
extern int16 three_step_search_ME(MEM *preBLK, MEM *curBLK);
extern int16 new_three_step_search_ME(MEM *preBLK, MEM *curBLK);

/*************************************************************************/
/* dct.c */
extern void DCT(short int *input, short int *output);
extern void IDCT(short int *input, short int *output);

/*************************************************************************/
/* huffman.c */
/* for encoder */
extern void init_VLC(void);
extern int16 put_VLC(int16 val, EHUFF *huff);
/* for decoder */
extern void init_VLD(void);
extern int16 get_VLC(DHUFF *huff);

/*************************************************************************/
/* codec.c */
/* for encoder */
extern boolean quantize(boolean intra_used, int16 *block, int16 quantizer);
extern void transfer_TCOEFF(boolean intra_used, int16 *block);
/* for decoder */
extern void Iquantize(boolean intra_used, int16 *block, int16 quantizer);
extern void Itransfer_TCOEFF(boolean intra_used, int16 *block);
extern void clip_reconstructed_block(int16 *block);

/*************************************************************************/
/* header.c */
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
/* memory.c */
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
extern void read_MB(int16 MBbuf[6][64], FSTORE *Fs);
extern void write_block(MEM *mem, int16 *block);
extern void load_residual(MEM *mem, int16 *block, boolean with_filter);
extern void save_residual(MEM *mem, int16 *block, boolean with_filter);

/*************************************************************************/
/* io.c */
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

/*************************************************************************/
/* stat.c */
extern void statistics(FSTORE *ref_fs, FSTORE *fs);
extern void print_time(void);
extern void print_codec_info(boolean decoder);
extern void print_frame_info(int32 Current_frame);
extern void print_sequence_info(boolean decoder);
extern double get_time_cost(void);

/*************************************************************************/
#ifdef X11
/* display.c */
extern boolean init_display(char *name);
extern void init_dither(void);
extern void dither(FSTORE *fs);
#endif

#endif
