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
#include "version.h"

/*********************************************************************//*!
 * @brief Send a data buffer over the specified socket (blocking).
 * 
 * On send error, this functions sets the supplied socket to 0.
 * 
 * @param pSock Pointer to socket to send the data over.
 * @param pBuf Pointer to the data to be sent.
 * @param len Length of the data buffer to be sent.
 * @return SUCCESS or a suitable error code.
 *//*********************************************************************/
static OSC_ERR Comm_SendData(int* pSock, const void *pBuf, uint32 len);

/*********************************************************************//*!
 * @brief Gets a new message from the command socket.
 *
 * Attempts to get a new command message from the socket. 
 *
 * @param pComm Pointer to the communication status structure.
 * @param timeout_ms Timeout of this function in milliseconds
 * @return Length of the command message on success, 0 on timeout,
 *         negative number on error.
 *//*********************************************************************/
static int Comm_GetCmdMsg(struct COMM *pComm, int timeout_ms);

/*********************************************************************//*!
 * @brief Initialize a socket according to the requirements of the
 *        host-target protocol.
 *
 * Gets a socket number, sets socket options, calls bind and listen on
 * the socket. Incoming connections have to be accepted in a different
 * function.
 * @see Comm_AcceptConnections
 * 
 * @param pSock Pointer to the socket to be created and initialized.
 * @param port Port number the socket should be bound to.
 * @return SUCCESS or an appropriate error code.
 *//*********************************************************************/
static OSC_ERR Comm_InitSocket(int *pSock, int port);

/*********************************************************************//*!
 * @brief Sends a reply to a received command.
 *
 * If the socket is not connected, a call to this function with -ETRY_AGAIN
 *
 * @param pComm Pointer to the communication status structure.
 * @return SUCCESS or a suitable error code.
 *//*********************************************************************/
static OSC_ERR Comm_SendReply(struct COMM *pComm);




void Comm_PrintMsg(const struct CommMsg *pMsg)
{
	int i;
	const struct MsgHdr *pHdr = &pMsg->hdr;

	OscLog(DEBUG, "Msg Start.\n");

	OscLog(DEBUG, "Body Len  = %d\n", pHdr->bodyLength);
	OscLog(DEBUG, "Type      = %#x\n", pHdr->msgType);
	OscLog(DEBUG, "Ident     = %#x\n", pHdr->ident);
	OscLog(DEBUG, "status    = %#x\n", pHdr->status);
	OscLog(DEBUG, "param0    = %#x\n", pHdr->msgParams.genericParams.param0);
	OscLog(DEBUG, "param1    = %#x\n", pHdr->msgParams.genericParams.param1);
	OscLog(DEBUG, "param2    = %#x\n", pHdr->msgParams.genericParams.param2);
	OscLog(DEBUG, "param3    = %#x\n", pHdr->msgParams.genericParams.param3);

	OscLog(DEBUG, "\nData:\n");
	for(i = 0; i < pHdr->bodyLength; i++)
	{
		OscLog(DEBUG, "%#x ", pMsg->body[i]);
		if(i % 32 == 31)
		{
			OscLog(DEBUG, "\n");
		}
	}
	OscLog(DEBUG, "\nMsg End.\n", i);
}


OSC_ERR Comm_AcceptConnections(struct COMM *pComm, int timeout_ms)
{
  int retval;
  fd_set s;
  struct timeval timeout;

  if(pComm->connCmdSock > 0 && pComm->connFeedSock > 0)
  {
	  /* Connection on both sockets already established. */
	  return 0;
  }

  FD_ZERO(&s);

  if(pComm->connCmdSock <= 0)
  {
	  FD_SET(pComm->cmdSock, &s);
  }

  if(pComm->connFeedSock <= 0)
  {
	  FD_SET(pComm->feedSock, &s);
  }

  timeout.tv_sec = timeout_ms/1000;
  timeout.tv_usec = (timeout_ms % 1000)*1000;

  retval = select(MAX(pComm->cmdSock, pComm->feedSock)+1, /* Highest socket number + 1 */
		  &s,   /* File descriptor set to monitor for reading and new connections. */
		  NULL, /* File descriptor set to monitor for writing. */
		  NULL, /* File descriptor set to monitor for exceptions. */
		  &timeout);
  if(retval > 0)
  {
	  /* Success. We have something new. */
	  if(FD_ISSET(pComm->cmdSock, &s))
	  {
		  pComm->connCmdSock = accept(pComm->cmdSock, NULL, 0);
		  if(pComm->connCmdSock < 0)
		  {
			  OscLog(ERROR, "%s: Command socket accept error (%s)!\n",
				 __func__, strerror(errno));
			  return -EDEVICE;
		  }
		  OscLog(INFO, "%s: Command socket connected.\n", __func__);
	  }

	  if(FD_ISSET(pComm->feedSock, &s))
	  {
		  pComm->connFeedSock = accept(pComm->feedSock, NULL, 0);
		  if(pComm->connFeedSock < 0)
		  {
			  OscLog(ERROR, "%s: Feed socket accept error (%s)!\n",
				 __func__, strerror(errno));
			  return -EDEVICE;
		  }
		  OscLog(INFO, "%s: Feed socket connected.\n", __func__);
	  }
	  return SUCCESS;
  } else if(retval < 0) {
	  OscLog(ERROR, "%s: Select failed (%s)!\n", __func__, strerror(errno));
	  return -EDEVICE;
  } else {
	  /* Timeout */
	  return -ETIMEOUT;
  }
		  
		  
}

static int Comm_GetCmdMsg(struct COMM *pComm, int timeout_ms)
{
  int retval;
  fd_set s;
  struct timeval timeout;

  if(pComm->connCmdSock <= 0)
  {
	  OscLog(DEBUG, "%s: Socket not connected.\n", __func__);
	  return 0;
  }

  FD_ZERO(&s);
  FD_SET(pComm->connCmdSock, &s);

  timeout.tv_sec = timeout_ms/1000;
  timeout.tv_usec = (timeout_ms % 1000)*1000;

  retval = select(pComm->connCmdSock + 1, /* Highest socket number + 1 */
		  &s,   /* File descriptor set to monitor for reading and new connections. */
		  NULL, /* File descriptor set to monitor for writing. */
		  NULL, /* File descriptor set to monitor for exceptions. */
		  &timeout);
  if(retval > 0)
  {
	  /* Success. We have a new command. */
	  retval = recv(pComm->connCmdSock,
			&pComm->cmdMsg,
			sizeof(struct CommMsg),
			0);
	  return retval;
  } else if(retval < 0) {
	  OscLog(ERROR, "%s: Select failed (%s)!\n", __func__, strerror(errno));
	  return -1;
  } else {
	  /* Timeout */
	  return 0;
  }
}

static OSC_ERR Comm_SendReply(struct COMM *pComm)
{
	if(pComm->connCmdSock <= 0)
	{
		OscLog(DEBUG, "%s: Socket not connected.\n", __func__);
		return -ETRY_AGAIN;
	}


	/* Send reply message. */
	return Comm_SendData(&pComm->connCmdSock, 
			     &pComm->cmdMsg, 
			     sizeof(struct MsgHdr) + pComm->cmdMsg.hdr.bodyLength);
}

OSC_ERR Comm_HandleCommands(struct COMM *pComm, void *pHsm, uint32 timeout_ms)
{
	OSC_ERR err;
	int bytesReceived;
	struct MsgHdr *pHdr;
	int reg;
	struct CBP_PARAM* pParam;

	bytesReceived = Comm_GetCmdMsg(pComm, timeout_ms);
	if(bytesReceived == 0)
	{
		return -ETIMEOUT;
	} else if(bytesReceived < 0)
	{
		return -EDEVICE;
	}

	pHdr = &pComm->cmdMsg.hdr;
	switch(pHdr->msgType)
	{
	case MSG_CMD_GET_VER:
		/* Can be handled without invoking the state machine. */
		pHdr->msgParams.getVerReply.CBPVersion = CBP_VERSION;
		pHdr->msgParams.getVerReply.FeedProtVersion = FEED_VERSION;
		pHdr->msgParams.getVerReply.TargetSWVersion = 
			VERSION_MAJOR << 16 | VERSION_MINOR << 8 | VERSION_PATCH;

		pHdr->bodyLength = 0;
		pHdr->status = STATUS_REPLY_SUCC;

		return Comm_SendReply(pComm);
	case MSG_CMD_GET_COMPL_CONFIG:
		/* Can be handled without invoking the state machine. */
		pHdr->bodyLength = pComm->nRegs * sizeof(struct CBP_PARAM);

		assert(pHdr->bodyLength <= MAX_MSG_BODY_LENGTH);
		memcpy(pComm->cmdMsg.body, pComm->pRegFile, pHdr->bodyLength);

		pHdr->status = STATUS_REPLY_SUCC;

		return Comm_SendReply(pComm);
	case MSG_CMD_SET_CONFIG:
		/* Invoke the state machine for all assigned config registers.
		   Do not change the contents of the register file here, this will
		   be done by the state machine. */
		pParam = (struct CBP_PARAM*)&pComm->cmdMsg;
		for(reg = 0; reg < pComm->nRegs; reg++)
		{
			pComm->enReqState = REQ_STATE_IDLE;
			err = SetConfigRegister(pHsm, pParam);
			pParam++;

			if(err != SUCCESS)
			{
				goto set_config_fail;
			}
		}					
		pHdr->status = STATUS_REPLY_SUCC;
		return Comm_SendReply(pComm);

set_config_fail:  
		pHdr->status = STATUS_REPLY_FAIL;
		return Comm_SendReply(pComm);
	default:
		OscLog(ERROR, "%s: Unsupported message type (%#x) received!\n",
		       __func__);
	}
	return SUCCESS;
}


OSC_ERR Comm_SendImage(struct COMM *pComm, const void* pImg, uint32 imgSize, const struct FeedHdr *pFeedHdr)
{
	OSC_ERR err;
	struct MsgHdr msgHdr;

	if(pComm->connFeedSock <= 0)
	{
		OscLog(DEBUG, "%s: Socket not connected.\n", __func__);
		return -ETRY_AGAIN;
	}

	msgHdr.bodyLength = sizeof(struct FeedHdr) + imgSize;
	msgHdr.msgType = MSG_FEED_DATA;
	msgHdr.ident = 0;
	msgHdr.status = STATUS_FEED;
	
	memset(&msgHdr.msgParams.feedDataParams, 0, sizeof(msgHdr.msgParams.feedDataParams));

	/* Send message header. */
	err = Comm_SendData(&pComm->connFeedSock, &msgHdr, sizeof(struct MsgHdr));
	if(err != SUCCESS)
	{
		return err;
	}

	/* Send feed header. */
	Comm_SendData(&pComm->connFeedSock, pFeedHdr, sizeof(struct FeedHdr));
	if(err != SUCCESS)
	{
		return err;
	}

	/* Send image data */
	Comm_SendData(&pComm->connFeedSock, pImg, imgSize);
	if(err != SUCCESS)
	{
		return err;
	}

	return SUCCESS;
}

static OSC_ERR Comm_SendData(int *pSock, const void *pBuf, uint32 len)
{
	int retval;
	uint8 *pTemp = (uint8*)pBuf;
	uint32 bytesToSend = len;

	while(bytesToSend > 0)
	{
		retval = send(*pSock, pTemp, bytesToSend, 0);
		if(retval < 0)
		{
			OscLog(ERROR, "%s: Send error (%s)!\n", 
			       __func__, strerror(errno));
			*pSock = 0;
			return -EDEVICE;
		}
		bytesToSend -= retval;
		pTemp += retval;
	}
	return SUCCESS;
}

static OSC_ERR Comm_InitSocket(int *pSock, int port)
{
	int sock, retval, on;
	struct sockaddr_in addr;

	if(*pSock > 0)
	{
		return -EALREADY_INITIALIZED;
	}

	/* Initialize command socket. */
	sock = socket(PF_INET, SOCK_STREAM, 0);
	if (sock < 0)
	{
		OscLog(ERROR, "%s: Could not get socket!\n", __func__);
		return -EDEVICE;
	}	

	/* Enable address reuse, because Linux sends TCP-FIN and Windows 
	   uses TCP-RST, so Windows does not close connection correctly 
	   on close(sock) */
	on = 1;
	retval = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
	if (retval==-1)
	{
		OscLog(ERROR, "%s: could not set socket-option SO_REUSEADDR", __func__);
		return -EDEVICE;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);

	retval = bind(sock, (struct sockaddr *)&addr, sizeof(addr));
	if (retval==-1)
	{
		OscLog(ERROR, "%s: could not bind socket (%s)!", 
		       __func__, strerror(errno));
		return -EDEVICE;
	}

	retval = listen(sock, 1);
	if(retval == -1)
	  {
	    OscLog(ERROR, "%s: Unable to listen to socket (%s)!\n",
		   __func__, strerror(errno));	    
	    return -EDEVICE;
	  }

	*pSock = sock;

	return SUCCESS;
}

OSC_ERR Comm_Init(struct COMM *pComm)
{
	OSC_ERR err;

	if (pComm->cmdSock > 0 || pComm->feedSock > 0)
	{
		return -EALREADY_INITIALIZED;
	}


	/* Initialize command socket. */
	err = Comm_InitSocket(&pComm->cmdSock, TCP_CMD_PORT);
	if(err != SUCCESS && err != -EALREADY_INITIALIZED)
	{
		return err;
	}

	/* Initialize feed socket. */
	err = Comm_InitSocket(&pComm->feedSock, TCP_FEED_PORT);
	if(err != SUCCESS && err != -EALREADY_INITIALIZED)
	{
		Comm_DeInit(pComm);
		return err;
	}

	return SUCCESS;
}


void Comm_DeInit(struct COMM *pComm)
{
	if(pComm->connCmdSock > 0)
	{
		close(pComm->connCmdSock);
		pComm->connCmdSock = -1;
	}
	if(pComm->connFeedSock > 0)
	{
		close(pComm->connFeedSock);
		pComm->connFeedSock = -1;
	}
	if(pComm->cmdSock > 0)
	{
		close(pComm->cmdSock);
		pComm->cmdSock = -1;
	}
	if(pComm->feedSock > 0)
	{
		close(pComm->feedSock);
		pComm->feedSock = -1;
	}
}






