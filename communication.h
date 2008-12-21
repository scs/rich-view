/*	Target side application for the Rich Viewer.
	Copyright (C) 2008 Supercomputing Systems AG
	
	This library is free software; you can redistribute it and/or modify it
	under the terms of the GNU Lesser General Public License as published by
	the Free Software Foundation; either version 2.1 of the License, or (at
	your option) any later version.
	
	This library is distributed in the hope that it will be useful, but
	WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser
	General Public License for more details.
	
	You should have received a copy of the GNU Lesser General Public License
	along with this library; if not, write to the Free Software Foundation,
	Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

/*! @file communication.h
 * @brief Header file for the TCP/UDP communication.
 */
 
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>

#include "inc/oscar.h"

#ifndef COMMUNICATION_H
#define COMMUNICATION_H

/******************************************************************************
*
*	UDP packet constants
*
******************************************************************************/

/*! @brief UDP command port number */
#define UdpCmdPort		3212		/* command UDP port */
/*! @brief UDP data port number */
#define UdpDataPort		3213		/* data UDP port */

/*! @brief TCP data port number */
#define TcpDataPort		49000		/* value from BM, can be changed */


/*! @brief socket error value */
#define SOCK_ERROR 		-1
/*! @brief UDP buffer size */
#define BSIZE			0x80000		/* UDP buffer size, use a hex number large enough to avoid timing problems ! */
/*	#define BSIZE			((WIDTH+4)*HEIGHT)		UDP buffer size, 1 entire image plus line-numbers, else timing problems ! */


/*! @brief UDP command-packet structure */
struct UdpCmdPacket
{
	char 	cmd;
	char	data[23];
};



/******************************************************************************
*
*	function prototypes
*
******************************************************************************/


/*********************************************************************//*!
 * @brief print fatal error message before exit.
 *
 * @param st The string to print
 *//*********************************************************************/
	void 	fatalerror(char *st);

/*********************************************************************//*!
 * @brief return the smaller of two integers.
 *
 * @param a integer one
 * @param b integer two
 * @return the smaller of the two integers
 *//*********************************************************************/
	int 	min(int a, int b);

/*********************************************************************//*!
 * @brief start a ms timer.
 *
 * @param starttime The start-time variable
 *//*********************************************************************/
	void 	mstimer_start(double *starttime);
/*********************************************************************//*!
 * @brief query, if the time is up.
 *
 * @param starttime The start-time
 * @param milliseconds The time to check in ms
 * @return true, if time is up, false otherwise
 *//*********************************************************************/
	int 	mstimeup(double starttime, int milliseconds);

/*********************************************************************//*!
 * @brief initialize the UDP socket.
 *
 * @param sock The socket to initialize (set to -1 for the first call)
 * @param port The port number
 * @return the socket number to use for data transfer and socket close
 *//*********************************************************************/
	int 	udp_init(int sock, int port);

/*********************************************************************//*!
 * @brief close the UDP socket.
 *
 * @param sock The socket number (from udp_init)
 * @return always 0
 *//*********************************************************************/
	int 	udp_close(int sock);

/*********************************************************************//*!
 * @brief set TX-buffer size.
 *
 * @param sock The socket number (from udp_init)
 * @param size The buffer size in Bytes
 * @return assigned size
 *//*********************************************************************/
	int 	udp_settxbufsize(int sock, int size);

/*********************************************************************//*!
 * @brief set RX-buffer size.
 *
 * @param sock The socket number (from udp_init)
 * @param size The buffer size in bytes
 * @return assigned size
 *//*********************************************************************/
	int 	udp_setrxbufsize(int sock, int size);


/*********************************************************************//*!
 * @brief receive a UDP command packet.
 *
 * @param pkt The buffer to store received packet
 * @param timeout_ms The timeout in ms
 * @param sock The socket number (from udp_init)
 * @param sockaddr_in socket-address structure
 * @return return value from the recfrom function
 *//*********************************************************************/
	int 	udp_getcmd(struct UdpCmdPacket *pkt, int timeout_ms, int sock, struct sockaddr_in *fromaddr);


/*********************************************************************//*!
 * @brief print packet-type and first 4 bytes of a packet for debug purposes.
 *
 * @param p The packet to print
 *//*********************************************************************/
	void	udp_print_pkt(struct UdpCmdPacket *p);

/*********************************************************************//*!
 * @brief initialize the TCP socket.
 *
 * @param sock The socket to initialize (set to -1 for the first call)
 * @param port The port number
 * @return the socket number to use for accepting incoming connections and socket close
 *//*********************************************************************/
	int		tcp_init(int sock, int port);

/*********************************************************************//*!
 * @brief close the TCP socket.
 *
 * @param sock The socket number (from tcp_getconnection / tcp_init)
 *//*********************************************************************/
	void	tcp_close(int sock);

/*********************************************************************//*!
 * @brief start accepting incoming TCP connections.
 *
 * @param sock The socket number (from tcp_init)
 * @return the socket number of the accepted connection to use for data transfer and socket close
 *//*********************************************************************/
	int		tcp_getconnection(int sock);

/*********************************************************************//*!
 * @brief send date to the TCP connection.
 *
 * @param csock The socket number (from tcp_getconnection)
 * @param pbuf The buffer with the data to send
 * @param size The length of the data in bytes
 * @return the socket number of the accepted connection to use for data transfer and socket close
 *//*********************************************************************/
	int		tcp_senddata (int csock, uint8* pbuf, int size);


#endif	/* COMMUNICATION_H */


