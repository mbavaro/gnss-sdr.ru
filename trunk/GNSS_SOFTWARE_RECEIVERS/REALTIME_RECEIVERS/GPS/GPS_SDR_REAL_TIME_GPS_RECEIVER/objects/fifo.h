/*----------------------------------------------------------------------------------------------*/
/*! \file fifo.h
//
// FILENAME: fifo.h
//
// DESCRIPTION: Defines the FIFO class.
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

#ifndef FIFO_H_
#define FIFO_H_

#include "includes.h"
#include "gps_source.h"

#define FIFO_DEPTH (4000)	//!< In ms

/*! \ingroup CLASSES
 *
 */
class FIFO : public Threaded_Object
{

	private:

		sem_t sem_full;
		sem_t sem_empty;

		ms_packet *buff;	//!< 1 second buffer (in 1 ms packets)
		ms_packet *head;	//!< Pointer to the head
		ms_packet *tail;	//!< Pointer to the tail

		int32 count;		//!< Count the number of packets received
		int32 tic;			//!< Master receiver tic

	public:

		FIFO();				//!< Create circular FIFO
		~FIFO();			//!< Destroy circular FIFO
		void Start();		//!< Start the thread
		void Import();		//!< Get data into the thread
		void Export();		//!< Get data out of the thread

		void Open();
		void Enqueue();
		void Dequeue(ms_packet *p);
		void ResetSource();
};

#endif /* FIFO_H */


