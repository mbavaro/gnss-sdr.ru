/*! \file GUI_Serial.cpp
	Implements member functions of GUI_Serial class.
*/
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

#include "gui_serial.h"
#define SWAP64(X) *((long long *)(&X)) = (((*((long long *)(&X)) & (0x00000000ffffffffLLU)) << 32) | ((*((long long *)(&X)) & (0xffffffff00000000LLU)) >> 32))

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

static uint32 SIZEOF_M[LAST_M_ID + 1] =
{
	0,
	0,
	sizeof(Board_Health_M),
	sizeof(Task_Health_M),
	sizeof(SPS_M),
	sizeof(TOT_M),
	sizeof(PPS_M),
	sizeof(Clock_M),
	sizeof(Channel_M),
	sizeof(SV_Position_M),
	sizeof(Measurement_M),
	sizeof(Pseudorange_M),
	sizeof(SV_Prediction_M),
	0,
	sizeof(EKF_State_M),
	sizeof(EKF_Covariance_M),
	sizeof(EKF_Residual_M),
	0,
	sizeof(Command_Ack_M),
	sizeof(Ephemeris_M),
	sizeof(Almanac_M),
	sizeof(Ephemeris_Status_M),
	sizeof(SV_Select_Status_M),
	sizeof(EEPROM_M),
	sizeof(EEPROM_Chksum_M),
	sizeof(Memory_M),
	sizeof(Memory_Chksum_M),
	0
};

GUI_Serial *aGUI_Serial;

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
int grun;
/*----------------------------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------------------------*/
void *GUI_Serial_Thread(void *_arg)
{

	aGUI_Serial = (GUI_Serial *)_arg;

	while(grun)
	{
		aGUI_Serial->Import();
		aGUI_Serial->Export();
	}

	pthread_exit(0);

}
/*----------------------------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------------------------*/
void lost_client(int _sig)
{
	aGUI_Serial->closePipe();
}
/*----------------------------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------------------------*/
void GUI_Serial::Start()
{
	Start_Thread(GUI_Serial_Thread, this);
}
/*----------------------------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------------------------*/
GUI_Serial::GUI_Serial()
{

	grun = 1;
	execution_tic 	= 0;
	start_tic 		= 0;
	stop_tic 		= 0;
	message_sync 	= 0;
	command_ready 	= 0;
	command_sent	= 1;
	command_ack 	= 1;
	command_count	= 666;
	byte_count 		= 0;
	serial 			= 0;
	npipe_open 		= false;
	npipe[READ] 	= -1;
	npipe[WRITE] 	= -1;
	lfile 			= NULL;
	gfile 			= NULL;
	robsfile 		= NULL;
	rephemfile 		= NULL;
	packet_count[LAST_M_ID] = 0;
	decoded_packet.tic = decoded_packet.id = decoded_packet.length = 0;
	memset(&packet_count[0], 0x0, (LAST_M_ID+1)*sizeof(int));
	memset(&command_body, 0x0, sizeof(Command_Union));
	memset(&command_header, 0x0, sizeof(CCSDS_Packet_Header));
	memset(&decoded_command, 0x0, sizeof(CCSDS_Decoded_Header));
	memset(&filepath[0], 0x0, 1024*sizeof(char));
	memset(&log_flag[0], 0x0, LAST_M_ID*sizeof(char));
	signal(SIGPIPE, lost_client);

}
/*----------------------------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------------------------*/
GUI_Serial::~GUI_Serial()
{
	if(lfile)
		fclose(lfile);

	if(npipe[READ])
		close(npipe[READ]);

	if(npipe[WRITE])
		close(npipe[WRITE]);

	grun = false;
}
/*----------------------------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------------------------*/
void GUI_Serial::openSerial()
{
	int32 spipe;
	struct termios tty;

	spipe = open("/dev/ttyS0", O_RDWR | O_NOCTTY | O_NONBLOCK);
    if(spipe < 0)
    {
		npipe[READ] = -1;
		npipe[WRITE] = -1;
		npipe_open = false;
    	return;
    }

    memset(&tty, 0x0, sizeof(tty));		//!< Initialize the port settings structure to all zeros
    tty.c_cflag =  B57600 | CS8 | CLOCAL | CREAD | CRTSCTS;	//!< 8N1
    tty.c_iflag = IGNPAR;
    tty.c_oflag = 0;
    tty.c_lflag = 0;
    tty.c_cc[VMIN] = 0;      			//!< 0 means use-vtime
    tty.c_cc[VTIME] = 1;      			//!< Time to wait until exiting read (tenths of a second)

    tcflush(spipe, TCIFLUSH);				//!< Flush old data
    tcsetattr(spipe, TCSANOW, &tty);		//!< Apply new settings
    fcntl(spipe, F_SETFL, FASYNC);
	fcntl(spipe, F_SETFL, O_NONBLOCK);		//!< Nonblocking reads and writes

	/* Alias the serial port */
	npipe[READ] = npipe[WRITE] = spipe;
	npipe_open = true;
}
/*----------------------------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------------------------*/
void GUI_Serial::openPipe()
{
	npipe[READ] = npipe[WRITE] = -1;
	npipe[READ] = open("/tmp/GPS2GUI", O_RDONLY | O_NONBLOCK);
	npipe[WRITE] = open("/tmp/GUI2GPS", O_WRONLY | O_NONBLOCK);

	if((npipe[READ] != -1) && (npipe[WRITE] != -1))
	{
		fcntl(npipe[READ] , F_SETFL, O_NONBLOCK);
		fcntl(npipe[WRITE] , F_SETFL, O_NONBLOCK);
		npipe_open = true;
	}
	else
	{
		close(npipe[READ]);
		close(npipe[WRITE]);
		npipe_open = false;
	}
}
/*----------------------------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------------------------*/
void GUI_Serial::closePipe()
{
	close(npipe[READ]);
	close(npipe[WRITE]);
	npipe_open = false;
}
/*----------------------------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------------------------*/
void GUI_Serial::setPipe(bool _status)
{
	npipe_open = _status;
	if(_status == false)
	{
		close(npipe[READ]);
		close(npipe[WRITE]);
		npipe[READ] = -1;
		npipe[WRITE] = -1;
	}
}
/*----------------------------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------------------------*/
void GUI_Serial::setIO(int32 _serial)
{

	if(npipe_open)
	{
		npipe_open = false;
		close(npipe[READ]);
		close(npipe[WRITE]);
		npipe[READ] = -1;
		npipe[WRITE] = -1;
	}

	serial = _serial;
}
/*----------------------------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------------------------*/
void GUI_Serial::Import()
{

	if(npipe_open == true)
	{
		readGPS();
	}
	else
	{
		if(serial)
			openSerial();
		else
			openPipe();
	}

	execution_tic++;

}
/*----------------------------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------------------------*/
void GUI_Serial::Export()
{

	/* Open the pipe */
	if(npipe_open == true)
		writeGPS();

}
/*----------------------------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------------------------*/
void endian_swap(void *_b, int32 _bytes, int32 _flag)
{

	uint8 *pt8, *pb8;
	uint32 *p32;
	uint32 temp;
	int32 dwords, rem, lcv;

	dwords = _bytes >> 2;
	rem = _bytes - (dwords << 2);

	p32 = (uint32 *)_b;
	pb8 = (uint8 *)_b;
	pt8 = (uint8 *)&temp;
	for(lcv = 0; lcv < dwords; lcv++)
	{
		temp = p32[0];

		if(_flag)
		{
			pb8[0] = pt8[1];
			pb8[1] = pt8[0];
			pb8[2] = pt8[3];
			pb8[3] = pt8[2];
		}
		else
		{
			pb8[0] = pt8[3];
			pb8[1] = pt8[2];
			pb8[2] = pt8[1];
			pb8[3] = pt8[0];
		}
		p32++;
		pb8 += 4;
	}

	temp = *p32;

	switch(rem)
	{
		case 1:
			pb8[0] = pt8[0];
			break;
		case 2:
			pb8[0] = pt8[1];
			pb8[1] = pt8[0];
			break;
		case 3:
			pb8[0] = pt8[1];
			pb8[1] = pt8[0];
			pb8[2] = pt8[2];
			break;
		default:
			break;
	}

}
/*----------------------------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------------------------*/
int GUI_Serial::Read(void *_b, int32 _bytes)
{

	int32 nbytes, bread, k;
	uint8 *buff;

	k = 0; nbytes = 0; bread = 0;
	buff = (uint8 *)_b;

	while((nbytes < _bytes) && grun && npipe_open)
	{
		bread = read(npipe[READ], buff, _bytes - nbytes);

		if(bread > -1)
		{
			nbytes += bread;
			buff += bread;
		}

		if(k++ > 5000)
		{
			closePipe();
			return(0);
		}

		usleep(100);

	}

	//endian_swap(_b, _bytes, _bytes <= 6);

	return(nbytes);

}
/*----------------------------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------------------------*/
int GUI_Serial::Write(void *_b, int32 _bytes)
{

	uint32 bwrote;

	if(npipe_open)
		bwrote = write(npipe[WRITE], _b, _bytes);
	else
		bwrote = 0;

	return(bwrote);

}
/*----------------------------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------------------------*/
void GUI_Serial::readGPS()
{
	uint32 checksumc, checksumr;
	int32 nbytes, bread, k, chan;
	uint8 v;

	Message_Struct *dst = &messages;
	Message_Union *src = &message_body;

	if(message_sync)
	{
		/* Check to make sure the first 4 bytes are "0xAAAAAAAA" */
		byte_count += Read(&syncword, sizeof(uint32));

		if(syncword == 0xAAAAAAAA)
		{
			message_sync++;
		}
		else
		{
			message_sync = 0;
			packet_count[LAST_M_ID]++;
			return;
		}

		/* If so get the packet header */
		byte_count += Read(&packet_header, sizeof(CCSDS_Packet_Header));
		DecodeCCSDSPacketHeader(&decoded_packet, &packet_header);

		/* Catch some failures */
		if(decoded_packet.length < 0 || decoded_packet.length > 512)
		{
			message_sync = 0;
			packet_count[LAST_M_ID]++;
			return;
		}

		/* Check to make sure ID is ok */
		if(decoded_packet.id < FIRST_M_ID || decoded_packet.id > LAST_M_ID)
		{
			message_sync = 0;
			packet_count[LAST_M_ID]++;
			return;
		}

		if((decoded_packet.id == FIRST_PERIODIC_M_ID) || (decoded_packet.id == LAST_PERIODIC_M_ID))
		{
			message_sync = 0;
			packet_count[LAST_M_ID]++;
			return;
		}

		/* Make sure the packet length matches the structure size */
		if(CheckPacket(&decoded_packet) == false)
		{
			message_sync = 0;
			packet_count[LAST_M_ID]++;
			return;
		}

		/* Read in the message */
		Read(src, decoded_packet.length);

		/* Assemble into a complete packet for the checksum */
		memcpy(&packet_body[0], &packet_header, 6);				//!< CCSDS Header
		memcpy(&packet_body[6], src, decoded_packet.length);	//!< Body
		checksumc = adler(&packet_body[0], 6 + decoded_packet.length);

		/* Read in the checksum */
		byte_count += Read(&checksumr, sizeof(uint32));

		/* Now verify the checksum */
		if(checksumc != checksumr)
		{
			message_sync = 0;
			packet_count[LAST_M_ID]++;
			return;
		}

		/* Read in the postword */
		byte_count += Read(&postword, sizeof(uint32));

		if(postword != 0xBBBBBBBB)
		{
			message_sync = 0;
			packet_count[LAST_M_ID]++;
			return;
		}

		Lock();

		/* For the bytes/sec calculation */
		byte_count += decoded_packet.length;

		packet_count[decoded_packet.id]++;

		/* Now copy in the body */
		switch(decoded_packet.id)
		{
			case BOARD_HEALTH_M_ID:
				memcpy(&dst->board_health, &src->board_health, sizeof(Board_Health_M));
				if(log_flag[BOARD_HEALTH_M_ID]) printBoard();
				break;
			case TASK_HEALTH_M_ID:
				memcpy(&dst->task_health, &src->task_health, sizeof(Task_Health_M));
				if(log_flag[TASK_HEALTH_M_ID]) printTask();
				break;
			case CHANNEL_M_ID:
				chan = src->channel.chan;
				if((chan >= 0) && (chan < MAX_CHANNELS))
				{
					memcpy(&dst->channel[chan], &src->channel, sizeof(Channel_M));
					if(log_flag[CHANNEL_M_ID]) printChan(chan);
				}
				else
				{
					message_sync = 0;
					packet_count[LAST_M_ID]++;
				}
				break;
			case SPS_M_ID:
				if(log_flag[LAST_M_ID + 1]) printRinexObs();
				FixDoubles((void *)&src->sps, 13);
				memcpy(&dst->sps, &src->sps, sizeof(SPS_M));
				if(log_flag[SPS_M_ID]) printPVT();
				if(log_flag[LAST_M_ID]) printGoogleEarth();
				break;
			case CLOCK_M_ID:
				FixDoubles((void *)&src->clock, 6);
				memcpy(&dst->clock, &src->clock, sizeof(Clock_M));
				if(log_flag[CLOCK_M_ID]) printClock();
				break;
			case TOT_M_ID:
				FixDoubles((void *)&src->tot, 1);
				memcpy(&dst->tot, &src->tot, sizeof(TOT_M));
				break;
			case PPS_M_ID:
				FixDoubles((void *)&src->pps, 4);
				memcpy(&dst->pps, &src->pps, sizeof(PPS_M));
				if(log_flag[PPS_M_ID]) printPPS();
				break;
			case SV_POSITION_M_ID:
				FixDoubles((void *)&src->sv_position, 12);
				chan = src->sv_position.chan;
				if((chan >= 0) && (chan < MAX_CHANNELS))
				{
					memcpy(&dst->sv_positions[chan], &src->sv_position, sizeof(SV_Position_M));
				}
				else
				{
					message_sync = 0;
					packet_count[LAST_M_ID]++;
				}
				break;
			case EKF_STATE_M_ID:
				FixDoubles((void *)&src->ekf_state, 11);
				memcpy(&dst->ekf_state, &src->ekf_state, sizeof(EKF_State_M));
				if(log_flag[EKF_STATE_M_ID]) printEKFState();
				break;
			case EKF_RESIDUAL_M_ID:
				FixDoubles((void *)&src->ekf_residual, MAX_CHANNELS);
				memcpy(&dst->ekf_residual, &src->ekf_residual, sizeof(EKF_Residual_M));
				if(log_flag[EKF_RESIDUAL_M_ID]) printEKFResidual();
				break;
			case EKF_COVARIANCE_M_ID:
				FixDoubles((void *)&src->ekf_covariance, 10);
				memcpy(&dst->ekf_covariance, &src->ekf_covariance, sizeof(EKF_Covariance_M));
				if(log_flag[EKF_COVARIANCE_M_ID]) printEKFCovariance();
				break;
			case MEASUREMENT_M_ID:
				chan = src->measurement.chan;
				if((chan >= 0) && (chan < MAX_CHANNELS))
				{
					memcpy(&dst->measurements[chan], &src->measurement, sizeof(Measurement_M));
					if(log_flag[MEASUREMENT_M_ID]) printMeas(chan);
				}
				else
				{
					message_sync = 0;
					packet_count[LAST_M_ID]++;
				}
				break;
			case PSEUDORANGE_M_ID:
				FixDoubles((void *)&src->pseudorange, 6);
				chan = src->pseudorange.chan;
				if((chan >= 0) && (chan < MAX_CHANNELS))
				{
					memcpy(&dst->pseudoranges[chan], &src->pseudorange, sizeof(Pseudorange_M));
					if(log_flag[PSEUDORANGE_M_ID]) printPseudo(chan);
				}
				else
				{
					message_sync = 0;
					packet_count[LAST_M_ID]++;
				}
				break;
			case EPHEMERIS_M_ID:
				FixDoubles((void *)&src->ephemeris, 24);
				chan = src->ephemeris.sv;
				if((chan >= 0) && (chan < MAX_SV))
				{
					memcpy(&dst->ephemerides[chan], &src->ephemeris, sizeof(Ephemeris_M));
				}
				else
				{
					message_sync = 0;
					packet_count[LAST_M_ID]++;
				}
				break;
			case ALMANAC_M_ID:
				FixDoubles((void *)&src->almanac, 10);
				chan = src->almanac.sv;
				if((chan >= 0) && (chan < MAX_SV))
				{
					memcpy(&dst->almanacs[chan], &src->almanac, sizeof(Almanac_M));
				}
				else
				{
					message_sync = 0;
					packet_count[LAST_M_ID]++;
				}
				break;
			case EPHEMERIS_STATUS_M_ID:
				memcpy(&dst->ephemeris_status, &src->ephemeris_status, sizeof(Ephemeris_Status_M));
				break;
			case SV_SELECT_STATUS_M_ID:
				memcpy(&dst->sv_select_status, &src->sv_select_status, sizeof(SV_Select_Status_M));
				break;
			case SV_PREDICTION_M_ID:
				FixDoubles((void *)&src->sv_prediction, 8);
				chan = src->sv_prediction.sv;
				memcpy(&dst->sv_predictions[MAX_SV], &src->sv_prediction, sizeof(SV_Prediction_M));
				if((chan >= 0) && (chan < MAX_SV))
				{
					memcpy(&dst->sv_predictions[chan], &src->sv_prediction, sizeof(SV_Prediction_M));
					if(log_flag[SV_PREDICTION_M_ID]) printSVPred(chan);
				}
				else
				{
					message_sync = 0;
					packet_count[LAST_M_ID]++;
				}
				break;
			case COMMAND_ACK_M_ID:
				memcpy(&dst->command_ack, &src->command_ack, sizeof(Command_Ack_M));
				command_ack = 1;
				break;
			default:
				message_sync = 0;
				packet_count[LAST_M_ID]++;
				break;
		}

		Unlock();

	}
	else
	{
		v = 0;
		Read(&v, 1);

		syncword <<= 8;
		syncword += (uint32)v;

		if(syncword == 0xAAAAAAAA)
		{
			message_sync++;

			/* If so get the packet header */
			Read(&packet_header, sizeof(CCSDS_Packet_Header));
			DecodeCCSDSPacketHeader(&decoded_packet, &packet_header);

			if(CheckPacket(&decoded_packet) == true)
			{
				Read(&buff[0], decoded_packet.length);
				Read(&checksumr, sizeof(uint32));
				Read(&postword, sizeof(uint32));
				if(postword != 0xBBBBBBBB)
				{
					message_sync = 0;
					packet_count[LAST_M_ID]++;
					return;
				}
			}
			else
			{
				message_sync = 0;
				packet_count[LAST_M_ID]++;
				return;
			}
		}
	}

}
/*----------------------------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------------------------*/
bool GUI_Serial::CheckPacket(CCSDS_Decoded_Header *_p)
{

	bool val = true;

	if((_p->id >= LAST_M_ID) || (_p->id <= FIRST_M_ID))
	{
		val = false;
	}
	else if(SIZEOF_M[_p->id] != _p->length)
	{
		val = false;
	}

	return(val);
}
/*----------------------------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------------------------*/
void GUI_Serial::FixDoubles(void *_b, int32 _num)
{

	int32 lcv;
	double *buff = (double *)_b;

//	for(lcv = 0; lcv < _num; lcv++)
//		SWAP64(buff[lcv]);
}
/*----------------------------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------------------------*/
void GUI_Serial::writeGPS()
{

	uint8 *sbuff;
	uint32 pre = 0xAAAAAAAA;
	uint32 post = 0xBBBBBBBB;
	uint32 checksum;

	Lock();

//	endian_swap(&command_header, sizeof(CCSDS_Packet_Header), 1);
//	endian_swap(&command_body, decoded_command.length, 0);

	sbuff = &packet_body[0];

	if(command_ready && (command_sent == 0))
	{
		/* Assemble into a complete packet */
		memcpy(sbuff, &pre, 4); 				sbuff += 4;		//!< Prefix
		memcpy(sbuff, &command_header, 6); 		sbuff += 6;		//!< CCSDS Header
		memcpy(sbuff, &command_body, decoded_command.length);	sbuff += decoded_command.length; //!< Payload
		checksum = adler(&packet_body[4], 6 + decoded_command.length);
		memcpy(sbuff, &checksum, 4); 			sbuff += 4;		//!< Checksum
		memcpy(sbuff, &post, 4); 				sbuff += 4;		//!< Postfix

		Write(&packet_body[0], decoded_command.length + 18);
		command_sent = 1;
	}

	Unlock();

}
/*----------------------------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------------------------*/
void GUI_Serial::formCommand(int32 _id, void *_p)
{

	uint32 pend, cmd_count;

	Command_Union *cb;
	CCSDS_Packet_Header *ch;
	CCSDS_Decoded_Header *dc;

	Lock();

	dc = &decoded_command;
	ch = &command_header;
	cb = &command_body;
	memset(cb, 0x0, sizeof(Command_Union));

	switch(_id)
	{
		case NULL_C_ID:
			cb->null.flag = 1;
			cb->null.command_id = _id;
			FormCCSDSPacketHeader(ch, COMMAND_M_ID, 0, sizeof(Null_C), 1, command_tic++);
			break;
		case RESET_ALL_C_ID:
			cb->reset_all.flag = 1;
			cb->reset_all.command_id = _id;
			FormCCSDSPacketHeader(ch, COMMAND_M_ID, 0, sizeof(Reset_All_C), 1, command_tic++);
			break;
		case RESET_PVT_C_ID:
			cb->reset_pvt.flag = 1;
			cb->reset_pvt.command_id = _id;
			FormCCSDSPacketHeader(ch, COMMAND_M_ID, 0, sizeof(Reset_PVT_C), 1, command_tic++);
			break;
		case RESET_EKF_C_ID:
			cb->reset_ekf.flag = 1;
			cb->reset_ekf.command_id = _id;
			FormCCSDSPacketHeader(ch, COMMAND_M_ID, 0, sizeof(Reset_EKF_C), 1, command_tic++);
			break;
		case RESET_CHANNEL_C_ID:
			cb->reset_channel.chan = *(int32 *)_p;
			cb->reset_channel.command_id = _id;
			FormCCSDSPacketHeader(ch, COMMAND_M_ID, 0, sizeof(Reset_Channel_C), 1, command_tic++);
			break;
		case RESET_EPHEMERIS_C_ID:
			cb->reset_ephemeris.sv = *(int32 *)_p;
			cb->reset_all.command_id = _id;
			FormCCSDSPacketHeader(ch, COMMAND_M_ID, 0, sizeof(Reset_Ephemeris_C), 1, command_tic++);
			break;
		case RESET_ALMANAC_C_ID:
			cb->reset_almanac.sv = *(int32 *)_p;
			cb->reset_almanac.command_id = _id;
			FormCCSDSPacketHeader(ch, COMMAND_M_ID, 0, sizeof(Reset_Almanac_C), 1, command_tic++);
			break;
		case RESET_PPS_C_ID:
			cb->reset_pps.flag = 1;
			cb->reset_pps.command_id = _id;
			FormCCSDSPacketHeader(ch, COMMAND_M_ID, 0, sizeof(Reset_PPS_C), 1, command_tic++);
			break;
		case RESET_WATCHDOG_C_ID:
			cb->reset_watchdog.flag = 1;
			cb->reset_watchdog.command_id = _id;
			FormCCSDSPacketHeader(ch, COMMAND_M_ID, 0, sizeof(Reset_Watchdog_C), 1, command_tic++);
			break;
		case GET_EPHEMERIS_C_ID:
			cb->get_ephemeris.sv = *(int32 *)_p;
			cb->get_ephemeris.command_id = _id;
			FormCCSDSPacketHeader(ch, COMMAND_M_ID, 0, sizeof(Get_Ephemeris_C), 1, command_tic++);
			break;
		case GET_ALMANAC_C_ID:
			cb->get_almanac.sv = *(int32 *)_p;
			cb->get_almanac.command_id = _id;
			FormCCSDSPacketHeader(ch, COMMAND_M_ID, 0, sizeof(Get_Almanac_C), 1, command_tic++);
			break;
		case SET_EPHEMERIS_C_ID:
			memcpy(&cb->set_ephemeris, _p, sizeof(Set_Ephemeris_C));
			FixDoubles((void *)&cb->set_ephemeris.ephemeris, 24);
			cb->set_ephemeris.command_id = _id;return
			FormCCSDSPacketHeader(ch, COMMAND_M_ID, 0, sizeof(Set_Ephemeris_C), 1, command_tic++);
			break;
		case SET_ALMANAC_C_ID:
			memcpy(&cb->set_almanac, _p, sizeof(Set_Almanac_C));
			FixDoubles((void *)&cb->set_almanac.almanac, 10);
			cb->set_almanac.command_id = _id;
			FormCCSDSPacketHeader(ch, COMMAND_M_ID, 0, sizeof(Set_Almanac_C), 1, command_tic++);
			break;
		case GET_SV_PREDICTION_C_ID:
			cb->get_sv_prediction.sv = *(int32 *)_p;
			cb->get_sv_prediction.command_id = _id;\
			FormCCSDSPacketHeader(ch, COMMAND_M_ID, 0, sizeof(Get_SV_Prediction_C), 1, command_tic++);
			break;
		case GET_EPHEMERIS_STATUS_C_ID:
			cb->get_ephemeris_status.flag = 1;
			cb->get_ephemeris_status.command_id = _id;
			FormCCSDSPacketHeader(ch, COMMAND_M_ID, 0, sizeof(Get_Ephemeris_Status_C), 1, command_tic++);
			break;
		case GET_SV_SELECT_STATUS_C_ID:
			cb->get_sv_select_status.flag = 1;
			cb->get_sv_select_status.command_id = _id;
			FormCCSDSPacketHeader(ch, COMMAND_M_ID, 0, sizeof(Get_SV_Select_Status_C), 1, command_tic++);
			break;
		case SET_EEPROM_C_ID:
			memcpy(&cb->set_eeprom, _p, sizeof(Set_EEPROM_C));
			cb->set_eeprom.command_id = _id;
			FormCCSDSPacketHeader(ch, COMMAND_M_ID, 0, sizeof(Set_EEPROM_C), 1, command_tic++);
			break;
		default:
			command_ready = 0;
			command_sent = 1;
			command_ack = 1;
			Unlock();
			return;
	}

	command_ready = 1;
	command_sent = 0;
	command_ack = 0;

	DecodeCCSDSPacketHeader(dc, ch);

	Unlock();

}
/*----------------------------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------------------------*/
void GUI_Serial::pendCommand()
{

	int32 pend = 1;

	while(pend)
	{
		Lock();
		pend = command_ack == 0;
		Unlock();
		usleep(10000);
	}

}
/*----------------------------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------------------------*/
int32 GUI_Serial::peekCommand()
{

	int32 pend = 0;

	Lock();
	pend = command_ack == 1;
	Unlock();

	return(pend);

}
/*----------------------------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------------------------*/
void GUI_Serial::setLogFile(const char *_str)
{
	strcpy(filepath, _str);
}
/*----------------------------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------------------------*/
void GUI_Serial::logStart()
{
	char filename[1024];

	if(lfile) fclose(lfile);
	if(gfile) fclose(gfile);
	if(robsfile) fclose(robsfile);
	if(rephemfile) fclose(rephemfile);

	strcpy(filename, filepath);
	strcat(filename,"/gps-sdr.log");
	lfile = fopen(filename, "wt");

	if(log_flag[LAST_M_ID])
	{
		strcpy(filename, filepath);
		strcat(filename,"/gps-sdr.klm");
		gfile = fopen(filename, "wt");
		printGoogleEarthHeader();
	}

	if(log_flag[LAST_M_ID + 1])
	{
		strcpy(filename, filepath);
		strcat(filename,"/gps-sdr.rnx");
		robsfile = fopen(filename, "wt");
		printRinexObsHeader();
	}

	if(log_flag[LAST_M_ID + 2])
	{
		strcpy(filename, filepath);
		strcat(filename,"/gps-sdr.epm");
		rephemfile = fopen(filename, "wt");
	}

	if(lfile)
		logging_on = true;
}
/*----------------------------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------------------------*/
void GUI_Serial::logStop()
{
	if(lfile) fclose(lfile);
	if(gfile) fclose(gfile);
	if(robsfile) fclose(robsfile);
	if(rephemfile) fclose(rephemfile);
	lfile = NULL;
	gfile = NULL;
	robsfile = NULL;
	rephemfile = NULL;
	logging_on = false;
}
/*----------------------------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------------------------*/
void GUI_Serial::logClear()
{
	char filename[1024];

	if(lfile) fclose(lfile);
	if(gfile) fclose(gfile);
	if(robsfile) fclose(robsfile);
	if(rephemfile) fclose(rephemfile);

	strcpy(filename, filepath);
	strcat(filename,"/gps-sdr.log");
	lfile = fopen(filename, "wt");

	if(log_flag[LAST_M_ID])
	{
		strcpy(filename, filepath);
		strcat(filename,"/gps-sdr.klm");
		gfile = fopen(filename, "wt");
	}

	if(log_flag[LAST_M_ID + 1])
	{
		strcpy(filename, filepath);
		strcat(filename,"/gps-sdr.rnx");
		robsfile = fopen(filename, "wt");
	}

	if(log_flag[LAST_M_ID + 2])
	{
		strcpy(filename, filepath);
		strcat(filename,"/gps-sdr.epm");
		rephemfile = fopen(filename, "wt");
	}

	logStop();
}
/*----------------------------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------------------------*/
void GUI_Serial::printPVT()
{
	SPS_M *pNav = &messages.sps; /* Navigation Solution */
	int32 lcv, nsvs;

	if(logging_on && lfile)
	{
		nsvs = 0;
		for(lcv = 0; lcv < MAX_CHANNELS; lcv++)
		{
			if((pNav->nsvs >> lcv) & 0x1)
				nsvs++;
		}

		fprintf(lfile,"%d,%d,%d,%d,%.16e,%.16e,%.16e,%.16e,%.16e,%.16e,%.16e,%.16e\n",
			SPS_M_ID,
			pNav->tic,
			nsvs,
			pNav->converged,
			pNav->time,
			pNav->x,
			pNav->y,
			pNav->z,
			pNav->vx,
			pNav->vy,
			pNav->vz,
			pNav->gdop);
	}
}
/*----------------------------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------------------------*/
void GUI_Serial::printClock()
{
	SPS_M		*pNav		= &messages.sps;		/* Navigation Solution */
	Clock_M		*pClock		= &messages.clock;	/* Clock solution */

	if(logging_on && lfile)
	{
		fprintf(lfile,"%d,%d,%d,%d,%.16e,%.16e,%.16e,%.16e,%.16e,%.16e\n",
		CLOCK_M_ID,
		pClock->tic,
		pClock->state,
		pClock->week,
		pClock->time,
		pClock->bias,
		pClock->rate,
		pClock->time0,
		pClock->time_raw,
		pClock->receiver_time);
	}

}
/*----------------------------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------------------------*/
void GUI_Serial::printEKFState()
{
	EKF_State_M *pEKF = &messages.ekf_state;

	if(logging_on && lfile)
	{
		fprintf(lfile,"%d,%d,%d,%d,%d,%.16e,%.16e,%.16e,%.16e,%.16e,%.16e,%.16e,%.16e,%.16e,%.16e,%.16e\n",
		EKF_STATE_M_ID,
		pEKF->tic,
		pEKF->status,
		pEKF->period,
		pEKF->week,
		pEKF->time,
		pEKF->x,
		pEKF->y,
		pEKF->z,
		pEKF->vx,
		pEKF->vy,
		pEKF->vz,
		pEKF->solar,
		pEKF->drag,
		pEKF->clock_bias,
		pEKF->clock_rate);
	}
}
/*----------------------------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------------------------*/
void GUI_Serial::printEKFCovariance()
{

	EKF_Covariance_M *pEKF = &messages.ekf_covariance;

	if(logging_on && lfile)
	{
		fprintf(lfile,"%d,%d,%.16e,%.16e,%.16e,%.16e,%.16e,%.16e,%.16e,%.16e,%.16e,%.16e\n",
		EKF_COVARIANCE_M_ID,
		pEKF->tic,
		pEKF->x,
		pEKF->y,
		pEKF->z,
		pEKF->vx,
		pEKF->vy,
		pEKF->vz,
		pEKF->solar,
		pEKF->drag,
		pEKF->clock_bias,
		pEKF->clock_rate);
	}

}
/*----------------------------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------------------------*/
void GUI_Serial::printEKFResidual()
{

	int32 lcv;

	EKF_Residual_M *pRes = &messages.ekf_residual;

	if(logging_on && lfile)
	{
		fprintf(lfile,"%d,%d",
			EKF_RESIDUAL_M_ID,
			pRes->tic);

		for(lcv = 0; lcv < MAX_CHANNELS; lcv++)
			fprintf(lfile,",%.16e",pRes->pseudorange_residuals[lcv]);

		for(lcv = 0; lcv < MAX_CHANNELS; lcv++)
			fprintf(lfile,",%d",pRes->status[lcv]);

		fprintf(lfile,"\n");
	}

}
/*----------------------------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------------------------*/
void GUI_Serial::printPPS()
{
	PPS_M		*pPPS		= &messages.pps;		/* Clock solution */
	SPS_M		*pNav		= &messages.sps;		/* Navigation Solution */

	if(logging_on && lfile)
	{
		/* Clock state */
		fprintf(lfile,"%d,%d,%d,%.16e,%.16e,%.16e,%.16e\n",
		PPS_M_ID,
		pPPS->tic,
		pPPS->state,
		pPPS->err,
		pPPS->err_lp,
		pPPS->clock_rate,
		pPPS->feedback);

	}

}
/*----------------------------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------------------------*/
void GUI_Serial::printPseudo(int32 _chan)
{

	Pseudorange_M *pPseudo = &messages.pseudoranges[_chan];
	SPS_M *pNav	= &messages.sps;

	if(logging_on && lfile)
	{
		/* Pseudorange */
		fprintf(lfile,"%d,%d,%d,%d,%.16e,%.16e,%.16e,%.16e,%.16e\n",
			PSEUDORANGE_M_ID,
			pPseudo->tic,
			_chan,
			pPseudo->sv,
			pPseudo->meters,
			pPseudo->meters_rate,
			pPseudo->residual,
			pPseudo->residual_rate,
			pPseudo->uncorrected);
	}
}
/*----------------------------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------------------------*/
void GUI_Serial::printMeas(int32 _chan)
{
	Measurement_M *pMeas = &messages.measurements[_chan];
	SPS_M *pNav = &messages.sps;

	if(logging_on && lfile)
	{
		fprintf(lfile,"%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
			MEASUREMENT_M_ID,
			pMeas->tic,
			_chan,
			pMeas->sv,
			pMeas->_1ms_epoch,
			pMeas->_20ms_epoch,
			pMeas->frac_code_phase,
			pMeas->code_phase,
			pMeas->frac_carrier_phase,
			pMeas->frac_carrier_phase_prev,
			pMeas->carrier_phase,
			pMeas->carrier_phase_prev);
	}
}
/*----------------------------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------------------------*/
void GUI_Serial::printChan(int32 _chan)
{
	SPS_M		*pNav = &messages.sps;
	Channel_M	*pChan = &messages.channel[_chan];

	if(logging_on && lfile)
	{
		fprintf(lfile,"%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
		CHANNEL_M_ID,
		pChan->tic,
		_chan,
		pChan->sv,
		pChan->count,
		pChan->bit_lock,
		pChan->frame_lock,
		pChan->subframe,
		pChan->len,
		pChan->p_avg,
		pChan->cn0,
		pChan->code_nco,
		pChan->carrier_nco);
	}

}
/*----------------------------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------------------------*/
void GUI_Serial::printSVPos(int32 _chan)
{
	SV_Position_M *pSV = &messages.sv_positions[_chan];

	if(logging_on && lfile)
	{
		/* Pseudorange */
		fprintf(lfile,"%d,%d,%d,%d,%.16e,%.16e,%.16e,%.16e,%.16e,%.16e,%.16e,%.16e,%.16e\n",
		SV_POSITION_M_ID,
		pSV->tic,
		_chan,
		pSV->sv,
		pSV->x,
		pSV->y,
		pSV->z,
		pSV->vx,
		pSV->vy,
		pSV->vz,
		pSV->time,
		pSV->clock_bias,
		pSV->frequency_bias);
	}
}
/*----------------------------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------------------------*/
void GUI_Serial::printSVPred(int32 _sv)
{
	SPS_M *pNav = &messages.sps;
	SV_Prediction_M *pSV = &messages.sv_predictions[_sv];

	if(logging_on && lfile)
	{
		/* Pseudorange */
		fprintf(lfile,"%d,%d,%d,%d,%.16e,%.16e,%.16e,%.16e,%.16e,%.16e\n",
		SV_PREDICTION_M_ID,
		pNav->tic,
		_sv,
		pSV->visible,
		pSV->time,
		pSV->delay,
		pSV->doppler,
		pSV->doppler_rate,
		pSV->elev,
		pSV->azim);
	}
}
/*----------------------------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------------------------*/
void GUI_Serial::printTask()
{
	Task_Health_M *pTask = &messages.task_health;
	SPS_M *pNav = &messages.sps;
	int32 lcv;

	if(logging_on && lfile)
	{
		fprintf(lfile,"%d,%d,%d,",
			TASK_HEALTH_M_ID,
			pTask->tic,
			pTask->missed_interrupts);

		for(lcv = 0; lcv < MAX_TASKS; lcv++)
			fprintf(lfile,"%d,",pTask->execution_tic[lcv]);

		for(lcv = 0; lcv < MAX_TASKS; lcv++)
			fprintf(lfile,"%d,",pTask->start_tic[lcv]);

		for(lcv = 0; lcv < MAX_TASKS-1; lcv++)
			fprintf(lfile,"%d,",pTask->stop_tic[lcv]);

		fprintf(lfile,"%d\n",pTask->stop_tic[lcv]);
	}
}
/*----------------------------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------------------------*/
void GUI_Serial::printBoard()
{
	Board_Health_M *pBoard = &messages.board_health;
	SPS_M *pNav = &messages.sps;
	int32 lcv;

	if(logging_on && lfile)
	{
		fprintf(lfile,"%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,",
			BOARD_HEALTH_M_ID,
			pBoard->tic,
			pBoard->dsa0,
			pBoard->dsa1,
			pBoard->dsa2,
			pBoard->dsa3,
			pBoard->ovrflw0,
			pBoard->ovrflw1,
			pBoard->ovrflw2,
			pBoard->ovrflw3);

		for(lcv = 0; lcv < SHU_SIGNALS-1; lcv++)
			fprintf(lfile,"%d,",pBoard->adc_values[lcv]);

		fprintf(lfile,"%d\n",pBoard->adc_values[lcv]);
	}
}
/*----------------------------------------------------------------------------------------------*/



/*----------------------------------------------------------------------------------------------*/
void GUI_Serial::printGoogleEarth()
{

	SPS_M *pNav = &messages.sps;

	if(gfile != NULL)
	{
		if(pNav->converged)
		{
			fseek(gfile, gfile_end, SEEK_SET);
			fprintf(gfile,"%15.9f,%15.9f,%15.9f\n",pNav->longitude*RAD_2_DEG,pNav->latitude*RAD_2_DEG,pNav->altitude);
			gfile_end = ftell(gfile);
			printGoogleEarthFooter();
		}
	}

}
/*----------------------------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------------------------*/
void GUI_Serial::printGoogleEarthHeader()
{

	if(gfile != NULL)
	{
		fprintf(gfile,"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
		fprintf(gfile,"<kml xmlns=\"http://earth.google.com/kml/2.1\">\n");
		fprintf(gfile,"<Document>\n");
		fprintf(gfile,"<name>\n");
		fprintf(gfile,"navigation.kml\n");
		fprintf(gfile,"</name>\n");
		fprintf(gfile,"<Placemark id=\"GPS SDR\">\n");
		fprintf(gfile,"<name>\n");
		fprintf(gfile,"GPS SDR\n");
		fprintf(gfile,"</name>\n");
		fprintf(gfile,"<visibility>\n");
		fprintf(gfile,"1\n");
		fprintf(gfile,"</visibility>\n");
		fprintf(gfile,"<description>\n");
		fprintf(gfile,"<![CDATA[]]>\n");
		fprintf(gfile,"</description>\n");
		fprintf(gfile,"<Style>\n");
		fprintf(gfile,"<LineStyle>\n");
		fprintf(gfile,"<color>\n");
		fprintf(gfile,"#FF0000FF\n");
		fprintf(gfile,"</color>\n");
		fprintf(gfile,"<width>\n");
		fprintf(gfile,"1.00\n");
		fprintf(gfile,"</width>\n");
		fprintf(gfile,"</LineStyle>\n");
		fprintf(gfile,"<PolyStyle>\n");
		fprintf(gfile,"<color>\n");
		fprintf(gfile,"00ffffff\n");
		fprintf(gfile,"</color>\n");
		fprintf(gfile,"</PolyStyle>\n");
		fprintf(gfile,"</Style>\n");
		fprintf(gfile,"<Polygon id=\"poly_plot3\">\n");
		fprintf(gfile,"<extrude>0</extrude>\n");
		fprintf(gfile,"<altitudeMode>\n");
		fprintf(gfile,"relativeToGround\n");
		fprintf(gfile,"</altitudeMode>\n");
		fprintf(gfile,"<outerBoundaryIs>\n");
		fprintf(gfile,"<extrude>0</extrude>\n");
		fprintf(gfile,"<LinearRing>\n");
		fprintf(gfile,"<extrude>0</extrude>\n");
		fprintf(gfile,"<tessellate>0</tessellate>\n");
		fprintf(gfile,"<altitudeMode>\n");
		fprintf(gfile,"absolute\n");
		fprintf(gfile,"</altitudeMode>\n");
		fprintf(gfile,"<coordinates>\n");
		gfile_end = ftell(gfile);
	}
}
/*----------------------------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------------------------*/
void GUI_Serial::printGoogleEarthFooter()
{
	if(gfile != NULL)
	{
		fprintf(gfile,"</coordinates>\n");
		fprintf(gfile,"</LinearRing>\n");
		fprintf(gfile,"</outerBoundaryIs>\n");
		fprintf(gfile,"</Polygon>\n");
		fprintf(gfile,"</Placemark>\n");
		fprintf(gfile,"</Document>\n");
		fprintf(gfile,"</kml>\n");
	}
}
/*----------------------------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------------------------*/
void GUI_Serial::printRinexObsHeader()
{

	if(robsfile != NULL)
	{
		fprintf(robsfile,"     2.10                                                   RINEX VERSION / TYPE\n");
		fprintf(robsfile,"     4    C1    D1    L1    S1                              # / TYPES OF OBSERV\n");
		fprintf(robsfile,"                                                            END OF HEADER\n");
	}

}
/*----------------------------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------------------------*/
void GUI_Serial::printRinexObs()
{

	int32 lcv;
	int32 nsv;
	time_t utcsec;
	Channel_M *pChan;
	Pseudorange_M *pPseudo;
	SPS_M *pNav = &messages.sps;
	Clock_M *pClock	= &messages.clock;
	wxDateTime theTime;

	if(robsfile != NULL)
	{

		/* Get the time into UTC */
		utcsec = floor(pClock->time_raw); //!< Seconds in GPS week
		utcsec += (pClock->week)*SECONDS_IN_WEEK;
		utcsec -= 10; 		 //!< Difference of TAI and UTC
		utcsec += 315964819; //!< 0 Unix time --> GPS time offset
		theTime.Set(utcsec);

		/* Pull out the individual PRN numbers */
		nsv = 0;
		for(lcv = 0; lcv < MAX_CHANNELS; lcv++)
		{
			if(messages.pseudoranges[lcv].sv != NON_EXISTENT_SV)
			{
				nsv++;
			}
		}

		/* Generate the date line */
		fprintf(robsfile,"%3d%3d%3d%3d%3d%12.8f 0%3d",
				theTime.GetYear() % 100,
				theTime.GetMonth()+1,
				theTime.GetDay(),
				theTime.GetHour(),
				theTime.GetMinute(),
				(double)theTime.GetSecond() + fmod(pClock->time_raw, 1.0),
				nsv);

		/* Push the individual PRN numbers */
		for(lcv = 0; lcv < MAX_CHANNELS; lcv++)
		{
			pPseudo = &messages.pseudoranges[lcv];
			if(pPseudo->sv != NON_EXISTENT_SV)
			{
				fprintf(robsfile,"%3d",pPseudo->sv+1);
			}
		}

		/* Newline */
		fprintf(robsfile,"\n");

		/* Loop over all of the channels */
		for(lcv = 0; lcv < MAX_CHANNELS; lcv++)
		{
			pChan = &messages.channel[lcv];
			pPseudo = &messages.pseudoranges[lcv];
			if(pPseudo->sv != NON_EXISTENT_SV)
			{
				fprintf(robsfile," %14.3f %14.3f %14.3f %14.3f\n",
						pPseudo->uncorrected, pPseudo->meters_rate, 0.0, icn0_2_fcn0(pChan->cn0));
			}
		}
	}
}
/*----------------------------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------------------------*/
void GUI_Serial::printRinexEphemHeader()
{
	if(rephemfile != NULL)
	{
		fprintf(rephemfile,"     2              N                                       RINEX VERSION / TYPE\n");
		fprintf(rephemfile,"MATLAB              G HECKLER         23-APR-02             PGM / RUN BY / DATE \n");
		fprintf(rephemfile,"PSEUDO-NAVIGATION DATA GENERATED FROM ALMANAC               COMMENT             \n");
		fprintf(rephemfile,"    0.0000D+00  0.0000D+00 -0.0000D+00  0.0000D+00          ION ALPHA           \n");
		fprintf(rephemfile,"   -0.0000D+00 -0.0000D+00  0.0000D+00 -0.0000D+00          ION BETA            \n");
		fprintf(rephemfile,"    0.000000000000D+00 0.000000000000D+00   0             0 DELTA-UTC: A0,A1,T,W\n");
		fprintf(rephemfile,"    12                                                      LEAP SECONDS        \n");
		fprintf(rephemfile,"                                                            END OF HEADER       \n");
	}
}
/*----------------------------------------------------------------------------------------------*/




/*----------------------------------------------------------------------------------------------*/
void GUI_Serial::printRinexEphem()
{
	if(rephemfile != NULL)
	{
//		/*Generate the date using the TOC week and second*/
//		utilities::timeStruct time;
//		utilities::gps2utc(tocwk+1024,toc,&time);
//
//		/*Write out date and time information*/
//		sprintf(output,"%2d%3d%3d%3d%3d%3d%5.1f %- 19.12E%- 19.12E%- 19.12E\n",
//					PRN,time.year,time.month,time.day,time.hour,time.minute,time.second,af0,af1,af2);
//
//		/*Broadcast Ephemeris Line 1*/
//		fprintf(rephemfile,"   %- 20.12E%- 20.12E%- 20.12E%- 20.12E\n",iode,crs,deltan,m0);
//		fprintf(rephemfile,"   %- 20.12E%- 20.12E%- 20.12E%- 20.12E\n",cuc,ecc,cus,sqrta);
//		fprintf(rephemfile,"   %- 20.12E%- 20.12E%- 20.12E%- 20.12E\n",toe,cic,om0,cis);
//		fprintf(rephemfile,"   %- 20.12E%- 20.12E%- 20.12E%- 20.12E\n",in0,crc,argp,omd);
//		fprintf(rephemfile,"   %- 20.12E%- 20.12E%- 20.12E%- 20.12E\n",idot,(double)codeOnL2,(double)toewk,(double)L2PData);
//		fprintf(rephemfile,"   %- 20.12E%- 20.12E%- 20.12E%- 20.12E\n",zeros,(double)health,tgd,iodc);
//		fprintf(rephemfile,"   %- 20.12E%- 20.12E%- 20.12E%- 20.12E\n",tofXmission,zeros,zeros,zeros);
	}
}
