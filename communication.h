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
#include <assert.h>

#include "inc/oscar.h"

#ifndef COMMUNICATION_H
#define COMMUNICATION_H

/******************************************************************************
*
*	Host - Target protocol definitions
*
******************************************************************************/
/*! @brief Version of the Common base protocol.*/
#define CBP_VERSION 2008121600
/*! @brief Version of the Feed protocol. */
#define FEED_VERSION 2008121600

/*! @brief TCP port to exchange commands with the host. */
#define TCP_CMD_PORT    49100
/*! @brief TCP image feed port number */
#define TCP_FEED_PORT   49099


/*! @brief socket error value */
#define SOCK_ERROR 		-1
/*! @brief Maximum message size (bytes) */
#define MAX_MSG_BODY_LENGTH	64*1024

/******************************************************************************
*	Message header
******************************************************************************/

/************ Message types **************/
/*! @brief Command to get version information. */
#define MSG_CMD_GET_VER			1
/*! @brief Command to set config registers. */
#define MSG_CMD_SET_CONFIG		10
/*! @brief Command to read out the complete config register file. */
#define MSG_CMD_GET_COMPL_CONFIG	20
/*! @brief Message contains feed data. */
#define MSG_FEED_DATA                   30

/***** Status codes in struct MsgHdr *****/
/*! @brief Status code for a request. */
#define STATUS_REQUEST   	20
/*! @brief Status code for a successful reply. */
#define STATUS_REPLY_SUCC	21
/*! @brief Status code for a failed reply. */
#define STATUS_REPLY_FAIL	22
/*! @brief Status code for a feed message. */
#define STATUS_FEED	        30

/*! @brief Dummy placeholder structure for message types without commands. */
typedef struct _Generic_Params
{
	/*! @brief Message type specific parameter 0. */
	uint32 param0;
	/*! @brief Message type specific parameter 1. */
	uint32 param1;
	/*! @brief Message type specific parameter 2. */
	uint32 param2;
	/*! @brief Message type specific parameter 3. */
	uint32 param3;
} Generic_Params;

/*! @brief MsgHdr parameters for the request message of the GetVersion 
  command. */
typedef Generic_Params GetVersionReq_Params;

/*! @brief MsgHdr parameters for the reply message of the GetVersion 
  command. */
typedef struct _GetVersionReply_Params
{
	/*! @brief Version of the Common Base Protocol. */
	uint32 CBPVersion;
	/*! @brief Version of the Feed Protocol. */
	uint32 FeedProtVersion;
	/*! @brief Target software version (Version of this program. */
	uint32 TargetSWVersion;
	/*! @brief unused */
	uint32 unused3;
} GetVersionReply_Params;

/*! @brief MsgHdr parameters for the request message of the SetConfig
  command. */
typedef Generic_Params SetConfigReq_Params;
/*! @brief MsgHdr parameters for the reply message of the SetConfig 
  command. */
typedef Generic_Params SetConfigReply_Params;

/*! @brief MsgHdr parameters for the request message of the GetCompleteConfig
  command. */
typedef  Generic_Params GetComplConfigReq_Params;
/*! @brief MsgHdr parameters for the reply message of the GetCompleteConfig
  command. */
typedef  Generic_Params GetcomplConfigReply_Params;

/*! @brief MsgHdr parameters for the feed protocol message. */
typedef  Generic_Params FeedData_Params;

/*! @brief The header shared by all messages (commands and feed data). */
struct MsgHdr
{
	/*! @brief Length of the body following this header in bytes. */
	uint32 bodyLength;
	/*! @brief Message type identifier. */
	uint32 msgType;
	/*! @brief This field may be used to identify messages e.g. for a
	  sequence number. */
	uint32 ident;
	/*! @brief Identifies whether the message is a request, reply, etc. */
	uint32 status;

	/*! @brief 4 additional values that depend on the message type. */
	union _msgParams
	{
		GetVersionReq_Params getVerReq;
		GetVersionReply_Params getVerReply;
		
		SetConfigReq_Params setConfReq;
		SetConfigReply_Params setConfReply;

		GetComplConfigReq_Params getComplConfReq;
		GetComplConfigReq_Params getComplConfReply;

		FeedData_Params feedDataParams;
		Generic_Params genericParams;
		
	} msgParams;
};

/******************************************************************************
*	Feed protocol header
******************************************************************************/
/*! @brief Convert a 4 byte character string to an unsigned int. */
#define STR_TO_UINT(str) ((str[0] << 24) | (str[1] << 16) | (str[2] << 8) | (str[3]))

/*! @brief Pixel format descriptor for 8 bit bayer pattern. */
#define V4L2_PIX_FMT_SBGGR8 STR_TO_UINT("BA81")
/*! @brief Pixel format descriptor for 8 bit greyscale images. */
#define V4L2_PIX_FMT_GREY   STR_TO_UINT("GREY")

/*! @brief The header for the image data in the feed protocol. */
struct FeedHdr
{
	/*! @brief Sequence number to detect communication problems. */
	uint32 seqNr;
	/*! @brief Number of milliseconds since start up of the target. */
	uint32 timeStamp;

	/*! @brief Width of the image following this header. */
	uint32 imgWidth;
	/*! @brief Height of the image following this header. */
	uint32 imgHeight;
	/*! @brief 4-character human readable code to identify how the pixels
	  are stored (equivalen to V4L2 pixel format descriptor). */
	uint32 pixFmt;
};

/******************************************************************************
*	Message packet
******************************************************************************/
/*! @brief The message type used in this protocol with maximum size*/
struct CommMsg
{
	/*! @brief Message header. */
	struct MsgHdr hdr;
	/*! @brief Message body. */
	uint8 body[MAX_MSG_BODY_LENGTH];
};

/******************************************************************************
*
*	Register file
*
******************************************************************************/

/*! @brief Represents one configuration parameter as an ID-value pair. */
struct CBP_PARAM
{
	uint32 id;
	uint32 val;
};

/******************************************************************************
*
*	Data container
*
******************************************************************************/


/*! @brief The different states of a pending request. */
enum EnRequestState
{
    REQ_STATE_IDLE,
    REQ_STATE_ACK_PENDING,
    REQ_STATE_NACK_PENDING
};

/*! @brief Contains all communication-relevant variables. */
struct COMM
{
	/*! @brief Socket for incoming UDP command packets */
	int cmdSock;						
	/*! @brief Socket for outgoing TCP data packets */
	int feedSock;						
	/*! @brief Socket for outgoing TCP feed after connection to host */
	int connFeedSock;	
	/*! @brief Socket for command traffic after connection to host. */
	int connCmdSock;

	/*! @brief Buffer for incoming command packets. */
	struct CommMsg cmdMsg;
	/*! @brief The state of the last command request. */
	enum EnRequestState enReqState;

	/*! @brief Temporary buffer containing the initialized message 
	  header of the feed protocol.*/
	struct FeedHdr feedHdr;

	/*! @brief Pointer to the register file of the main program. */
	struct CBP_PARAM *pRegFile;
	/*! @brief Number of entries (registers) in the register file. */
	uint32 nRegs;
};


/******************************************************************************
*
*	function prototypes
*
******************************************************************************/

/*! @brief Build the maximum of two numbers. */
#define MAX(a, b) (a >= b ? a : b)

/*********************************************************************//*!
 * @brief Set a register in the configuration register file and invoke 
 * all actions that need to be done after a write to that specific
 * register.
 * This function is implemented within the main program but accessed by
 * the communication part.
 * 
 * @param pHsm Pointer to the state machine.
 * @param pReg Pointer to the register id/value pair.
 *//*********************************************************************/
OSC_ERR SetConfigRegister(void *pMainState, struct CBP_PARAM *pReg);

/*********************************************************************//*!
 * @brief Accepts incoming connection on the feed and command socket.
 *
 * Sockets that are already connected are ignored.
 *
 * @param pComm Pointer to the communication status structure.
 * @param timeout_ms Timeout of this function in milliseconds
 * @return SUCCESS, -ETIMEOUT, -EDEVICE
 *//*********************************************************************/
OSC_ERR Comm_AcceptConnections(struct COMM *pComm, int timeout_ms);

/*********************************************************************//*!
 * @brief Send a new image over the feed.
 *
 * Attempts to send a new image over the feed. Fills out the message header.
 * The feed header containing the format and size information of the image has
 * to be supplied and filled out by the caller. If the feed socket is not
 * connected, a call to this function returns with -ETRY_AGAIN;
 *
 * @param pComm Pointer to the communication status structure.
 * @param pImg Pointer to the image to be sent.
 * @param imgSize Total length of the image data.
 * @param pFeedHdr Pointer to a filled out feed header for the image data.
 * @return SUCCESS or an appropriate error code.
 *//*********************************************************************/
OSC_ERR Comm_SendImage(struct COMM *pComm, const void* pImg, uint32 imgSize, const struct FeedHdr *pFeedHdr);

/*********************************************************************//*!
 * @brief Check for new commands from the host and handle them.
 *
 * Commands only reading out the register file can be handled locally.
 * Commands that need to invoke the state machine do this with the function
 * SetConfigRegister.
 * @see SetConfigRegister
 *
 * @param pComm Pointer to the communication status structure.
 * @param pHsm Pointer to state machine
 * @param Timeout of this function in milliseconds
 * @return SUCCESS or an appropriate error code.
 *//*********************************************************************/
OSC_ERR Comm_HandleCommands(struct COMM *pComm, void *pHsm, uint32 timeout_ms);

/*********************************************************************//*!
 * @brief Logs the contents of a message to the console.
 *
 * @param pMsg Pointer to the message to be printed.
 *//*********************************************************************/
void Comm_PrintMsg(const struct CommMsg *pMsg);

/*********************************************************************//*!
 * @brief Initialize the command and feed sockets to be ready to accept
 *        connections.
 *
 * @see Comm_DeInit
 * 
 * @param pComm Pointer to the communication status structure.
 * @return SUCCESS or a suitable error code.
 *//*********************************************************************/
OSC_ERR Comm_Init(struct COMM *pComm);

/*********************************************************************//*!
 * @brief Deinitialize the command and feed sockets.
 * @param pComm Pointer to the communication status structure.
 *//*********************************************************************/
void Comm_DeInit(struct COMM *pComm);

#endif	/* COMMUNICATION_H */


