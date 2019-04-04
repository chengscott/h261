/*************************************************************************
 *
 *	Name:		huffman.c
 *	Description:	Huffman coding (variable-length-coding) and decoding
 *	Date: 96/04/16	Author: Chu Ching-Wen in N.T.H.U., Taiwan
 *
 *************************************************************************/

#include "globals.h"
#include "huffman.h"

#define MAX_STATE 200	/* maximum # of states for Decoder-Huffman-table */

/*************************************************************************/
/* public */
/* for encoder */
extern void init_VLC(void);
extern int16 put_VLC(int16 val, EHUFF *huff);
/* for decoder */
extern void init_VLD(void);
extern int16 get_VLC(DHUFF *huff);

/* for encoder */
EHUFF *MBA_Ehuff;
EHUFF *MVD_Ehuff;
EHUFF *CBP_Ehuff;
EHUFF *T1_Ehuff;
EHUFF *T2_Ehuff;
EHUFF *MTYPE_Ehuff;
/* for decoder */
DHUFF *MBA_Dhuff;
DHUFF *MVD_Dhuff;
DHUFF *CBP_Dhuff;
DHUFF *T1_Dhuff;
DHUFF *T2_Dhuff;
DHUFF *MTYPE_Dhuff;

/*************************************************************************/
/* private */
/* for encoder */
static EHUFF *make_EHUFF(int16 n, char *table_name);
static void load_EHUFF(EHUFF *table, int16 *array);
/* for decoder */
static DHUFF *make_DHUFF(char *table_name);
static void load_DHUFF(DHUFF *huff, int16 *array);
static void add_code(int16 value, int16 code_length, int16 code, DHUFF *huff);

#define EMPTY_STATE -1	/* ID for empty state */
#define LEAF_NODE -2	/* ID for leaf node */

/* for next_state[RIGHT or LEFT][] */
enum {RIGHT=0, LEFT=1};
#define LEAF(sval, huff) (huff->next_state[RIGHT][(sval)] == LEAF_NODE)
#define SET_LEAF(value, sval, huff) {\
			huff->next_state[RIGHT][(sval)] = LEAF_NODE;\
			huff->next_state[LEFT][(sval)] = value;\
			}
#define GET_LEAF(sval, huff) (huff->next_state[LEFT][(sval)])

/*************************************************************************
 *
 *	Name:		init_VLC()
 *	Description:	initialize encoder-huffman-table for VLC
 *	Input:          none
 *	Return:		none
 *	Side effects:   all ?_Ehuff will be maked and loaded
 *	Date: 96/04/16	Author: Chu Ching-Wen in N.T.H.U., Taiwan
 *
 *************************************************************************/
void init_VLC(void)
{
	DEBUG("init_VLC");

	/* for Eecoder-Huffman-table */
	MBA_Ehuff = make_EHUFF(40, "MBA");
	MVD_Ehuff = make_EHUFF(40, "MVD");
	CBP_Ehuff = make_EHUFF(70, "CBP");
	T1_Ehuff = make_EHUFF(8192, "TCOEFF1");
	T2_Ehuff = make_EHUFF(8192, "TCOEFF2");
	MTYPE_Ehuff = make_EHUFF(20, "MTYPE");

	load_EHUFF(MBA_Ehuff, MBAtable);
	load_EHUFF(MVD_Ehuff, MVDtable);
	load_EHUFF(CBP_Ehuff, CBPtable);
	load_EHUFF(T1_Ehuff, TCOEFFtable1);
	load_EHUFF(T2_Ehuff, TCOEFFtable2);
	load_EHUFF(MTYPE_Ehuff, MTYPEtable);
}

/*************************************************************************
 *
 *	Name:		put_VLC()
 *	Description:	put a variable-length-code according to a value and
 *			a encoder-huffman-table out to the bitstream
 *	Input:		the value, and the pointer to the huffman-table
 *	Return:		the number of bits written to the bitstream,
 *			a zero indicates error
 *	Side effects:   the write position of bitstream file will be updated
 *	Date: 96/04/16	Author: Chu Ching-Wen in N.T.H.U., Taiwan
 *
 *************************************************************************/
int16 put_VLC(int16 val, EHUFF *huff)
{
	DEBUG("put_VLC");

	#ifdef DEBUG_ON
	if (val<0) {	/* serious error, illegal, notify... */
		ERROR_LINE();
		printf("Out of bounds val in %s-EHUFF: %d<0\n",
			huff->table_name, val);
		exit(ERROR_BOUNDS);
	}
	#endif

	if ((val>=huff->n) || (huff->Hlen[val]==EMPTY_STATE)) {
		/* no serious error: can pass thru by alerting routine.*/
		return 0;
	}

	put_n_bits(huff->Hlen[val], (int32) huff->Hcode[val]);

	return huff->Hlen[val];
}

/*************************************************************************
 *
 *	Name:		init_VLD()
 *	Description:	initialize decoder-huffman-table for VLD
 *	Input:          none
 *	Return:		none
 *	Side effects:   all ?_Dhuff will be maked and loaded
 *	Date: 96/04/16	Author: Chu Ching-Wen in N.T.H.U., Taiwan
 *
 *************************************************************************/
void init_VLD(void)
{
	DEBUG("init_VLD");

	/* for Decoder-Huffman-table */
	MBA_Dhuff = make_DHUFF("MBA");
	MVD_Dhuff = make_DHUFF("MVD");
	CBP_Dhuff = make_DHUFF("CBP");
	T1_Dhuff = make_DHUFF("TCOEFF1");
	T2_Dhuff = make_DHUFF("TCOEFF2");
	MTYPE_Dhuff = make_DHUFF("MTYPE");

	load_DHUFF(MBA_Dhuff, MBAtable);
	load_DHUFF(MVD_Dhuff, MVDtable);
	load_DHUFF(CBP_Dhuff, CBPtable);
	load_DHUFF(T1_Dhuff, TCOEFFtable1);
	load_DHUFF(T2_Dhuff, TCOEFFtable2);
	load_DHUFF(MTYPE_Dhuff, MTYPEtable);
}

/*************************************************************************
 *
 *	Name:	       	get_VLC()
 *	Description:	get a value according to a variable-length-code
 *			(read from bitstream) and a decoder-huffman-table
 *	Input:		the pointer to the huffman-table
 *	Return:		the value of current VLC
 *	Side effects:   the read position of bitstream file will be updated
 *	Date: 96/04/16	Author: Chu Ching-Wen in N.T.H.U., Taiwan
 *
 *************************************************************************/
int16 get_VLC(DHUFF *huff)
{
	DEBUG("get_VLC");
	int16 next;
	int16 current_state = 0;

	next = huff->next_state[get_bit()][current_state];
	while (!LEAF(next, huff)) {
		/* consider 1 bit by 1 bit */
		/*            */
		/*   1 /\ 0   */
		/*    /  \    */
		/*   .    .   */

		#ifdef DEBUG_ON
		if (next==EMPTY_STATE) {
			ERROR_LINE();
			printf("Invalid state reached in %s-DHUFF\n",
				huff->table_name);
			exit(ERROR_HUFFMAN);
		}
		#endif

		/* try to next state */
		current_state = next;
		next = huff->next_state[get_bit()][current_state];
	}

	/* successful finding */
	return GET_LEAF(next, huff);
}

/*************************************************************************
 *
 *	Name:		make_EHUFF()
 *	Description:	make a encoder-huffman-table for VLC
 *	Input:          the size and name of the encoder-huffman-table
 *	Return:		the pointer to the constructed huffman-table
 *	Side effects:   Hlen and Hcode of ?_Ehuff will be set to EMPTY_STATE
 *	Date: 96/04/16	Author: Chu Ching-Wen in N.T.H.U., Taiwan
 *
 *************************************************************************/
static EHUFF *make_EHUFF(int16 n, char *table_name)
{
	DEBUG("make_EHUFF");
	EHUFF *temp;

	MAKE_STRUCTURE(temp, EHUFF);
	strcpy(temp->table_name, table_name);
	temp->n = n;
	temp->Hlen = (int16 *) malloc(n * sizeof(int16));
	temp->Hcode = (int16 *) malloc(n * sizeof(int16));
	if ((!temp->Hlen) || (!temp->Hcode)) {
		ERROR_LINE();
		printf("Cannot make a structure: %s-EHUFF\n", table_name);
		exit(ERROR_MEMORY);
	}

	/* initialize */
	for (--n; n>=0; n--)
		temp->Hlen[n] = temp->Hcode[n] = EMPTY_STATE;

	return temp;
}

/*************************************************************************
 *
 *	Name:		load_EHUFF()
 *	Description:	load an array into an encoder-huffman-table for VLC
 *			(indeed, the table is just an array)
 *	Input:         	the pointer to the encoder-huffman-table, and the
 *			loaded array
 *	Return:		none
 *	Side effects:   Hlen and Hcode of huff will be filled
 *	Date: 96/04/16	Author: Chu Ching-Wen in N.T.H.U., Taiwan
 *
 *************************************************************************/
static void load_EHUFF(EHUFF *huff, int16 *array)
{
	DEBUG("load_EHUFF");

	/* the first EOT signals the end of the table */
	while (*array!=EOT) {	/* not Eof-Of-Table */
		if (*array>huff->n) {
			ERROR_LINE();
			printf("%s-EHUFF overflow: %d>%d\n",
				huff->table_name, *array, huff->n);
			exit(ERROR_HUFFMAN);
		}
		huff->Hlen[*array] = array[1];
		huff->Hcode[*array] = array[2];
		array += 3;
	}
}

/*************************************************************************
 *
 *	Name:		make_DHUFF()
 *	Description:	make a decoder-huffman-table for VLD
 *	Input:          the name of the decoder-huffman-table
 *	Return:		the pointer to the constructed huffman-table
 *	Side effects:   all next_state of ?_Dhuff will be set to EMPTY_STATE
 *	Date: 96/04/16	Author: Chu Ching-Wen in N.T.H.U., Taiwan
 *
 *************************************************************************/
static DHUFF *make_DHUFF(char *table_name)
{
	DEBUG("make_DHUFF");
	int16 i;
	DHUFF *temp;

	/* get memory */
	MAKE_STRUCTURE(temp, DHUFF);
	strcpy(temp->table_name, table_name);
	temp->next_state[RIGHT] = (int16 *) malloc(MAX_STATE * sizeof(int16));
	temp->next_state[LEFT] = (int16 *) malloc(MAX_STATE * sizeof(int16));
	if ((!temp->next_state[RIGHT]) || (!temp->next_state[LEFT])) {
		ERROR_LINE();
		printf("Cannot make a structure: %s-DHUFF\n", table_name);
		exit(ERROR_MEMORY);
	}

	/* initialize */
	temp->number_of_states = 1;
	for (i=0; i<MAX_STATE; i++) {
		temp->next_state[RIGHT][i] =
		temp->next_state[LEFT][i] = EMPTY_STATE;
	}

	return temp;
}

/*************************************************************************
 *
 *	Name:		load_DHUFF()
 *	Description:	load an array into a decoder-huffman-table for VLD
 *			(indeed, the table is a binary tree)
 *	Input:         	the pointer to the encoder-huffman-table, and the
 *			loaded array
 *	Return:		none
 *	Side effects:   entries of huff will be filled
 *	Date: 96/04/16	Author: Chu Ching-Wen in N.T.H.U., Taiwan
 *
 *************************************************************************/
static void load_DHUFF(DHUFF *huff, int16 *array)
{
	DEBUG("load_DHUFF");

	/* The array consists of trios of Huffman definitions:
	 * the value, the code-length, and the variable-length-code */

	/* the first EOT signals the end of the table */
	while (*array!=EOT) {	/* not End-Of-Table */
		add_code(array[0], array[1], array[2], huff);
		array += 3;
	}
}

/*************************************************************************
 *
 *	Name:		add_code()
 *	Description:	add a huffman code into a decoder-huffman-table
 *			(indeed, the table is a binary tree)
 *	Input:         	the value, the code-length, the code, and
 *			the pointer to the decoder-huffman-table
 *	Return:		none
 *	Side effects:   entries of huff will be updated
 *	Date: 96/04/16	Author: Chu Ching-Wen in N.T.H.U., Taiwan
 *
 *************************************************************************/
static void add_code(int16 value, int16 code_length, int16 code, DHUFF *huff)
{
	DEBUG("add_code");
	int16 i, next;
	int16 go_left;
	int16 current_state = 0;

	if (value<0) {
		ERROR_LINE();
		printf("Negative addcode value in %s-DHUFF: %d\n",
			value, huff->table_name);
		exit(ERROR_HUFFMAN);
	}

	for (i=code_length-1; i>=0; i--) {
		/* consider i-th bit in code */
		/*            */
		/*   1 /\ 0   */
		/*    /  \    */
		/*   .    .   */

		go_left = ((code & (1<<i)) ? LEFT : RIGHT);
		next = huff->next_state[go_left][current_state];

		if (next==EMPTY_STATE) {	/* a new branch */
			/* add a new state */
			if ((next=huff->number_of_states) > MAX_STATE) {
				ERROR_LINE();
				printf("%s-DHUFF overflow: %d>%d\n",
					huff->table_name, next, MAX_STATE);
				exit(ERROR_HUFFMAN);
			}
			huff->number_of_states++;
			huff->next_state[go_left][current_state] = next;
		} else if (LEAF(next, huff)) {	/* a leaf node => wrong! */
			ERROR_LINE();
			printf("Non-unique prefix in %s-DHUFF\n",
				huff->table_name);
			printf("Length: %d   Code: %d|%x  Value: %d|%x\n",
				code_length, code, code, value, value);
			exit(ERROR_HUFFMAN);
		}

		current_state = next;
	}
	SET_LEAF(value, current_state, huff);
}

