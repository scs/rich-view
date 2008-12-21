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
	{ MODE_IDLE_EVT },
	{ MODE_RUN_INTERN_EVT },
	{ MODE_RUN_EXTERN_EVT },
	{ APPLY_PARAMETER_EVT }
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


OSC_ERR SendImage(uint8 *pImg)
{
	int statusTcp;
	statusTcp = tcp_senddata (data.connsock, pImg, IMAGE_AERA);

	if( statusTcp == IMAGE_AERA)
	{
		OscLog(DEBUG, "image sent\n");
		return SUCCESS;
	}

	OscLog(ERROR, "Failed to send image to host. (byte count = %d)\n", statusTcp);
	if (statusTcp < 0)         
	{
    	/* Nothing sent. Try to reconnect. */
		OscLog(INFO, "Waiting for new connection to GUI ... ");
		data.connsock = tcp_getconnection(data.datasock);
		OscLog(INFO, "DONE\n");      
	}
	return -EDEVICE;
}


OSC_ERR ApplyParameters(void)
{
  OSC_ERR err;
#ifdef HAS_CPLD
  uint8 cpldReg;
  int exposureDelay;
#endif /* HAS_CPLD */
  struct CFG_KEY configKey;
  struct CFG_VAL_STR strCfg;
  uint8 macAddr[6];
  uint8 ipAddr[4];
  char strIp[20], strMac[20];
  FILE *pF;
  int i;


  switch (data.cmdpkt.cmd)
    {
    case CmdBrightness:
      {
	/* Apply exposure time and store to configuration. */
	data.exposureTime = 34 * (((uint8)data.cmdpkt.data[0] << 8) | ((uint8)data.cmdpkt.data[1]));  

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
	break;
      }
#ifdef HAS_CPLD
    case CmdDelay:
      {
	/* Apply exposure delay to CPLD. Keep enable bit as currently set. */
	exposureDelay = (uint8)data.cmdpkt.data[0];
	if (exposureDelay > 99)
	  {
	    OscLog(ERROR, "Invalid exposure delay value (%d). Valid range: 0..99\n", 
		   exposureDelay);
	    break;
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
	    break;
	  }
	OscLog(INFO, "%s: Exposure applied to CPLD: %d fine clocks.\n", __func__, data.exposureDelay);
	break;
      }
#endif /* HAS_CPLD */

#ifdef HAS_CPLD
	case CmdStoreDelay:
	{ 
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
      }      
#endif /* HAS_CPLD */

    case CmdPerspective:
      {
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
      }

    case CmdMacAddr:
      {
	/* Store Mac/Ip persisten. Applied on next reboot. */
	for (i=0; i<6; i++)
	  {
	    macAddr[i] = (uint8)data.cmdpkt.data[i];
	  }
	for (i=0; i<4; i++)
	  {
	    ipAddr[i] = (uint8)data.cmdpkt.data[i+6];
	  }

	/* Compose strings */
	sprintf(strMac, "%02x:%02x:%02x:%02x:%02x:%02x", 
		macAddr[0], macAddr[1], 
		macAddr[2], macAddr[3], 
		macAddr[4], macAddr[5]);
	sprintf(strIp, "%d.%d.%d.%d", 
		ipAddr[0], ipAddr[1], ipAddr[2], ipAddr[3]);

	OscLog(INFO, "Set IP addr environment variable: %s\n",strIp);
	OscLog(INFO, "Set MAC addr environment variable: %s (one time programmable)\n",strMac);

	/* Write to persistent u-boot environment ipaddr. */
	pF = fopen("/tmp/ip", "w");
	fprintf(pF, "%s", strIp);
	fflush(pF);
	fclose(pF);
	system("fw_setenv ipaddr `more /tmp/ip`");

	/* Write to persistent u-boot environment ethaddr. */
	pF = fopen("/tmp/mac", "w");
	fprintf(pF, "%s", strMac);
	fflush(pF);
	fclose(pF);
	system("fw_setenv ethaddr `more /tmp/mac`");
	break;	
      }

    } /* switch */
  
  return SUCCESS;
}


/*********************************************************************//*!
 * @brief Inline function to throw an event to be handled by the statemachine.
 * 
 * @param pHsm Pointer to state machine
 * @param evt Event to be thrown.
 *//*********************************************************************/
inline void ThrowEvent(struct MainState *pHsm, unsigned int evt)
{
	const Msg *pMsg = &mainStateMsg[evt];
	HsmOnEvent((Hsm*)pHsm, pMsg);
}


Msg const *MainState_top(MainState *me, Msg *msg)
{
	switch (msg->evt)
	{
	case START_EVT:
	  STATE_START(me, &me->idle);
	  return 0;
	case APPLY_PARAMETER_EVT:
	  ApplyParameters();
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
	case MODE_RUN_INTERN_EVT:
	  /* Switch state to internal capturing mode.  */
	  STATE_TRAN(me, &me->internal);
	  return 0;
	case MODE_RUN_EXTERN_EVT:
	  /* Switch state to external capturing mode.  */
	  STATE_TRAN(me, &me->external);
	  return 0;
	case MODE_IDLE_EVT:
	  return 0;
	}
	return msg;
}

Msg const *MainState_capture(MainState *me, Msg *msg)
{
        OSC_ERR err;
	void *pDummyImg = NULL;

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
		/* Process the image. */
	  	SendImage(data.pCurRawImg);
		return 0;
	case FRAMEPAR_EVT:	
		return 0;
	case MODE_IDLE_EVT:
	  /* Read picture until no more capture is active.Always use self-trigg*/
	  err = SUCCESS;
	  while(err != -ENO_CAPTURE_STARTED)
	    {
	      SelfTrigger();
	      err = OscCamReadPicture(OSC_CAM_MULTI_BUFFER, &pDummyImg, 0, CAMERA_TIMEOUT);
	      OscLog(DEBUG, "%s: Removed picture from queue! (%d)\n", __func__, err);
	    } 
	  STATE_TRAN(me, &me->idle);
	  return 0;
	case EXIT_EVT:
	  /* Read picture until no more capture is active.Always use self-trigg*/
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
	case MODE_RUN_INTERN_EVT:
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
	case MODE_RUN_EXTERN_EVT:
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
	int statusUdp;
	struct sockaddr_in HostAddr;
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
		  /*----------- Alternating: 	a) check for UDP command (and do process) 
		   * 							b) check for available picture */
			statusUdp = udp_getcmd(&data.cmdpkt, 1, data.cmdsock, &HostAddr);
			if( statusUdp > 0)
			{
				OscLog(INFO, "UDP cmd received.\n");		
			}
			
			 
			if( statusUdp > 0)
			{		      
				switch(data.cmdpkt.cmd)
				{
				case CmdLiveMode:
				{
					ThrowEvent(&mainState, MODE_IDLE_EVT); 
				    ThrowEvent(&mainState, MODE_RUN_INTERN_EVT);
				    break;
				}
				case CmdCalibMode:
				{
					ThrowEvent(&mainState, MODE_IDLE_EVT);
#ifdef HAS_CPLD 
				    ThrowEvent(&mainState, MODE_RUN_EXTERN_EVT);
#else
				    ThrowEvent(&mainState, MODE_RUN_INTERN_EVT);
#endif /* HAS_CPLD */
				    break;
				}
				case CmdStopCmd:
				{
				    ThrowEvent(&mainState, MODE_IDLE_EVT); 
				    break;
				}
				case CmdExit:
				{
				    ThrowEvent(&mainState, MODE_IDLE_EVT); 
					Terminate();
				    break;
				}				
				default:
					ThrowEvent(&mainState, APPLY_PARAMETER_EVT);
				}
			} /* statusUdp */


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


