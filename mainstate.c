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

/*! @file mainstate.c
 * @brief Main State machine for rich-view application.
 * 
 * Makes use of Framework HSM module.
 */
#include "rich-view.h"
#include "mainstate.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

const Msg mainStateMsg[] = {
	{ FRAMESEQ_EVT },
	{ FRAMEPAR_EVT },
	{ TRIGGER_EVT },
	{ CMD_GO_IDLE_EVT },
	{ CMD_GO_ACQ_EVT },
	{ CMD_USE_INTERN_TRIGGER_EVT },
	{ CMD_USE_EXTERN_TRIGGER_EVT }
};

OSC_ERR SelfTrigger(void)
{
  OSC_ERR err;
#ifdef HAS_CPLD
  err = OscLgxTriggerImage();
#else
  err = OscGpioTriggerImage();
#endif /* HAS_CPLD */
  if (err != SUCCESS)
    {
      OscLog(ERROR, "%s: Unable to trigger capture (%d)!\n", __func__, err);
    }
  return err;
}


/*********************************************************************//*!
 * @brief Inline function to throw an event to be handled by the statemachine.
 * 
 * @param pHsm Pointer to state machine
 * @param evt Event to be thrown.
 *//*********************************************************************/
static inline void ThrowEvent(struct MainState *pHsm, unsigned int evt)
{
	const Msg *pMsg = &mainStateMsg[evt];
	HsmOnEvent((Hsm*)pHsm, pMsg);
}



OSC_ERR SetConfigRegister(void *pMainState, struct CBP_PARAM *pReg)
{
	OSC_ERR err;
	struct CFG_KEY configKey;
	struct CFG_VAL_STR strCfg;
	struct MainState *pHsm = (struct MainState *)pMainState;
#ifdef HAS_CPLD
	uint8 cpldReg;
	int exposureDelay;
#endif /* HAS_CPLD */
#ifdef UNSUPPORTED
	int i;
#endif /* UNSUPPORTED */

	switch(pReg->id)
	{
	case REG_ID_AQUISITION_MODE:
		if(pReg->val == 0)
		{
			ThrowEvent(pHsm, CMD_GO_IDLE_EVT);
			break;
		}
		if(pReg->val == 1)
		{
			ThrowEvent(pHsm, CMD_GO_ACQ_EVT);
			break;			
		}
		return -EUNSUPPORTED;
	case REG_ID_TRIGGER_MODE:
		if(pReg->val == 0)
		{
			ThrowEvent(pHsm, CMD_USE_INTERN_TRIGGER_EVT);
			break;
		}
		if(pReg->val == 1)
		{
			ThrowEvent(pHsm, CMD_USE_EXTERN_TRIGGER_EVT);
			break;			
		}
		return -EUNSUPPORTED;
	case REG_ID_EXP_TIME:
		/* Apply exposure time and store to configuration. */
		data.exposureTime = pReg->val;  

		/* Apply value */
		err = OscCamSetShutterWidth( data.exposureTime);
		if( err != SUCCESS)
		{
			OscLog(ERROR, "%s: Failed to modify exposure time! (%d)\n", __func__, err);
			break;
		}
		OscLog(INFO, "%s: Exposure time stored and applied to %d us\n", __func__, data.exposureTime);

		/* Store to configuration. */            
		configKey.strSection = NULL;
		configKey.strTag = "EXP";
		sprintf(strCfg.str, "%ld", data.exposureTime);
		err = OscCfgSetStr( data.hConfig,
				    &configKey,
				    strCfg.str);
		err |= OscCfgFlushContent(data.hConfig);
		return err;
#ifdef UNSUPPORTED
		/* This code is not yet ported to the new communication scheme and
		   thus unsupported. */
	case REG_ID_MAC_ADDR:
		/* Store Mac/Ip persistent. Applied on next reboot. */
		for (i=0; i<6; i++)
		{
			macAddr[i] = (uint8)data.cmdpkt.data[i];
		}

		/* Compose strings */
		sprintf(strMac, "%02x:%02x:%02x:%02x:%02x:%02x", 
			macAddr[0], macAddr[1], 
			macAddr[2], macAddr[3], 
			macAddr[4], macAddr[5]);

		OscLog(INFO, "Set MAC addr environment variable: %s (one time programmable)\n",strMac);

		/* Write to persistent u-boot environment ethaddr. */
		pF = fopen("/tmp/mac", "w");
		fprintf(pF, "%s", strMac);
		fflush(pF);
		fclose(pF);
		system("fw_setenv ethaddr `more /tmp/mac`");
	case CmdPerspective:
		/* Apply perspective setting and store to configuration */
		data.perspective = (enum EnOscCamPerspective)data.cmdpkt.data[0];

		/* Apply to sensor */
		err = OscCamSetupPerspective( data.perspective);	

		/* Store to configuration */
		configKey.strSection = NULL;
		configKey.strTag = "PER";
		err = OscCamPerspectiveEnum2Str( data.perspective, strCfg.str);
		err = OscCfgSetStr( data.hConfig, 
				    &configKey, 
				    strCfg.str);    
		err |= OscCfgFlushContent(data.hConfig);
		break;
#endif /* UNSUPPORTED */
		return -EUNSUPPORTED;
#ifdef HAS_CPLD
	case REG_ID_EXP_DELAY:
		/* Apply exposure delay to CPLD. Keep enable bit as currently set. */
		exposureDelay = pReg->val;
		if (exposureDelay > 99)
		{
			OscLog(ERROR, "Invalid exposure delay value (%d). Valid range: 0..99\n", 
			       exposureDelay);
			return -EINVALID_PARAMETER;
		}
		/* Store to data struct */
		data.exposureDelay = exposureDelay;	
	
		/* Apply to CPLD. Preserve current enable bit state. */
		err = OscCpldRget(OSC_LGX_CLKDELAY, &cpldReg);	
		cpldReg = OSC_LGX_CLKDELAY_ENABLE;
		if( cpldReg & OSC_LGX_CLKDELAY_ENABLE)
		{		
			cpldReg = exposureDelay | OSC_LGX_CLKDELAY_ENABLE;	
		}
		else
		{
			cpldReg = exposureDelay;	
		}
			
		err |= OscCpldRset(OSC_LGX_CLKDELAY, cpldReg);
		if( err != SUCCESS)
		{
			OscLog(ERROR, "%s: Failed to apply exposure delay to CPLD!\n", __func__);
			return err;
		}
		OscLog(INFO, "%s: Exposure applied to CPLD: %d fine clocks.\n", __func__, data.exposureDelay);
		return SUCCESS;
	case REG_ID_STORE_CUR_EXP_DELAY:
		/* Read current fine clock position from CPLD. Store this offset in
		 * configuration and apply to CPLD exposure delay. */
		err = OscCpldRget(OSC_LGX_FASTCLKCOUNT, &cpldReg);	 
	 
		exposureDelay = cpldReg;
		/* Value 0 is reserved with the current CPLD version */
		if( 0 == exposureDelay)
		{
			exposureDelay++;
		}
		OscLog(INFO, "%s: Read current fine clock position from CPLD: %d\n", __func__, exposureDelay);
	 
		/* Store exposure delay to configuration. */            
		configKey.strSection = NULL;
		configKey.strTag = "DEL";
		sprintf(strCfg.str, "%d", exposureDelay);
		err = OscCfgSetStr( data.hConfig,
				    &configKey,
				    strCfg.str);        
		err |= OscCfgFlushContent(data.hConfig);
		if( err != SUCCESS)
		{
			OscLog(ERROR, "%s: Failed to store exposure delay to configuration!\n", __func__);
			break;
		}
		OscLog(INFO, "%s: Exposure delay stored to configuration: %d fine clocks.\n", __func__, exposureDelay);
	
		/* Apply delay to CPLD. Preserve the current enable bit state */
		err = OscCpldRget(OSC_LGX_CLKDELAY, &cpldReg);	
		cpldReg = OSC_LGX_CLKDELAY_ENABLE;
		if( cpldReg & OSC_LGX_CLKDELAY_ENABLE)
		{		
			cpldReg = exposureDelay | OSC_LGX_CLKDELAY_ENABLE;	
		}
		else
		{
			cpldReg = exposureDelay;	
		}
			
		err |= OscCpldRset(OSC_LGX_CLKDELAY, cpldReg);
		if( err != SUCCESS)
		{
			OscLog(ERROR, "%s: Failed to apply exposure delay to CPLD!\n", __func__);
			break;
		}
		OscLog(INFO, "%s: Exposure applied to CPLD: %d fine clocks.\n", __func__, exposureDelay);		
	
		break;
#endif /* HAS_CPLD */
	default:
		OscLog(WARN, "%s: Invalid register (%#x)!\n", __func__, pReg->id);
		return -EUNSUPPORTED;

	}

	/* Evaluate the success or failure of the commands that invoked the state machine. */
	if(data.comm.enReqState == REQ_STATE_ACK_PENDING)
	{
		/* Success. */
		return SUCCESS;
	} else if(data.comm.enReqState == REQ_STATE_NACK_PENDING) {
		return -EDEVICE;
	} else {
		OscLog(ERROR, "%s: Change of register %d was not handled by the state machine!\n",
		       __func__, pReg->id);
		return -EDEVICE;
	}
}

Msg const *MainState_top(MainState *me, Msg *msg)
{
	switch (msg->evt)
	{
	case START_EVT:
	  STATE_START(me, &me->idle);
	  return 0;
	}
	return msg;
}


Msg const *MainState_idle(MainState *me, Msg *msg)
{
	switch (msg->evt)
	{
	case ENTRY_EVT:
		OscLog(INFO, "Enter idle mode.\n");
#ifndef HAS_CPLD
		/* Set onboard LED green */
		OscGpioSetTestLed( TRUE);       
		OscGpioSetTestLedColor(FALSE, TRUE); /* R, G*/ 
#endif /* !HAS_CPLD */
		return 0;
	case FRAMESEQ_EVT:
		/* Sleep here for a short while in order not to violate the vertical
		 * blank time of the camera sensor when triggering a new image
		 * right after receiving the old one. This can be removed if some
		 * heavy calculations are done here. */
		usleep(1000);
		return 0;
	case FRAMEPAR_EVT:		
		return 0;
	case CMD_GO_IDLE_EVT:
		data.comm.enReqState = REQ_STATE_ACK_PENDING;
		return 0;
	case CMD_GO_ACQ_EVT:
		if(data.enTriggerMode == TRIG_MODE_INTERNAL)
		{
			STATE_TRAN(me, &me->internal);
		} else if(data.enTriggerMode == TRIG_MODE_EXTERNAL)
		{
			STATE_TRAN(me, &me->external);
		} else {
			OscLog(ERROR, "%d: Invalid trigger mode configured (%d)!\n",
			       __func__, data.enTriggerMode);
			data.comm.enReqState = REQ_STATE_NACK_PENDING;
			return 0;
		}
		data.comm.enReqState = REQ_STATE_ACK_PENDING;
		return 0;
	case CMD_USE_INTERN_TRIGGER_EVT:
		/* Switch state to internal capturing mode.  */
		data.enTriggerMode = TRIG_MODE_INTERNAL;
		data.comm.enReqState = REQ_STATE_ACK_PENDING;
		return 0;
	case CMD_USE_EXTERN_TRIGGER_EVT:
		/* Switch state to external capturing mode.  */
		data.enTriggerMode = TRIG_MODE_EXTERNAL;
		data.comm.enReqState = REQ_STATE_ACK_PENDING;
		return 0;
	}
	return msg;
}

Msg const *MainState_capture(MainState *me, Msg *msg)
{
        OSC_ERR err;
	void *pDummyImg = NULL;
	uint32 imgSize;

	switch (msg->evt)
	{
	case ENTRY_EVT:
		OscLog(INFO, "Enter generic capture mode.\n");
#ifndef HAS_CPLD
		/* Set onboard LED red */
		OscGpioSetTestLed( TRUE);       
		OscGpioSetTestLedColor(TRUE, FALSE); /* R, G*/ 
#endif /* !HAS_CPLD */

		OscLog(INFO, "Setup capture\n");
		err = OscCamSetupCapture( OSC_CAM_MULTI_BUFFER);
		if (err != SUCCESS)
		{
			OscLog(ERROR, "%s: Unable to setup initial capture (%d)!\n", __func__, err);
		}
		return 0;
	case FRAMESEQ_EVT:
		/* Fill out the feed header. */
		data.comm.feedHdr.seqNr++;
		/* We need the uptime in milliseconds. */
		data.comm.feedHdr.timeStamp = (uint32)(OscSupCycToMilliSecs(OscSupCycGet64()));
		
		data.comm.feedHdr.imgWidth = OSC_CAM_MAX_IMAGE_WIDTH;
		data.comm.feedHdr.imgHeight = OSC_CAM_MAX_IMAGE_HEIGHT;
#ifdef TARGET_TYPE_LEANXCAM
		data.comm.feedHdr.pixFmt = V4L2_PIX_FMT_GREY;
		imgSize = data.comm.feedHdr.imgWidth * data.comm.feedHdr.imgHeight * 1;
#endif /* TARGET_TYPE_LEANXCAM */
#ifdef TARGET_TYPE_INDXCAM
		data.comm.feedHdr.pixFmt = V4L2_PIX_FMT_SBGGR8;
		imgSize = data.comm.feedHdr.imgWidth * data.comm.feedHdr.imgHeight * 1;
#endif /* TARGET_TYPE_INDXCAM */
		/* Send the image to the host. */
	  	Comm_SendImage(&data.comm, 
			       data.pCurRawImg, 
			       imgSize,
			       &data.comm.feedHdr);
		return 0;
	case FRAMEPAR_EVT:	
		return 0;
	case CMD_GO_IDLE_EVT:
		/* Read picture until no more capture is active.Always use self-trigg*/
		err = SUCCESS;
		while(err != -ENO_CAPTURE_STARTED)
		{
			SelfTrigger();
			err = OscCamReadPicture(OSC_CAM_MULTI_BUFFER, &pDummyImg, 0, CAMERA_TIMEOUT);
			OscLog(DEBUG, "%s: Removed picture from queue! (%d)\n", __func__, err);
		} 
		STATE_TRAN(me, &me->idle);
		data.comm.enReqState = REQ_STATE_ACK_PENDING;
		return 0;
	case CMD_GO_ACQ_EVT:
		data.comm.enReqState = REQ_STATE_ACK_PENDING;
		return 0;
	case CMD_USE_INTERN_TRIGGER_EVT:
	case CMD_USE_EXTERN_TRIGGER_EVT:
		/* Not supported in acquisition mode. */
		data.comm.enReqState = REQ_STATE_NACK_PENDING;
		return 0;
	case EXIT_EVT:
		/* Read picture until no more capture is active. Always use self-trigg*/
		err = SUCCESS;
		while(err != -ENO_CAPTURE_STARTED)
		{
			SelfTrigger();
			err = OscCamReadPicture(OSC_CAM_MULTI_BUFFER, &pDummyImg, 0, CAMERA_TIMEOUT);
			OscLog(DEBUG, "%s: Removed picture from queue! (%d)\n", __func__, err);
		} 
		return 0;
	}
	return msg;
}

Msg const *MainState_internal(MainState *me, Msg *msg)
{
	switch (msg->evt)
	{
	case ENTRY_EVT:
		OscLog(INFO, "Enter internal capture mode.\n");
		/* Initiate manual triggering. Target dependet. */
		SelfTrigger();
		return 0;
	case TRIGGER_EVT:
		/* Initiate manual triggering. Target dependet. */
		SelfTrigger();
		return 0;
	}
	return msg;
}

Msg const *MainState_external(MainState *me, Msg *msg)
{
	switch (msg->evt)
	{
	case ENTRY_EVT:
		OscLog(INFO, "Enter external capture mode.\n");
#ifdef HAS_CPLD
		/* Enable CPLD counter. */
		OscCpldFset(OSC_LGX_CLKDELAY, OSC_LGX_CLKDELAY_ENABLE, OSC_LGX_CLKDELAY_ENABLE);
#endif
		return 0;

	case EXIT_EVT:
#ifdef HAS_CPLD
		/* Disable CPLD counter. */
		OscCpldFset(OSC_LGX_CLKDELAY, OSC_LGX_CLKDELAY_ENABLE, !OSC_LGX_CLKDELAY_ENABLE);
#endif
		return 0;
	}
	return msg;
}

void MainStateConstruct(MainState *me)
{
	HsmCtor((Hsm *)me, "MainState", (EvtHndlr)MainState_top);
	StateCtor(&me->idle, "idle",
		&((Hsm *)me)->top, (EvtHndlr)MainState_idle);	
	StateCtor(&me->capture, "capture",
		&((Hsm *)me)->top, (EvtHndlr)MainState_capture);   
	StateCtor(&me->internal, "internal",
		&me->capture, (EvtHndlr)MainState_internal);
	StateCtor(&me->external, "external",
		&me->capture, (EvtHndlr)MainState_external);
}

OSC_ERR StateControl( void)
{
	OSC_ERR err;
	MainState mainState;
	void *pCurRawImg = NULL;

	/* Setup main state machine. Start with idle mode. */
	MainStateConstruct(&mainState);
	HsmOnStart((Hsm *)&mainState);

	/*----------- infinite main loop */
	while( TRUE)
	{
		/*----------- wait for captured picture */
		while (TRUE)
		{
		  /*----------- Alternating 	a) check for new connections
		   *                            b) check for commands (and do process) 
		   * 				c) check for available picture */
			err = Comm_AcceptConnections(&data.comm, ACCEPT_CONNS_TIMEOUT);
			if(err != SUCCESS && err != -ETRY_AGAIN)
			{
				OscLog(ERROR, "%s: Error accepting new connections (%d)!\n",
				       __func__, err);
			}

			err = Comm_HandleCommands(&data.comm, &mainState, GET_CMDS_TIMEOUT);
			if(err != SUCCESS && err != -ETRY_AGAIN)
			{
				OscLog(ERROR, "%s: Error handling commands (%d)!\n",
				       __func__, err);
			} else if(err == SUCCESS)
			{
				OscLog(INFO, "Command received.\n");		
			}
			
			 
			err = OscCamReadPicture(OSC_CAM_MULTI_BUFFER, &pCurRawImg, 0, CAMERA_TIMEOUT);
			if ((err != -ETIMEOUT) ||(err != -ENO_CAPTURE_STARTED) )
			{
				/* Anything other than a timeout or no pending capture  means that we should
				* stop trying and analyze the situation. */
				break;
			}
		}		
		if( err == SUCCESS) /* only if breaked due to CamReadPic() */
		{
		    data.pCurRawImg = pCurRawImg;
		    OscLog(DEBUG, "---image available\n");
		}
		else
		{
		    pCurRawImg = NULL;
		}
		
		/*----------- process frame by state engine (pre-setup) Sequentially with next capture */
		if( pCurRawImg)
		{
		    ThrowEvent(&mainState, FRAMESEQ_EVT);
		}
		
		/*----------- prepare next capture */
		if( pCurRawImg)
		{
		    err = OscCamSetupCapture( OSC_CAM_MULTI_BUFFER);
		    if (err != SUCCESS)
			{
				OscLog(ERROR, "%s: Unable to setup capture (%d)!\n", __func__, err);
				break;
			}	
		}	

		/*----------- do self-triggering (if required) */
		ThrowEvent(&mainState, TRIGGER_EVT);
	
	
		/*----------- process frame by state engine (post-setup) Parallel with next capture */
		if( pCurRawImg)
		{
			ThrowEvent(&mainState, FRAMEPAR_EVT);
		}
	
	} /* end while ever */
	
	return SUCCESS;
}


