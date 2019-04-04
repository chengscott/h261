/* Chen's DCT for 8x8 block:
   W. H. Chen, C. H. Smith, and S. C. Fralick,
   "A fast computational algorithm for the disctrte cosine transform",
   IEEE Trans. Comm., vol COM-25, no. 9, Sep. 1977, pp. 1004-1009.
*/

/* Define shift operations */
#define LS(r,s) ((r) << (s))
#define RS(r,s) ((r) >> (s))       /* Caution with rounding... */
#define MSCALE(expr)  RS((expr),9)

/* Cos constants */
#define c1d4 362L       /* cos(PI / 4) */

#define c1d8 473L       /* cos(PI / 8) */
#define c3d8 196L       /* cos(3 PI / 8) */

#define c1d16 502L      /* cos(PI / 16) */
#define c3d16 426L      /* cos(3 PI / 16) */
#define c5d16 284L      /* cos(5 PI / 16) */
#define c7d16 100L      /* cos(7 PI / 16) */

/* Obtain 1D DCT by Chen's algorithm */
/* 16(*) 26(+) */
void ChenDct_1D(short int *x, short int *y)
{
	register short int a0,a1,a2,a3;
	register short int b0,b1,b2,b3;
	register short int c0,c1,c2,c3;
	register short int *ptr1, *ptr2, *yptr;

	ptr1 = x;
	ptr2 = x + 7;
	c3 = RS(*ptr1 - *ptr2, 1);
	a0 = RS(*ptr1 + *ptr2, 1);
	c2 = RS(*(++ptr1) - *(--ptr2), 1);
	a1 = RS(*ptr1 + *ptr2, 1);
	c1 = RS(*(++ptr1) - *(--ptr2), 1);
	a2 = RS(*ptr1 + *ptr2, 1);
	c0 = RS(*(++ptr1) - *(--ptr2), 1);
	a3 = RS(*ptr1 + *ptr2, 1);

	b0 = a0 + a3;
	b1 = a1 + a2;
	b2 = a1 - a2;
	b3 = a0 - a3;

	*(yptr=y) = MSCALE(c1d4 * (b0+b1));
	*(yptr+4) = MSCALE(c1d4 * (b0-b1));
	*(yptr+2) = MSCALE((c3d8*b2) + (c1d8*b3));
	*(yptr+6) = MSCALE((c3d8*b3) - (c1d8*b2));

	b0 = MSCALE(c1d4 * (c2-c1));
	b1 = MSCALE(c1d4 * (c2+c1));

	a0 = c0 + b0;
	a1 = c0 - b0;
	a2 = c3 - b1;
	a3 = c3 + b1;

	*(yptr+1) = MSCALE((c7d16*a0) + (c1d16*a3));
	*(yptr+3) = MSCALE((c3d16*a2) - (c5d16*a1));
	*(yptr+5) = MSCALE((c3d16*a1) + (c5d16*a2));
	*(yptr+7) = MSCALE((c7d16*a3) - (c1d16*a0));
}

/* Obtain 1D IDCT by Chen's algorithm */
/* 16(*) 26(+) */
void ChenIDct_1D(short int *x,short int *y)
{
	register short int a0,a1,a2,a3;
	register short int b0,b1,b2,b3;
	register short int c0,c1,c2,c3;
	register short int *ptr;


	/* Even terms:  b0 = x0  b1 = x4  b2 = x2  b3 = x6 */
	/* Odd terms:   a0 = x1  a1 = x3  a2 = x5  a3 = x7 */
	ptr = x;
	b0 = *(ptr=x);
	a0 = *(++ptr);
	b2 = *(++ptr);
	a1 = *(++ptr);
	b1 = *(++ptr);
	a2 = *(++ptr);
	b3 = *(++ptr);
	a3 = *(++ptr);

	c0 = MSCALE((c7d16*a0) - (c1d16*a3));
	c1 = MSCALE((c3d16*a2) - (c5d16*a1));
	c2 = MSCALE((c3d16*a1) + (c5d16*a2));
	c3 = MSCALE((c1d16*a0) + (c7d16*a3));

	/* First Butterfly on even terms */
	a0 = MSCALE(c1d4 * (b0+b1));
	a1 = MSCALE(c1d4 * (b0-b1));

	a2 = MSCALE((c3d8*b2) - (c1d8*b3));
	a3 = MSCALE((c1d8*b2) + (c3d8*b3));

	/* Calculate last set of b's */
	b0 = a0 + a3;
	b1 = a1 + a2;
	b2 = a1 - a2;
	b3 = a0 - a3;

	/* Second Butterfly */
	a0 = c0 + c1;
	a1 = c0 - c1;
	a2 = c3 - c2;
	a3 = c3 + c2;

	c0 = a0;
	c1 = MSCALE(c1d4 * (a2-a1));
	c2 = MSCALE(c1d4 * (a2+a1));
	c3 = a3;

	*(ptr=y) = RS(b0 + c3, 1);
	*(++ptr) = RS(b1 + c2, 1);
	*(++ptr) = RS(b2 + c1, 1);
	*(++ptr) = RS(b3 + c0, 1);
	*(++ptr) = RS(b3 - c0, 1);
	*(++ptr) = RS(b2 - c1, 1);
	*(++ptr) = RS(b1 - c2, 1);
	*(++ptr) = RS(b0 - c3, 1);
}

/* row-column based using Chen's DCT */
void DCT(short int *x, short int *y)
{
	register short int i;
	register short int *aptr,*bptr;
	register short int a0,a1,a2,a3;
	register short int b0,b1,b2,b3;
	register short int c0,c1,c2,c3;

	/* Loop over columns */
	for (i=0; i<8; i++) {
		aptr = x+i;
		bptr = aptr+56;

		a0 = LS((*aptr+*bptr),2);
		c3 = LS((*aptr-*bptr),2);
		aptr += 8;
		bptr -= 8;
		a1 = LS((*aptr+*bptr),2);
		c2 = LS((*aptr-*bptr),2);
		aptr += 8;
		bptr -= 8;
		a2 = LS((*aptr+*bptr),2);
		c1 = LS((*aptr-*bptr),2);
		aptr += 8;
		bptr -= 8;
		a3 = LS((*aptr+*bptr),2);
		c0 = LS((*aptr-*bptr),2);

                b0 = a0+a3;
                b1 = a1+a2;
                b2 = a1-a2;
		b3 = a0-a3;

                aptr = y+i;

                *aptr = MSCALE(c1d4*(b0+b1));
		aptr[32] = MSCALE(c1d4*(b0-b1));

		aptr[16] = MSCALE((c3d8*b2)+(c1d8*b3));
		aptr[48] = MSCALE((c3d8*b3)-(c1d8*b2));

		b0 = MSCALE(c1d4*(c2-c1));
                b1 = MSCALE(c1d4*(c2+c1));

                a0 = c0+b0;
		a1 = c0-b0;
		a2 = c3-b1;
                a3 = c3+b1;

                aptr[8] = MSCALE((c7d16*a0)+(c1d16*a3));
                aptr[24] = MSCALE((c3d16*a2)-(c5d16*a1));
                aptr[40] = MSCALE((c3d16*a1)+(c5d16*a2));
                aptr[56] = MSCALE((c7d16*a3)-(c1d16*a0));
        }

        /* Loop over rows */
	for (i=0; i<8; i++) {
		aptr = y+LS(i,3);
		bptr = aptr+7;

                c3 = RS((*(aptr)-*(bptr)),1);
                a0 = RS((*(aptr++)+*(bptr--)),1);
                c2 = RS((*(aptr)-*(bptr)),1);
                a1 = RS((*(aptr++)+*(bptr--)),1);
		c1 = RS((*(aptr)-*(bptr)),1);
                a2 = RS((*(aptr++)+*(bptr--)),1);
                c0 = RS((*(aptr)-*(bptr)),1);
		a3 = RS((*(aptr)+*(bptr)),1);

                b0 = a0+a3;
                b1 = a1+a2;
		b2 = a1-a2;
		b3 = a0-a3;

		aptr = y+LS(i,3);

                *aptr = MSCALE(c1d4*(b0+b1));
		aptr[4] = MSCALE(c1d4*(b0-b1));
                aptr[2] = MSCALE((c3d8*b2)+(c1d8*b3));
		aptr[6] = MSCALE((c3d8*b3)-(c1d8*b2));

                b0 = MSCALE(c1d4*(c2-c1));
                b1 = MSCALE(c1d4*(c2+c1));

                a0 = c0+b0;
                a1 = c0-b0;
                a2 = c3-b1;
                a3 = c3+b1;

		aptr[1] = MSCALE((c7d16*a0)+(c1d16*a3));
		aptr[3] = MSCALE((c3d16*a2)-(c5d16*a1));
		aptr[5] = MSCALE((c3d16*a1)+(c5d16*a2));
		aptr[7] = MSCALE((c7d16*a3)-(c1d16*a0));
	}

	/* We have an additional factor of 8 in the Chen algorithm. */
	for (i=0, aptr=y; i<64; i++, aptr++)
		*aptr = (((*aptr<0) ? (*aptr-4) : (*aptr+4))/8);
}

/* row-column based using Chen's IDCT */
void IDCT(short int *x, short int *y)
{
	register short int i;
	register short int *aptr;
        register short int a0,a1,a2,a3;
        register short int b0,b1,b2,b3;
        register short int c0,c1,c2,c3;

        /* Loop over columns */
        for (i=0; i<8; i++) {
                aptr = x+i;
		b0 = LS(*aptr,2);
		aptr += 8;
                a0 = LS(*aptr,2);
                aptr += 8;
		b2 = LS(*aptr,2);
                aptr += 8;
                a1 = LS(*aptr,2);
                aptr += 8;
                b1 = LS(*aptr,2);
                aptr += 8;
                a2 = LS(*aptr,2);
                aptr += 8;
                b3 = LS(*aptr,2);
                aptr += 8;
                a3 = LS(*aptr,2);

                /* Split into even mode  b0 = x0  b1 = x4  b2 = x2  b3 = x6.
                   And the odd terms a0 = x1 a1 = x3 a2 = x5 a3 = x7. */
		c0 = MSCALE((c7d16*a0)-(c1d16*a3));
                c1 = MSCALE((c3d16*a2)-(c5d16*a1));
                c2 = MSCALE((c3d16*a1)+(c5d16*a2));
		c3 = MSCALE((c1d16*a0)+(c7d16*a3));

		/* First Butterfly on even terms.*/
                a0 = MSCALE(c1d4*(b0+b1));
		a1 = MSCALE(c1d4*(b0-b1));

                a2 = MSCALE((c3d8*b2)-(c1d8*b3));
                a3 = MSCALE((c1d8*b2)+(c3d8*b3));

                b0 = a0+a3;
                b1 = a1+a2;
                b2 = a1-a2;
		b3 = a0-a3;

                /* Second Butterfly */
                a0 = c0+c1;
                a1 = c0-c1;
                a2 = c3-c2;
                a3 = c3+c2;

                c0 = a0;
		c1 = MSCALE(c1d4*(a2-a1));
                c2 = MSCALE(c1d4*(a2+a1));
                c3 = a3;

                aptr = y+i;
                *aptr = b0+c3;
                aptr += 8;
                *aptr = b1+c2;
                aptr += 8;
                *aptr = b2+c1;
                aptr += 8;
                *aptr = b3+c0;
		aptr += 8;
                *aptr = b3-c0;
		aptr += 8;
                *aptr = b2-c1;
		aptr += 8;
                *aptr = b1-c2;
                aptr += 8;
		*aptr = b0-c3;
        }

        /* Loop over rows */
        for (i=0; i<8; i++) {
		aptr = y+LS(i,3);
                b0 = *(aptr++);
                a0 = *(aptr++);
                b2 = *(aptr++);
		a1 = *(aptr++);
                b1 = *(aptr++);
                a2 = *(aptr++);
                b3 = *(aptr++);
                a3 = *(aptr);

                /* Split into even mode  b0 = x0  b1 = x4  b2 = x2  b3 = x6.
                   And the odd terms a0 = x1 a1 = x3 a2 = x5 a3 = x7. */
		c0 = MSCALE((c7d16*a0)-(c1d16*a3));
                c1 = MSCALE((c3d16*a2)-(c5d16*a1));
		c2 = MSCALE((c3d16*a1)+(c5d16*a2));
                c3 = MSCALE((c1d16*a0)+(c7d16*a3));

		/* First Butterfly on even terms.*/
                a0 = MSCALE(c1d4*(b0+b1));
                a1 = MSCALE(c1d4*(b0-b1));

		a2 = MSCALE((c3d8*b2)-(c1d8*b3));
                a3 = MSCALE((c1d8*b2)+(c3d8*b3));

		/* Calculate last set of b's */
		b0 = a0+a3;
                b1 = a1+a2;
                b2 = a1-a2;
                b3 = a0-a3;

                /* Second Butterfly */
                a0 = c0+c1;
                a1 = c0-c1;
		a2 = c3-c2;
                a3 = c3+c2;

                c0 = a0;
                c1 = MSCALE(c1d4*(a2-a1));
                c2 = MSCALE(c1d4*(a2+a1));
                c3 = a3;

                aptr = y+LS(i,3);
                *(aptr++) = b0+c3;
                *(aptr++) = b1+c2;
                *(aptr++) = b2+c1;
		*(aptr++) = b3+c0;
                *(aptr++) = b3-c0;
                *(aptr++) = b2-c1;
                *(aptr++) = b1-c2;
                *(aptr) = b0-c3;
        }

	/* Retrieve correct accuracy. We have additional factor
           of 16 that must be removed. */
	for (i=0, aptr=y; i<64; i++, aptr++)
                *aptr = (((*aptr<0) ? (*aptr-8) : (*aptr+8)) /16);
}

