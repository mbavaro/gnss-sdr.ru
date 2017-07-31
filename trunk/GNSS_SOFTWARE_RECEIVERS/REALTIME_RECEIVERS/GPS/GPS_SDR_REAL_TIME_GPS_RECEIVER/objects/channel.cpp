/*----------------------------------------------------------------------------------------------*/
/*! \file channel.cpp
//
// FILENAME: channel.cpp
//
// DESCRIPTION: Implements member functions of Channel class.
//
// DEVELOPERS: Gregory W. Heckler (2003-2009)
//
// LICENSE TERMS: Copyright (c) Gregory W. Heckler 2009
//
// This file is part of the GPS Software Defined Radio (GPS-SDR)
//
// The GPS-SDR is free software; you can redistribute it and/or modify it under the terms of the
// GNU General Public License as published by the Free Software Foundation; either version 2 of
// the License, or (at your option) any later version. The GPS-SDR is distributed in the hope that
// it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
// more details.
//
// Note:  Comments within this file follow a syntax that is compatible with
//        DOXYGEN and are utilized for automated document extraction
//
// Reference:
*/
/*----------------------------------------------------------------------------------------------*/

#include "channel.h"

/*----------------------------------------------------------------------------------------------*/
Channel::Channel(int32 _chan):Threaded_Object("CHNTASK")
{
	char fname[1024];

	chan = _chan;

	if(gopt.verbose)
		fprintf(stdout,"Creating Channel %d\n",chan);

	if(gopt.log_channel)
	{
		sprintf(fname,"chan%02d.dat",chan);
		fp = fopen(fname, "wb");
	}

	pFFT = new FFT(FREQ_LOCK_POINTS);

	Clear();

}
/*----------------------------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------------------------*/
Channel::~Channel()
{

	delete pFFT;

	if(gopt.log_channel)
		fclose(fp);

	if(gopt.verbose)
		fprintf(stdout,"Destructing Channel %d\n",chan);

}
/*----------------------------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------------------------*/
void Channel::Clear()
{

	/* Status info */
	len = 1;
	count = 0;
	state = CHANNEL_EMPTY;
	sv = 666;

	/* Loop data */
	memset(&aPLL, 0x0, sizeof(Phase_lock_loop));
	memset(&aDLL, 0x0, sizeof(Delay_lock_loop));

	/* Correlations */
	I[0] = I[1] = I[2] = 1;
	Q[0] = Q[1] = Q[2] = 1;
	P[0] = P[1] = P[2] = 1;
	I_prev = Q_prev = 1;		//Important to prevent divide by zero
	I_avg = 1;
	Q_var = 1;
	P_avg = 8e4;
	cn0 = 40.0;

	/* Bit lock stuff */
	bit_lock = false;
	bit_lock_pend = false;
	bit_lock_ticks = 0;
	I_sum20 = 0;
	Q_sum20 = 0;
	memset(&I_buff, 0x0, 20*sizeof(int32));
	memset(&Q_buff, 0x0, 20*sizeof(int32));
	memset(&P_buff, 0x0, 20*sizeof(int32));
	_20ms_epoch = 0;
	_1ms_epoch = 0;
	best_epoch = 0;

	/* Data message sheit */
	memset(&valid_frame, 0x0, 5*sizeof(int32));
	converged = false;
	navigate = false;
	z_lock = false;
	frame_z = 0;
	z_count = 0;
	memset(&word_buff, 0x0, FRAME_SIZE_PLUS_2*sizeof(uint32));

	/* Frame synch, process data bit sheit */
	frame_lock = false;
	frame_lock_pend = false;
	bit_number = 0;
	subframe = 0;

	/* FFT and buffer for FFT estimate of frequency after initial lock */
	freq_lock_ticks = 0;
	freq_lock = false;
	memset(&fft_buff[0], 0x0, FREQ_LOCK_POINTS*sizeof(CPX));


}
/*----------------------------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------------------------*/
void Channel::Start(int32 _sv, Acq_Command_S result, int32 _corr_len)
{

	/* Clear out the channel */
	Clear();

	sv 			= _sv;
	code_nco	= CODE_RATE + result.doppler*CODE_RATE/L1;
	carrier_nco	= IF_FREQUENCY + result.doppler;

	aDLL.x		= 2.0*result.doppler*CODE_RATE/L1;

	aPLL.w		= 0;
	aPLL.x 		= 2.0*result.doppler;
	aPLL.z 		= result.doppler;

	switch(_corr_len)
	{
		case 1:
			len = 1;
			PLL_W(18.0);
			break;
		case 20:
			len = 20;
			PLL_W(18.0);
			break;
		default:
			len = 1;
			PLL_W(18.0);
			break;
	}

	DLL_W(1.0);

	state = CHANNEL_NORMAL;

}
/*----------------------------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------------------------*/
Channel_M Channel::getPacket()
{
	return(packet);
}
/*----------------------------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------------------------*/
void Channel::Accum(Correlation_S *corr, NCO_Command_S *_feedback)
{

	corr->I[0] >>= 2;
	corr->I[1] >>= 2;
	corr->I[2] >>= 2;
	corr->Q[0] >>= 2;
	corr->Q[1] >>= 2;
	corr->Q[2] >>= 2;

	/* Integrate */
	I[0] += corr->I[0];
	I[1] += corr->I[1];
	I[2] += corr->I[2];
	Q[0] += corr->Q[0];
	Q[1] += corr->Q[1];
	Q[2] += corr->Q[2];

	/* Always do these, a running sum of past 20 1ms accumulations */
	I_sum20	+= corr->I[1] - I_buff[_1ms_epoch];
	Q_sum20 += corr->Q[1] - Q_buff[_1ms_epoch];

	/* Buffer storing past 20 1ms accumulations */
	I_buff[_1ms_epoch] = corr->I[1];
	Q_buff[_1ms_epoch] = corr->Q[1];

	if((I_buff[_1ms_epoch] > 0) !=  (I_buff[(_1ms_epoch + 19) % 20] > 0))
	{
		P_buff[(_1ms_epoch + 19) % 20]++;
	}

	/* Lowpass filter */
	//P_buff[_1ms_epoch] = (63 * P_buff[_1ms_epoch] + (I_sum20 >> 6) * (I_sum20 >> 6)	+ (Q_sum20 >> 6)*(Q_sum20 >> 6) + 32) >> 6;
	//P_buff[_1ms_epoch] = (63 * P_buff[_1ms_epoch] + (I_sum20 >> 6) * (I_sum20 >> 6)	+ 32) >> 6;

	/* Dump accumulation and do tracking according to integration length */
	if((_1ms_epoch % len) == 0)
	{
		Export();
		DumpAccum();
	}

	/* These functions must be called every ms */
	EstCN0();
	BitLock();
	BitStuff();
	Epoch();

	/* Kill the correlator if the channel has stopped itself */
	if(state == CHANNEL_EMPTY)
		_feedback->kill = true;
	else
		_feedback->kill = false;

	/* copy over the updated NCO values */
	_feedback->carrier_nco = carrier_nco;
	_feedback->code_nco = code_nco;

	/* send frame and bit lock resets */
	if(bit_lock_pend && (_1ms_epoch == 0))
	{
		_feedback->reset_1ms = true;
		bit_lock_pend = false;
	}
	else
		_feedback->reset_1ms = false;

	if(frame_lock_pend)
	{
		_feedback->reset_20ms = true;
		frame_lock_pend = false;
	}
	else
		_feedback->reset_20ms = false;

	if(z_count_pend)
	{
		_feedback->set_z_count = 1;
		_feedback->z_count = z_count;
		z_count_pend = false;
	}
	else
		_feedback->set_z_count = false;

	if(converged)
	{
		navigate = true;
		_feedback->navigate = navigate;
	}
	else
	{
		navigate = false;
		_feedback->navigate = navigate;
	}

}
/*----------------------------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------------------------*/
void Channel::DumpAccum()
{
	/* Compute the powers */
	P[0] = (I[0] * I[0]) + (Q[0] * Q[0]);
	P[1] = (I[1] * I[1]) + (Q[1] * Q[1]);
	P[2] = (I[2] * I[2]) + (Q[2] * Q[2]);

	/* Lowpass filtered values here */
	I_avg += (fabs((float)I[1]) - I_avg) * .02;
	Q_var += ((float)Q[1]*(float)Q[1] - Q_var) * .02;
	P_avg += ((float)P[1]/len - P_avg) * .02;

	/* First do estimate of frequency offset via FFT */
	if(freq_lock == false)
	{
		FrequencyLock();
	}
	else
	{
		PLL();
	}

	DLL();

	/* Dump pertinent data */
	Error();

	/* Save Previous Correlations for Loops */
	I_prev = I[1];
	Q_prev = Q[1];

	/* Zero out the correlations */
	I[0] = I[1] = I[2] = 0;
	Q[0] = Q[1] = Q[2] = 0;

}
/*----------------------------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------------------------*/
void Channel::EstCN0()
{
	int32 lcv;
	float NP;
	float NBP;
	float WBP;
	float ncn0;

	/* Try out new cn0 estimate, PG 393 of Global Positioning System, Theory and Applications */
	if((_1ms_epoch == 19) && bit_lock)
	{
		NBP = I_sum20*I_sum20 + Q_sum20*Q_sum20;
		WBP = 0;

		for(lcv = 0; lcv < 20; lcv++)
			WBP += I_buff[lcv]*I_buff[lcv] + Q_buff[lcv]*Q_buff[lcv];

		if(WBP > 0.0)
			NP = NBP/WBP;

		if((NP - 1.0)/(20.0 - NP) > 0.0)
		{
			ncn0 = 10*log10((NP - 1.0)/(20.0 - NP)) + 30.0 + .25;
			cn0 += (ncn0 - cn0)*.02;
		}

		if(cn0 < 15.0)
		{
			cn0 = 15.0;
		}
	}

}
/*----------------------------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------------------------*/
void Channel::FrequencyLock()
{

	int32 it, qt;
	int32 max, mind, lcv;
	float df;
	int32 *p;

	it = I[1] >> 3;
	qt = Q[1] >> 3;

	/* First frequency double to remove data bits */
	if(count > 1000)
	{
		fft_buff[freq_lock_ticks].i = (int16)(it*it - qt*qt);
		fft_buff[freq_lock_ticks].q = (int16)(2*it*qt);
		freq_lock_ticks++;
	}

	if(freq_lock_ticks >= FREQ_LOCK_POINTS)
	{

		p = (int32 *)&fft_buff[0];

		/* Now do the FFT */
		pFFT->doFFT(&fft_buff[0], true);

		/* Convert to power */
		x86_cmag(&fft_buff[0], FREQ_LOCK_POINTS);

		/* Get peak */
		max = mind = 0;
		for(lcv = 0; lcv < FREQ_LOCK_POINTS; lcv++)
		{
			if(p[lcv] > max)
			{
				max = p[lcv];
				mind = lcv;
			}
		}

		/* Positive and negative frequency adjustment */
		if(mind >= (FREQ_LOCK_POINTS/2))
			mind -= FREQ_LOCK_POINTS;

		/* Convert to frequency correction */
		df = 1000.0/((float)2.0*len);	// Bandwidth of FFT
		df /= (float)FREQ_LOCK_POINTS;	// Spacing of FFT bins
		df *= (float)mind;				// Convert to frequency offset

		/* Update the loop filters */
		aDLL.x += 2.0*df*CODE_RATE/L1;
		aPLL.x += 2.0*df;

		freq_lock = true;
		freq_lock_ticks = 0;
	}
}
/*----------------------------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------------------------*/
/*! Make this 2nd order ? */
void Channel::DLL()
{

	double code_err;
	double ep, lp, sp;

	ep = sqrt(double(P[0]));
	lp = sqrt(double(P[2]));
	sp = sqrt(double(P[2] + P[0]));

	code_err  = (ep - lp) / sp;

	/* Not working too well right now, debug some later */
//	aDLL.x += aDLL.t*(code_err*aDLL.w02);
//	aDLL.z = 0.5*aDLL.x + aDLL.a*aDLL.w02*code_err;
//	code_nco = CODE_RATE + (0.5*aPLL.x*CODE_RATE*INVERSE_L1) + aDLL.z;

	if((count < 1000) && (P_avg < 8e4))
	{
		code_nco = CODE_RATE + (0.5 * aPLL.x * CODE_RATE * INVERSE_L1) - 5.0;
	}
	else
	{
		code_nco = CODE_RATE + (0.5 * aPLL.x * CODE_RATE * INVERSE_L1) + code_err;
	}
}
/*----------------------------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------------------------*/
void Channel::PLL()
{

	double df;
	double dp;
	double dot;
	double cross;
	int32 I1, I2, Q1, Q2, lcv;

	df = dp = 0;

//	if(bit_lock)
//	{
//		/* FLL discriminator */
//		dot = 	  I_prev*I[1] + Q_prev*Q[1];
//		cross =   I_prev*Q[1] - Q_prev*I[1];
//
//		/* No FLL for now */
//		if((dot != 0.0) && (cross != 0.0))
//		{
//			df = atan2(cross, dot);
//			df /= (TWO_PI * aPLL.t);
//		}
//	}

//	df = 0;

	/* PLL discriminator */
	if(I[1] != 0)
	{
		dp = atan((double)Q[1]/(double)I[1])/TWO_PI;
	}

	/* Lock indicators */
	aPLL.pll_lock += (dp - aPLL.pll_lock) * .1;
	aPLL.pll_lock = dp;
	aPLL.fll_lock += (cross/P_avg - aPLL.fll_lock) * .1;

	/* 3rd order PLL wioth 2nd order FLL assist */
	aPLL.w += aPLL.t * (aPLL.w0p3 * dp + aPLL.w0f2 * df);
	aPLL.x += aPLL.t * (0.5*aPLL.w + aPLL.a2 * aPLL.w0f * df + aPLL.a3 * aPLL.w0p2 * dp);
	aPLL.z  = 0.5*aPLL.x + aPLL.b3 * aPLL.w0p * dp;

	carrier_nco = IF_FREQUENCY + aPLL.z;

}
/*----------------------------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------------------------*/
void Channel::Epoch()
{

	_1ms_epoch++;
	if(_1ms_epoch >= 20)
	{
		_1ms_epoch = 0;
		_20ms_epoch++;

		if(_20ms_epoch >= 300)
		{
			z_count += 6;
			_20ms_epoch = 0;
		}
	}

	count++;
}
/*----------------------------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------------------------*/
void Channel::BitLock()
{
	int32 power_buff[20];
	int32 lcv, new_epoch;
	int32 thresh_high, thresh_low, bit_lock_err;

	/* Set bitlock threshold */
	thresh_high = 100;
	thresh_low = 25;

	if(_1ms_epoch == 19)
	{
		if(bit_lock == false)
		{
			/* Find the maximum of the power buffer */
			bit_lock_err = 0; new_epoch = 0;
			for(lcv = 0; lcv < 20; lcv++)
			{
				if(P_buff[lcv] > thresh_high)
				{
					bit_lock = true;
					new_epoch = lcv;
				}

				if(P_buff[lcv] > thresh_low)
				{
					bit_lock_err++;
				}
			}

			/* Bin counter exceeded threshold */
			if(bit_lock == true)
			{
				best_epoch = 19;
				bit_lock_pend = 1;
				bit_lock_ticks = 0;
				_1ms_epoch = (38 - new_epoch) % 20;

				/* Copy over the power buffer to put the max in element 19 */
				for(lcv = 0 ; lcv < 20; lcv++)
					power_buff[lcv] = P_buff[lcv];

				for(lcv = 0 ; lcv < 20; lcv++)
					P_buff[lcv] = power_buff[(lcv + new_epoch + 1) % 20];
			}

			/* Bit lock failure, reset! */
			if(bit_lock_err > 1)
			{
				best_epoch = 0;
				bit_lock_ticks = 0;
				bit_lock = false;
				frame_lock = false;
				for(lcv = 0 ; lcv < 20; lcv++)
					P_buff[lcv] = 0;
			}

		}
		else if(bit_lock_ticks < 60000) /* Bitlock obtained, keep monitoring it up to 1 minute */
		{
			/* Find the maximum of the power buffer */
			bit_lock_err = 0; new_epoch = 0;
			for(lcv = 0; lcv < 20; lcv++)
			{
				if(P_buff[lcv] > bit_lock_err)
				{
					bit_lock_err = P_buff[lcv];
					new_epoch = lcv;
				}
			}

			/* Best epoch not in the correct place! */
			if(new_epoch != 19)
			{
				best_epoch = 0;
				bit_lock_ticks = 0;
				bit_lock = false;
				frame_lock = false;
				for(lcv = 0 ; lcv < 20; lcv++)
					P_buff[lcv] = 0;
			}
		}
	}

	bit_lock_ticks++;

}
/*----------------------------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------------------------*/
void Channel::BitStuff()
{

	uint32 lcv, temp_bit, feedbit;

	if(bit_lock && (_1ms_epoch == 19))
	{

		/* Make a bit decision */
		if(I_sum20 > 0)
			temp_bit = 1;
		else
			temp_bit = 0;

		/* Now add the bit into the buffer */
		/* Each word in wordbuff is composed of:
		Bits 0 to 29 = the GPS data word
		Bits 30 to 31 = 2 LSBs of the GPS word ahead. */
		for(lcv = 0; lcv <= (FRAME_SIZE_PLUS_2-2); lcv++)
		{
			/* Shift the data word 1 place to the left. */
			word_buff[lcv] <<= 1;

			/* Add the MSB of the following GPS word (bit 29 of a wordbuff word). */
			feedbit = (word_buff[lcv+1] >> 29) & 0x00000001;
			word_buff[lcv] += feedbit;
		}

		/* The bit added for the 12th word is the new data bit. */
		word_buff[FRAME_SIZE_PLUS_2-1] <<= 1;
		word_buff[FRAME_SIZE_PLUS_2-1] += temp_bit;

		ProcessDataBit();
	}

}
/*----------------------------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------------------------*/
void Channel::ProcessDataBit()
{

	int32 sid, lcv;
	uint32 word0, word1;

	/* Else attempt frame sync. */
	if(!frame_lock)
	{
		word0 = word_buff[FRAME_SIZE_PLUS_2-2];
		word1 = word_buff[FRAME_SIZE_PLUS_2-1];

		if(FrameSync(word0, word1))
		{
			frame_lock = true;
			frame_lock_pend = 1;
			bit_number = 299;
			_20ms_epoch = 60;
		}
	}

	if(frame_lock)
	{
		bit_number = (bit_number + 1) % 300;

		if(bit_number == 0)
		{
			memcpy(&ephem_packet.word_buff[0], word_buff, FRAME_SIZE_PLUS_2*sizeof(uint32));

			if(ValidFrameFormat(&ephem_packet.word_buff[0]))
			{
				sid = (int32)((ephem_packet.word_buff[1] >> 8) & 0x00000007);
				frame_z = 4*((ephem_packet.word_buff[1] >> 13) & 0x0001FFFF);

				if((sid > 0) && (sid < 6))
				{
					subframe = sid;
					valid_frame[sid-1] = true;
					ephem_packet.subframe = subframe;
					ephem_packet.sv = sv;

					write(CHN_2_EPH_P[WRITE], &ephem_packet, sizeof(Channel_2_Ephemeris_S));

					if(!z_lock)
					{
						z_count_pend = true;
						z_count = 3*frame_z/2;
						z_lock = true;
						navigate = true;
						converged = true;
					}

				}
				else
				{
					subframe = 0;
					valid_frame[0] = valid_frame[1] = valid_frame[2] = valid_frame[3] = valid_frame[4] = false;
					frame_lock = false;
				}
			}		//end valid subframe
			else
			{
				subframe = 0;
				valid_frame[0] = valid_frame[1] = valid_frame[2] = valid_frame[3] = valid_frame[4] = false;
				frame_lock = false;
			}
		}		//end if bit_number = 300;
	}



}
/*----------------------------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------------------------*/
bool Channel::FrameSync(uint32 word0, uint32 word1)
{
    uint32 preamble;           /* The TLM word preamble sequence 10001011. */
    uint32 sid;                               /* The subframe ID (1 to 5). */
    uint32 zerobits;         /* The zero bits (the last 2 bits of word 2). */

    /* Extract the preamble. */
    preamble = (word0 >> 22) & 0x000000FF;

    /* Extract the subframe ID. */
    sid = (word1 >> 8) & 0x00000007;

    /* Extract the zero bits. */
    zerobits = word1 & 0x00000003;

    /* Invert the extracted data according to bit 30 of the previous word. */
    if (word0 & 0x40000000)
    {
        preamble ^= 0x000000FF;
        zerobits ^= 0x00000003;
    }
    if (word1 & 0x40000000)
        sid ^= 0x00000007;

    /* Check that the preamble, subframe and zero bits are ok. */
    if(preamble != PREAMBLE)
        return false;

    /* Check if the subframe ID is ok. */
    if(sid < 1 || sid > 5)
        return false;

    /* Check that the zero bits are ok. */
    if(zerobits!=0)
        return false;

    /* Check that the 2 most recently logged words pass parity. Have to first
       invert the data bits according to bit 30 of the previous word. */
    if(word0 & 0x40000000)
        word0 ^= 0x3FFFFFC0;

    if(word1 & 0x40000000)
        word1 ^= 0x3FFFFFC0;

    if(!this->ParityCheck(word0) || !this->ParityCheck(word1))
        return false;

    return true;
}
/*----------------------------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------------------------*/
bool Channel::ParityCheck(uint32 gpsword)
{

    uint32 d1,d2,d3,d4,d5,d6,d7,t,parity;

    /* XOR as many bits in parallel as possible.  The magic constants pick
       up bits which are to be XOR'ed together to implement the GPS parity
       check algorithm described in ICD-GPS-200.  This avoids lengthy shift-
       and-xor loops. */

    d1 = gpsword & 0xFBFFBF00;
    d2 = _lrotl(gpsword,1) & 0x07FFBF01;
    d3 = _lrotl(gpsword,2) & 0xFC0F8100;
    d4 = _lrotl(gpsword,3) & 0xF81FFE02;
    d5 = _lrotl(gpsword,4) & 0xFC00000E;
    d6 = _lrotl(gpsword,5) & 0x07F00001;
    d7 = _lrotl(gpsword,6) & 0x00003000;

    t = d1 ^ d2 ^ d3 ^ d4 ^ d5 ^ d6 ^ d7;

    // Now XOR the 5 6-bit fields together to produce the 6-bit final result.

    parity = t ^ _lrotl(t,6) ^ _lrotl(t,12) ^ _lrotl(t,18) ^ _lrotl(t,24);
    parity = parity & 0x3F;
    if (parity == (gpsword&0x3F))
        return(true);
    else
        return(false);

}
/*----------------------------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------------------------*/
bool Channel::ValidFrameFormat(uint32 *subframe)
{
	uint32 preamble,next_preamble;
    uint32 zerobits,next_zerobits;
    uint32 sid,next_sid;
    uint32 wordcounter;
    uint32 parity_errors;

    /* Extract the preamble. */
    preamble = (subframe[0] >> 22) & 0x000000FF;

    /* Extract the subframe ID. */
    sid = (subframe[1] >> 8) & 0x00000007;

    /* Extract the zero bits. */
    zerobits = subframe[1] & 0x00000003;

    /* Invert the extracted data according to bit 30 of the previous word. */
    if(subframe[0] & 0x40000000)
	{
		preamble ^= 0x000000FF;
		zerobits ^= 0x00000003;
	}

    if(subframe[1] & 0x40000000)
        sid ^= 0x00000007;

    /* Check that the preamble, subframe and zero bits are ok. */
    if(preamble != PREAMBLE)
        return(false);

    if(sid < 1 || sid > 5)
        return(false);

    if(zerobits != 0)
        return(false);

    /* Extract the next preamble (10001011). */
    next_preamble = (subframe[FRAME_SIZE_PLUS_2-2] >> 22) & 0x000000FF;

    /* Extract the next subframe ID. */
    next_sid = (subframe[FRAME_SIZE_PLUS_2-1] >> 8) & 0x00000007;

    /* Extract the next zero bits. */
    next_zerobits = (subframe[FRAME_SIZE_PLUS_2-1] & 0x00000003);

    /* Invert the extracted data according to bit 30 of the previous word. */
    if(subframe[FRAME_SIZE_PLUS_2-2] & 0x40000000)
	{
		next_preamble ^= 0x000000FF;
		next_zerobits ^= 0x00000003;
	}

    if(subframe[FRAME_SIZE_PLUS_2-1] & 0x40000000)
        next_sid ^= 0x00000007;

    /* Check that the preamble, subframe and zero bits are ok. */
    if(next_preamble != PREAMBLE)
        return(false);

    if(next_sid < 1 || next_sid > 5)
        return(false);

    if(next_zerobits != 0)
        return(false);

    /* Check that the subframe IDs are consistent. */
    if((next_sid-sid) != 1 && (next_sid-sid) != -4)
        return(false);

    /* Check that all 12 words have correct parity. Have to first
       invert the data bits according to bit 30 of the previous word. */
    parity_errors = 0;

	for(wordcounter = 0; wordcounter < FRAME_SIZE_PLUS_2; wordcounter++)
		if (subframe[wordcounter] & 0x40000000)
			subframe[wordcounter] ^= 0x3FFFFFC0;

    for(wordcounter = 0; wordcounter < FRAME_SIZE_PLUS_2; wordcounter++)
		if (!this->ParityCheck(subframe[wordcounter]))
			parity_errors++;

    if(parity_errors != 0)
        return(false);

    return(true);
}
/*----------------------------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------------------------*/
void Channel::PLL_W(float _bwpll)
{

	aPLL.PLLBW = _bwpll;
	aPLL.FLLBW = 4.0;

	aPLL.b3 = 2.40;
	aPLL.a3 = 1.10;
	aPLL.a2 = 1.414;
	aPLL.w0p = aPLL.PLLBW/0.7845;
	aPLL.w0p2 = aPLL.w0p*aPLL.w0p;
	aPLL.w0p3 = aPLL.w0p2*aPLL.w0p;
	aPLL.w0f = aPLL.FLLBW/0.53;
	aPLL.w0f2 = aPLL.w0f*aPLL.w0f;
	aPLL.gain = 1.0;
	aPLL.t = .001*(float)len;

}
/*----------------------------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------------------------*/
void Channel::DLL_W(float _bwdll)
{

	aDLL.DLLBW = _bwdll;
	aDLL.a = 1.414;
	aDLL.w0 = aDLL.DLLBW/0.7845;
	aDLL.w02 = aDLL.w0*aDLL.w0;
	aDLL.t = .001*(float)len;

}
/*----------------------------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------------------------*/
void Channel::Error()
{

	/* Monitor DLL */
	if((P_avg < 8e4) && (count > 1000))
		Kill();

	/* Monitor cn0 for false PLL lock */
	if((count == 15000) && (bit_lock == false) && (freq_lock == true))
	{
		freq_lock_ticks = 0;
		freq_lock = false;
	}

	/* If 30 seconds have passed and channel has not converged dump it */
	if((count > 30000) && (converged == false))
		Kill();

	/* The channel should be killed if the nco goes outside the pre generated wipeoff table */
	if(fabs(carrier_nco-IF_FREQUENCY) > CARRIER_BINS*CARRIER_SPACING)
		Kill();

	/* Adjust integration length based on cn0 */
	if(bit_lock)
	{
		if((cn0 > 39.0) && (len != 1))
		{
			len = 1;
			PLL_W(18.0);
		}

		if((cn0 < 37.0) && (len != 20))
		{
			len = 20;
			PLL_W(18.0);
		}
	}

}
/*----------------------------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------------------------*/
void Channel::Kill()
{
	state = CHANNEL_EMPTY;
	Clear();
}
/*----------------------------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------------------------*/
void Channel::Export()
{

	int32 bwrote;

	packet.chan 		= chan;
	packet.state		= state;
	packet.sv 			= sv;
	packet.antenna		= antenna;
	packet.len 			= len;
	packet.w			= aPLL.w;
	packet.x 			= aPLL.x;
	packet.z 			= aPLL.z;
	packet.cn0 			= cn0;
	packet.p_avg 		= P_avg;
	packet.bit_lock 	= bit_lock;
	packet.frame_lock 	= frame_lock;
	packet.navigate		= navigate;
	packet.count		= count;
	packet.subframe 	= subframe;
	packet.best_epoch 	= best_epoch;
	packet.code_nco 	= code_nco;
	packet.carrier_nco 	= carrier_nco;

	/* Dump the extra info */
	if(gopt.log_channel && (fp != NULL))
	{
		fwrite(&packet, sizeof(Channel_M), 1,  fp);
		fwrite(&I[0], sizeof(int32), 3,  fp);
		fwrite(&Q[0], sizeof(int32), 3,  fp);
		fwrite(&P_buff[0], sizeof(int32), 20,  fp);
	}
}
/*----------------------------------------------------------------------------------------------*/

