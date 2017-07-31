/*----------------------------------------------------------------------------------------------*/
/*! \file signaldef.h
//
// FILENAME: signaldef.h
//
// DESCRIPTION: Describe the IF data's characteristics
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

#ifndef SIGNALDEF_H_
#define SIGNALDEF_H_

/* Data from the Universal Software Radio Peripheral */
/*----------------------------------------------------------------------------------------------*/
#define IF_SAMPLE_FREQUENCY	(2048000)				//!< How many CPX samples in a second
#define IF_FREQUENCY		(38400)					//!< IF frequency
#define ZERO_DOPPLER_RATE	(IF_FREQUENCY)			//!< IF frequency
#define NCO_CARR_INCR_NOM	(0x04CCCCCC)			//!< 38400 Hz
#define NCO_CODE_INCR_NOM	(0x3FF00000)			//!< = 1.023e6 Mcps, 1 chips/s = 1049 in terms of the code NCO
#define F_SAMPLE_NOM		(64e6)					//!< Nominal USRP sample rate
#define SAMPS_PER_READ		(4096)					//!< Nominal samples per read
#define BYTES_PER_READ  	(4*SAMPS_PER_READ)		//!< Read this many bytes
#define HZ_2_NCO_CARR_INCR	(2.097152000000000e+03) //!< Convert from Hz to carrier NCO units
#define NCO_CARR_INCR_2_HZ	(4.768371582031250e-04) //!< Convert from carrier NCO to Hz
#define HZ_2_NCO_CODE_INCR  (1.048576000000000e+03) //!< Convert from Hz to code NCO units
#define NCO_CODE_INCR_2_HZ  (9.536743164062500e-04) //!< Convert from code NCO to Hz
/*----------------------------------------------------------------------------------------------*/

#endif



