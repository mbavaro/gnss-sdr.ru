/************************************************************************************************
Copyright 2008 Gregory W Heckler

This file is part of the GPS Software Defined Radio (GPS-SDR)

The GPS-SDR is free software; you can redistribute it and/or modify it under the terms of the
GNU General Public License as published by the Free Software Foundation; either version 2 of the
License, or (at your option) any later version.

The GPS-SDR is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without
even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License along with GPS-SDR; if not,
write to the:

Free Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
************************************************************************************************/


#include "includes.h"


/*----------------------------------------------------------------------------------------------*/
/*!
 * code_gen, generate the given prn code
 * */
int32 code_gen(CPX *_dest, int32 _prn)
{

	uint32 G1[1023];
	uint32 G2[1023];
	uint32 G1_register[10], G2_register[10];
	uint32 feedback1, feedback2;
	uint32 lcv, lcv2;
	uint32 delay;

	/* G2 Delays as defined in GPS-ISD-200D */
	int32 delays[51] = {5, 6, 7, 8, 17, 18, 139, 140, 141, 251, 252, 254 ,255, 256, 257, 258, 469, 470, 471, 472,
		473, 474, 509, 512, 513, 514, 515, 516, 859, 860, 861, 862, 145, 175, 52, 21, 237, 235, 886, 657, 634, 762,
		355, 1012, 176, 603, 130, 359, 595, 68, 386};

	/* A simple error check */
	if((_prn < 0) || (_prn > 51))
		return(0);

	for(lcv = 0; lcv < 10; lcv++)
	{
		G1_register[lcv] = 1;
		G2_register[lcv] = 1;
	}

	/* Generate G1 & G2 Register */
	for(lcv = 0; lcv < 1023; lcv++)
	{
		G1[lcv] = G1_register[0];
		G2[lcv] = G2_register[0];

		feedback1 = G1_register[7]^G1_register[0];
		feedback2 = (G2_register[8] + G2_register[7] + G2_register[4] + G2_register[2] + G2_register[1] + G2_register[0]) & 0x1;

		for(lcv2 = 0; lcv2 < 9; lcv2++)
		{
			G1_register[lcv2] = G1_register[lcv2+1];
			G2_register[lcv2] = G2_register[lcv2+1];
		}

		G1_register[9] = feedback1;
		G2_register[9] = feedback2;
	}

	/* Set the delay */
	delay = 1023 - delays[_prn];

	/* Generate PRN from G1 and G2 Registers */
	for(lcv = 0; lcv < 1023; lcv++)
	{
		_dest[lcv].i = G1[lcv]^G2[delay];
		_dest[lcv].q = 0;

		delay++;
		delay %= 1023;
	}

	return(1);

}
/*----------------------------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------------------------*/
/*!
 * sine_gen, generate a full scale sinusoid of frequency f with sampling frequency fs for _samps samps and put it into _dest
 * */
void sine_gen(CPX *_dest, double _f, double _fs, int32 _samps)
{

	int32 lcv;
	int16 c, s;
	float phase, phase_step;

	phase = 0;
	phase_step = (float)TWO_PI*_f/_fs;

	for(lcv = 0; lcv < _samps; lcv++)
	{
		c =	(int16)floor(16383.0*cos(phase));
		s =	(int16)floor(16383.0*sin(phase));
		_dest[lcv].i = c;
		_dest[lcv].q = s;

		phase += phase_step;
	}

}
/*----------------------------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------------------------*/
void sine_gen(CPX *_dest, double _f, double _fs, int32 _samps, double _p)
{

	int32 lcv;
	int16 c, s;
	double phase, phase_step;

	phase = _p;
	phase_step = (double)TWO_PI*_f/_fs;

	for(lcv = 0; lcv < _samps; lcv++)
	{
		c =	(int16)floor(16383.0*cos(phase));
		s =	(int16)floor(16383.0*sin(phase));
		_dest[lcv].i = c;
		_dest[lcv].q = s;

		phase += phase_step;
	}

}
/*----------------------------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------------------------*/
/*!
 * wipeoff_gen, generate a full scale sinusoid of frequency f with sampling frequency fs for _samps samps and put it into _dest
 * */
void wipeoff_gen(MIX *_dest, double _f, double _fs, int32 _samps)
{

	int32 lcv;
	int16 c, s;
	double phase, phase_step;

	phase = 0;
	phase_step = (double)TWO_PI*_f/_fs;

	for(lcv = 0; lcv < _samps; lcv++)
	{
		c =	(int16)floor(16383.0*cos(phase));
		s =	(int16)floor(16383.0*sin(phase));
		_dest[lcv].i = _dest[lcv].ni = c;
		_dest[lcv].q = s;
		_dest[lcv].nq = -s;

		phase += phase_step;
	}

}
/*----------------------------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------------------------*/
void downsample(CPX *_dest, CPX *_source, double _fdest, double _fsource, int32 _samps)
{

	int32 lcv, k;
	uint32 phase_step;
	uint32 lphase, phase;

	phase_step = (uint32)floor((double)4294967296.0*_fdest/_fsource);

	k = lphase = phase = 0;

	for(lcv = 0; lcv < _samps; lcv++)
	{
		if(phase <= lphase)
		{
			_dest[k] = _source[lcv];
			k++;
		}

		lphase = phase;
		phase += phase_step;
	}

}
/*----------------------------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------------------------*/
/*!
 * round_2, round a value to the next LOWEST value of 2^N
 * */
int32 round_2(int32 _N)
{











}
/*----------------------------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------------------------*/
/*!
 * Gather statistics and run AGC
 * */
int32 run_agc(CPX *_buff, int32 _samps, int32 _bits, int32 _scale)
{
	int32 lcv, num;
	int16 max, *p;
	int16 val;

	p = (int16 *)&_buff[0];

	val = (1 << _scale - 1);
	max = 1 << _bits;
	num = 0;

	if(_scale)
	{
		for(lcv = 0; lcv < 2*_samps; lcv++)
		{
			p[lcv] += val;
			p[lcv] >>= _scale;
			if(abs(p[lcv]) > max)
				num++;
		}
	}
	else
	{
		for(lcv = 0; lcv < 2*_samps; lcv++)
		{
			if(abs(p[lcv]) > max)
				num++;
		}
	}

	return(num);

}
/*----------------------------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------------------------*/
/*!
 * Get a rough first guess of scale value to quickly initialize agc
 * */
void init_agc(CPX *_buff, int32 _samps, int32 bits, int32 *scale)
{
	int32 lcv;
	int16 *p;
	int32 max;

	p = (int16 *)&_buff[0];

	max = 0;
	for(lcv = 0; lcv < 2*_samps; lcv++)
	{
		if(p[lcv] > max)
			max = p[lcv];
	}

	scale[0] = (1 << 14) / max;

}
/*----------------------------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------------------------*/
int32 Invert4x4(double A[4][4], double B[4][4])
{
	int32	column_swap[20];	/* Used	to keep	track of column	swapping. */
	int32	l, k, m;			/* Used	for	row	and	column indexing. */
	double pivot;
	double determinate;
	double swap;				/* Used	to swap	element	contents. */

	for(int32 i = 0; i < 4; i++)
		for(int32 j = 0; j < 4; j++)
			B[i][j] = A[i][j];

	determinate	= 1.0;

	for	(l = 0;	l <	4; l++)
		column_swap[l] = l;

	for	(l = 0;	l <	4; l++)
	{
		pivot =	0.0;
		m =	l;

		/* Find	the	element	in this	row	with the largest absolute value	-
		the	pivot. Pivoting	helps avoid	division by	small quantities. */
		for	(k = l;	k <	4; k++)
		{
			if (fabs (pivot) < fabs	(B[l][k]))
			{
				m =	k;
				pivot =	B[l][k];
			}
		}

		/* Swap	columns	if necessary to	make the pivot be the leading
		nonzero	element	of the row.	*/
		if (l != m)
		{
			k =	column_swap[m];
			column_swap[m] = column_swap[l];
			column_swap[l] = k;
			for	(k = 0;	k <	4; k++)
			{
				swap = B[k][l];
				B[k][l] =	B[k][m];
				B[k][m] =	swap;
			}
		}

		/* Divide the row by the pivot,	making the leading element
		1.0	and	multiplying	the	determinant	by the pivot. */
		B[l][l] = 1.0;
		determinate	= determinate *	pivot;	/* Determinant of the matrix. */
		if (fabs (determinate) < 1.0E-6)
			return(0);	/* Pivot = 0 therefore singular	matrix.	*/
		for	(m = 0;	m <	4; m++)
			B[l][m] /= pivot;

		/* Gauss-Jordan	elimination.  Subtract the appropriate multiple
		of the current row from	all	subsequent rows	to get the matrix
		into row echelon form. */

		for	(m = 0;	m <	4; m++)
		{
			if (l == m)
				continue;
			pivot =	B[m][l];
			if (pivot == 0.0)
			continue;
			B[m][l] =	0.0;
				for	(k = 0;	k <	4; k++)
			B[m][k] -= (pivot	* B[l][k]);
		}
	}

	/* Swap	the	columns	back into their	correct	places.	*/
	for	(l = 0;	l <	4; l++)
	{
		if (column_swap[l] == l)
		continue;

		/* Find	out	where the column wound up after	column swapping. */

		for	(m = l + 1;	m <	4; m++)
			if (column_swap[m] == l)
				break;

		column_swap[m] = column_swap[l];
		for	(k = 0;	k <	4; k++)
		{
			pivot =	B[l][k];
			B[l][k] =	B[m][k];
			B[m][k] =	pivot;
		}
		column_swap[l] = l;
	}

	return(1);

}
/*----------------------------------------------------------------------------------------------*/




/*----------------------------------------------------------------------------------------------*/
/*! atan2array, the fixed point lookup table for the function */
const int32 atan2array[129] =
{0, 64, 128, 192, 256, 320, 384, 448, 511, 575, 639, 702, 766, 829, 892, 956, 1019, 1082, 1144,
 1207, 1270, 1332, 1394, 1456, 1518, 1580, 1642, 1703, 1764, 1825, 1886, 1947, 2007, 2067,
 2127, 2187, 2246, 2305, 2364, 2423, 2481, 2539, 2597, 2655, 2712, 2769, 2826, 2883, 2939,
 2995, 3051, 3106, 3161, 3216, 3270, 3325, 3378, 3432, 3485, 3538, 3591, 3643, 3695, 3747,
 3798, 3849, 3900, 3950, 4000, 4050, 4100, 4149, 4197, 4246, 4294, 4342, 4389, 4437, 4483,
 4530, 4576, 4622, 4667, 4713, 4758, 4802, 4846, 4890, 4934, 4977, 5020, 5063, 5105, 5147,
 5189, 5230, 5272, 5312, 5353, 5393, 5433, 5473, 5512, 5551, 5590, 5628, 5666, 5704, 5741,
 5779, 5816, 5852, 5889, 5925, 5961, 5996, 6031, 6066, 6101, 6136, 6170, 6204, 6237, 6271,
 6304, 6337, 6369, 6402, 6434};
/*----------------------------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------------------------*/
/*!
	4 quadrant fixed point atan function.
*/
int32 Atan2Approx(int32 y, int32 x)
{
	int32 angle;		/* The arctangent. */
	int32 index;        /* Index into the arctangent array. */
	int32 swap;			/* Used to swap the arguments according to the quadrant. */

	int32 quadrant;		/* The quadrant of the result (1-4). */
	int32 sign;			/* The sign of the result. */

	bool greaterthanpiby4;/* Indicates result greater than pi/4. */

	/* First check for the invalid case of x and y equal to 0. */
	if(x == 0 && y == 0)
		return (0);

	sign = 1;			/* +ve. */
	if (y < 0)          /* Check if the result is -ve., y<0. */
	{
		sign = -1;      /* -ve. */
		y = -y;         /* Put y in the 1st or 4th quadrant */
	}

	/* Check if the result will be in the 1st or 4th quadrant. */
	quadrant = 1;
	if (x < 0)
	{
		quadrant = 4;
		x = -x;
	}

	/* Check if the result will be greater than pi/4. */
	greaterthanpiby4 = FALSE;
	if (y > x)
	{
		greaterthanpiby4 = TRUE;
		swap = x;
		x = y;
		y = swap;
	}

	/* Get the index into the atan array (Niles fix) **/
	index = (y * 128 + x / 2) / x;

	angle = (long) atan2array[index];

	if (greaterthanpiby4)  /* Put the result in the right quadrant. */
		angle = 12868 - angle;

	if (quadrant == 4)
		angle = 25735 - angle;

	return ((int32) sign * angle);
}
/*----------------------------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------------------------*/
int32 AtanApprox(int32 y, int32 x)
{
	if (x < 0)           /* Map x and y to the 1st and 2nd quadrants. */
	{
		x = -x;
		y = -y;
	}

	int32 angle;		/* The arctangent. */
	int32 index;        /* Index into the arctangent array. */
	int32 swap;			/* Used to swap the arguments according to the quadrant. */

	int32 quadrant;		/* The quadrant of the result (1-4). */
	int32 sign;			/* The sign of the result. */

	bool greaterthanpiby4; /* Indicates result greater than pi/4. */

	/* First check for the invalid case of x and y equal to 0. */
	if(x == 0 && y == 0)
		return (0);

	sign = 1;			/* +ve. */
	if (y < 0)          /* Check if the result is -ve., y<0. */
	{
		sign = -1;      /* -ve. */
		y = -y;         /* Put y in the 1st or 4th quadrant */
	}

	/* Check if the result will be in the 1st or 4th quadrant. */
	quadrant = 1;
	if (x < 0)
	{
		quadrant = 4;
		x = -x;
	}

	/* Check if the result will be greater than pi/4. */
	greaterthanpiby4 = FALSE;
	if (y > x)
	{
		greaterthanpiby4 = TRUE;
		swap = x;
		x = y;
		y = swap;
	}

	/* Get the index into the atan array (Niles fix) **/
	index = (y * 128 + x / 2) / x;

	angle = (long) atan2array[index];

	if (greaterthanpiby4)  /* Put the result in the right quadrant. */
		angle = 12868 - angle;

	if (quadrant == 4)
		angle = 25735 - angle;

	return ((int32) sign * angle);
}
/*----------------------------------------------------------------------------------------------*/



/*----------------------------------------------------------------------------------------------*/
/* Form the CCSDS packet header with the given _apid, sequence flag, and packet length, and command bit */
void FormCCSDSPacketHeader(CCSDS_Packet_Header *_p, uint32 _apid, uint32 _sf, uint32 _pl, uint32 _cm, uint32 _tic)
{

	_p->pid = ((_cm & 0x1) << 12) + (_apid + CCSDS_APID_BASE) & 0x7FF;
	_p->psc = (_sf & 0x3) + ((_tic & 0x3FFF) << 2);
	_p->pdl = _pl & 0xFFFF;

}
/* Decode a command into its components */
void DecodeCCSDSPacketHeader(CCSDS_Decoded_Header *_d, CCSDS_Packet_Header *_p)
{

	_d->id 		= _p->pid - CCSDS_APID_BASE;
	_d->type 	= _p->psc & 0x3;
	_d->tic		= (_p->psc >> 2) & 0x3FFF;
	_d->length 	= _p->pdl & 0xFFFF;

}
/*----------------------------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------------------------*/
#define MOD_ADLER 65521
/* Data is input buffer, len is in bytes! */
uint32 adler(uint8 *data, int32 len)
{

    uint32 a, b;
    int32 tlen;

    a = 1; b = 0;

    if(len < 5552)
    {
        do
        {
            a += *data++;
            b += a;
        } while (--len);

        a %= MOD_ADLER;
        b %= MOD_ADLER;
    }

    return (b << 16) | a;

}
/*----------------------------------------------------------------------------------------------*/

