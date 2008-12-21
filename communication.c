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

/*! @file communication.c
 * @brief TCP/UDP communication implementation
 */

#include "communication.h"

const int NOTIMEOUT = 0xFFFFFFFF;

void fatalerror(char *st)
{
	perror(st);
	exit(-1);
}

int min(int a, int b)
{
	if (a < b)
	{
		return a;
	}
	return b;
}

void mstimer_start(double *starttime)
{
	struct timeval t;
	gettimeofday(&t, NULL);
	*starttime = t.tv_sec + t.tv_usec/1000000.0;
}

int mstimeup(double starttime, int milliseconds)
{
	struct timeval t;
	double stoptime;

	gettimeofday(&t, NULL);
	stoptime = t.tv_sec + t.tv_usec/1000000.0;

	return (stoptime-starttime) >= milliseconds/1000.0;
}

void udp_print_pkt( struct UdpCmdPacket *p)
{
	int i;

	memcpy(&i, &p->data[0],sizeof(i));

	OscLog(DEBUG, "Type      = %#x\n", p->cmd);
	OscLog(DEBUG, "data[0]   = %#x\n", (char)p->data[0]);
	OscLog(DEBUG, "data[1]   = %#x\n", (char)p->data[1]);
	OscLog(DEBUG, "data[2]   = %#x\n", (char)p->data[2]);
	OscLog(DEBUG, "data[3]   = %#x\n", (char)p->data[3]);
	OscLog(DEBUG, "data[0-3] = %#x\n", i);
}


int udp_settxbufsize(int sock, int size)
{
	int retval;
	int is_size;
	socklen_t len=sizeof(size);

	retval = setsockopt(sock, SOL_SOCKET, SO_SNDBUF, (char *)&size,
				(int)sizeof(size));
	if (retval==SOCK_ERROR) fatalerror("setsockopt SO_SNDBUF");
	retval = getsockopt(sock, SOL_SOCKET, SO_SNDBUF, (char *)&is_size, &len);

	if (retval==SOCK_ERROR) fatalerror("getsockopt");
	OscLog(DEBUG, "udp_settxbufsizes(): sendbufsize = %i\n", is_size);

	return size;
}



int udp_setrxbufsize(int sock, int size)
{
	int retval;
	int is_size;
	socklen_t len=sizeof(size);

	retval = setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (char *)&size,
				(int)sizeof(size));
	if (retval==SOCK_ERROR) fatalerror("setsockopt SO_RCVBUF");
	retval = getsockopt(sock, SOL_SOCKET, SO_RCVBUF, (char *)&is_size, &len);

	if (retval==SOCK_ERROR) fatalerror("getsockopt");
	OscLog(DEBUG, "%s: rcvbufsize = %i\n", __func__, is_size);

	return size;
}


int udp_init(int sock, int port)
{
	int retval;
	struct sockaddr_in addr;

	if (sock >= 0)
	{
		fatalerror("udp_init: UDP already initialized");
	}

	sock=socket(PF_INET, SOCK_DGRAM, 0);
	if (sock==SOCK_ERROR)
	{
		fatalerror("udp_init: Could not get socket");
	}

	if (port == UdpCmdPort)
	{
		udp_settxbufsize(sock, BSIZE);		/* use large buffer also for cmd-port !!! */
		udp_setrxbufsize(sock, BSIZE);
	}
	else
	{
		udp_settxbufsize(sock, BSIZE);		/* 1 entire image plus line numbers for data port */
		udp_setrxbufsize(sock, BSIZE);
	}

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);

	retval = bind(sock, (struct sockaddr *)&addr, sizeof(addr));
	if (retval==SOCK_ERROR)
	{
		fatalerror("udp_init: Could not bind socket");
	}
	return (sock);
}


int udp_close(int sock)
{
	if (sock < 0)
	{
		fatalerror("udp_close: Socket not initialized");
	}
	close(sock);
	return 0;
}


int udp_getcmd(struct UdpCmdPacket *pkt, int timeout_ms, int sock, struct sockaddr_in *fromaddr)
{
	struct sockaddr_in FromAddr;
	int    retval;
	fd_set s;
	struct timeval timeout;
	socklen_t    addrlen=sizeof(struct sockaddr_in);

	if (fromaddr==NULL)
	{
		fromaddr = &FromAddr;
	}

	FD_ZERO(&s);
	FD_SET(sock, &s);
	timeout.tv_sec = timeout_ms/1000;
	timeout.tv_usec = (timeout_ms % 1000)*1000;
	retval = select(sock+1, &s, NULL, NULL, &timeout);
	if (retval > 0)
	{
		retval = recvfrom(sock, pkt, BSIZE, 0, (struct sockaddr *)fromaddr, &addrlen);
	}
	else
	{
		return -1;
	}

	OscLog(DEBUG, "%s: Received %i bytes\n", __func__, retval);
	udp_print_pkt(pkt);
	return retval;
}


int tcp_init(int sock, int port)
{
	int retval, on;
	struct sockaddr_in addr;

	if (sock >= 0)
	{
		fatalerror("tcp_init: TCP already initialized");
	}

	sock = socket(PF_INET, SOCK_STREAM, 0);
	if (sock==-1)
	{
		fatalerror("tcp_init: Could not get TCP socket");
	}

	/* Enable address reuse, because Linux sends TCP-FIN and Windows uses TCP-RST, so Windows does not close connection correctly on close(sock) */
	on = 1;
	retval = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
	if (retval==-1)
	{
		OscLog(ERROR, "%s: could not set socket-option SO_REUSEADDR", __func__);
	}

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);

	retval = bind(sock, (struct sockaddr *)&addr, sizeof(addr));
	if (retval==-1)
	{
		fatalerror("tcp_init: could not bind socket");
	}
	return (sock);
}


void tcp_close(int sock)
{
	close(sock);
}


int tcp_getconnection(int sock)
{
	int csock;

	if (listen(sock, 1)==-1)
	{
		fatalerror("could not listen");
	}

	csock = accept(sock, NULL, 0);
	if (csock == -1)
	{
		fatalerror("accept error");
	}

	return csock;
}


int tcp_senddata (int csock, uint8* pbuf, int size)
{
	uint8 *tmpbuf;
	int retval, len;
	int bytessent = 0;

	tmpbuf = pbuf;		/* get base addr. of buffer */
	len = size;			/* get entire size in bytes */
	while (len > 0)
	{
		retval = send(csock, tmpbuf, len, 0);
		if (retval<0)
		{
			perror("tcp_senddata: send error\n");
			return -1;
		}
		len -= retval;				/* remaining length */
		tmpbuf += retval;			/* buffer offset */
		bytessent += retval;		/* total bytes sent */
	}
	return bytessent;
}





