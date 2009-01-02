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

/*! @file main.c
 * @brief Main file of the Rich-View application. Mainly contains initialization
 * code.
 */

#include "rich-view.h"
#include <string.h>
#include <sched.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>

/*! @brief The "configuration register file" of this program. */
struct CBP_PARAM regfile[] =
{
	{REG_ID_AQUISITION_MODE, 0}, /* Mode  
					   0: Idle mode
					   1: Acquisition mode.*/
	{REG_ID_TRIGGER_MODE, 0},    /* Trigger mode
					0: Internal triggering
					1: External triggering */
	{REG_ID_EXP_TIME, 15000},    /* Exposure time in us. */
	{REG_ID_MAC_ADDR, 0},        /* MAC address. */
	{REG_ID_EXP_DELAY, 1}        /* Exposure delay (indXcam only) */
};
       
/*! @brief This stores all variables needed by the algorithm. */
struct DATA data;

/*! @brief The framework module dependencies of this application. */
struct OSC_DEPENDENCY deps[] = {
        {"log", OscLogCreate, OscLogDestroy},
        {"sup", OscSupCreate, OscSupDestroy},
        {"gpio", OscGpioCreate, OscGpioDestroy},
#ifdef HAS_CPLD	
        {"lgx", OscLgxCreate, OscLgxDestroy},
#endif /* HAS_CPLD */	        
        {"cam", OscCamCreate, OscCamDestroy},
        {"cfg", OscCfgCreate, OscCfgDestroy}        
};

OSC_ERR GetVersionNumber(
	char *hMajor, 
	char *hMinor, 
	char *hPatch)
{
	*hMajor 	= VERSION_MAJOR;
	*hMinor 	= VERSION_MINOR;
	*hPatch 	= VERSION_PATCH;
	return SUCCESS;
}

OSC_ERR GetVersionString( char *hVersion)
{  
  	sprintf(hVersion, "v%d.%d", VERSION_MAJOR, VERSION_MINOR);
   	if(VERSION_PATCH)
   	{    	
   		sprintf(hVersion, "%s-p%d", hVersion, VERSION_PATCH);
   	}
	return SUCCESS;
}

/*********************************************************************//*!
 * @brief Initialize everything so the application is fully operable
 * after a call to this function.
 * 
 * @return SUCCESS or an appropriate error code.
 *//*********************************************************************/
static OSC_ERR init(const int argc, const char * argv[])
{
    OSC_ERR err = SUCCESS;
    uint8 multiBufferIds[2] = {0, 1};
    char strVersion[15]; 
    struct CFG_KEY configKey;
    struct CFG_VAL_STR strCfg;
#ifdef HAS_CPLD
    uint16 exposureDelay;
#endif /* HAS_CPLD */	
    memset(&data, 0, sizeof(struct DATA));
	
    /* Print software version */
    GetVersionString( strVersion); 
	fprintf(stderr, "Software rich-view version: %s\n", strVersion); 	
	
    /******* Create the framework **********/
    err = OscCreate(&data.hFramework);
    if (err < 0)
    {
    	fprintf(stderr, "%s: Unable to create framework.\n", __func__);
    	return err;
    }
	
    /******* Load the framework module dependencies. **********/
    err = OscLoadDependencies(data.hFramework, deps, sizeof(deps)/sizeof(struct OSC_DEPENDENCY));
    
    if (err != SUCCESS)
    {
    	fprintf(stderr, "%s: ERROR: Unable to load dependencies! (%d)\n", __func__, err);
    	goto dep_err;
    }

    /* Set logging levels */
    OscLogSetConsoleLogLevel(INFO);
    OscLogSetFileLogLevel(WARN);

    /* Print framework version */
    OscGetVersionString( strVersion);    
    OscLog(INFO, "Oscar framework version: %s\n", strVersion);

	/* Disable watchdog (probably activated from previous application) */
	OscSupWdtInit();
	OscSupWdtClose();
	
	/* Set LED to green, util the idle state is entered */
	OscGpioSetTestLed( TRUE);       
	OscGpioSetTestLedColor(FALSE, TRUE); /* R, G*/ 	

    /* Register configuration file */
    err = OscCfgRegisterFile(&data.hConfig, CONFIG_FILE_NAME, CONFIG_FILE_SIZE);
    if (err != SUCCESS)
    {
    	OscLog(ERROR, "Cannot access config file.\n");
    	goto cfg_err;
    } 
    

    /* Get perspective setting from config file- */
    configKey.strSection = NULL;
    configKey.strTag = "PER";
   
    strcpy( strCfg.str, "");    
    err = OscCfgGetStr( data.hConfig, 
            &configKey, 
            &strCfg);    

    err |= OscCamPerspectiveStr2Enum( strCfg.str, &data.perspective);  
    if( err != SUCCESS)
    {
        OscLog(WARN, 
                 "%s: No (valid) camera-scene perspective configured (%s). "
                 "Use default (%s).\n",
                 __func__, strCfg.str, OSC_CAM_PERSPECTIVE_DEFAULT);
		data.perspective = OSC_CAM_PERSPECTIVE_DEFAULT;
    }             


    /* Get exposure time setting from configuration. */            
    configKey.strSection = NULL;
    configKey.strTag = "EXP";
    err = OscCfgGetUInt32( data.hConfig,
            &configKey, 
            &data.exposureTime);    
    
    if( err != SUCCESS)
    {
        OscLog(WARN, 
                "%s: No (valid) Exposure Time defined in configuration (%d). "
                "Use default (%d).\n",
                __func__, data.exposureTime, DEFAULT_EXPOSURE_TIME);
        data.exposureTime = DEFAULT_EXPOSURE_TIME;
    }  

#ifdef HAS_CPLD		
    /* Get exposure delay setting from configuration. */            
    configKey.strSection = NULL;
    configKey.strTag = "DEL";
    err = OscCfgGetUInt16Range( data.hConfig,
            &configKey, 
            &exposureDelay, 
            0, 
            FINECLK2CLK_RATIO-1);    
    data.exposureDelay = exposureDelay & 0x00ff;
    if( err != SUCCESS)
    {
        OscLog(WARN, 
                "%s: No (valid) Exposure Delay defined in configuration (%d). "
                "Use default (%d).\n",
                __func__, data.exposureDelay, DEFAULT_EXPOSURE_DELAY);
        data.exposureDelay = DEFAULT_EXPOSURE_DELAY;
    }  
#endif /* HAS_CPLD */	
	
	
#ifdef HAS_CPLD	
	/* Get firmware version */
	err = OscCpldRget(OSC_LGX_FWREV, &data.firmwareRevision);	
	if(err != SUCCESS)
	{
	        OscLog(ERROR, "Cannot read firmware version. (%d)\n", err);
		goto cpld_err;
	}	

	/* Apply exposure delay to CPLD and disable. */
	err = OscCpldRset(OSC_LGX_CLKDELAY, 
		(const uint8)(data.exposureDelay & !OSC_LGX_CLKDELAY_ENABLE));		
	if(err != SUCCESS)
	{
		OscLog(ERROR, "Cannot disable clock-delay in CPLD.\n");
		goto cpld_err;
	}
	/* Set CPLD to synchronous mode. */
	err = OscCpldFset(OSC_LGX_VARCTRL, OSC_LGX_VARCTRL_SYNCOUT, OSC_LGX_VARCTRL_SYNCOUT);	
	if(err != SUCCESS)
	{
		OscLog(ERROR, "Cannot set CPLD to synchronous mode.\n");
		goto cpld_err;
	}
#endif /* HAS_CPLD */	
	
	

	/* Set the camera registers to sane default values. */
	err = OscCamPresetRegs();
	if (err != SUCCESS)
	{
		OscLog(ERROR, "%s: Unable to preset camera registers! (%d)\n", __func__, err);
		goto cam_err;
	}
	
	/* Set up two frame buffers with enough space for the maximum
	 * camera resolution in cached memory. */
	err = OscCamSetFrameBuffer(0, IMAGE_AERA, data.u8FrameBuffers[0], TRUE);
	if (err != SUCCESS)
	{
		OscLog(ERROR, "%s: Unable to set up first frame buffer!\n", __func__);
		goto cam_err;
	}
	err = OscCamSetFrameBuffer(1, IMAGE_AERA, data.u8FrameBuffers[1], TRUE);
	if (err != SUCCESS)
	{
		OscLog(ERROR, "%s: Unable to set up second frame buffer!\n", __func__);
		goto cam_err;
	}
	
	/* Create a double-buffer from the frame buffers initilalized above.*/
	err = OscCamCreateMultiBuffer(2, multiBufferIds);
	if (err != SUCCESS)
	{
		OscLog(ERROR, "%s: Unable to set up multi buffer!\n", __func__);
		goto mb_err;
	}
	
	OscCamSetupPerspective( data.perspective);

	/* Make the register file known to the communication protocol. */
	data.comm.pRegFile = regfile;
	data.comm.nRegs = (sizeof(regfile)/sizeof(struct CBP_PARAM));

	/* Init communication sockets. */
	err = Comm_Init(&data.comm);
	if (err != SUCCESS)
	{
		OscLog(ERROR, "Communication initialization failed.\n");
		goto comm_err;		
	}	
	
	return SUCCESS;
	
comm_err:    
cfg_err:
#ifdef HAS_CPLD	
cpld_err:
#endif /* HAS_CPLD */
mb_err:
cam_err:
    OscUnloadDependencies(data.hFramework,
            deps,
            sizeof(deps)/sizeof(struct OSC_DEPENDENCY));
dep_err:
	OscDestroy(&data.hFramework);
	
	return err;
}

OSC_ERR Unload()
{
	/******** Unload the framework module dependencies **********/
	OscUnloadDependencies(data.hFramework, deps, sizeof(deps)/sizeof(struct OSC_DEPENDENCY));
	
	OscDestroy(data.hFramework);
	
	/* Close all communication */
	Comm_DeInit(&data.comm);

	/* Clear global data fields. */
	memset(&data, 0, sizeof(struct DATA));
    	
	return SUCCESS;
}

void Terminate()
{   
    Unload();
    printf("Unload complete! Exiting.\n");
    
    exit(0);
    return;
}

/*********************************************************************//*!
 * @brief Program entry
 * 
 * @param argc Command line argument count.
 * @param argv Command line argument strings.
 * @return 0 on success
 *//*********************************************************************/
int main(const int argc, const char * argv[])
{
	OSC_ERR err = SUCCESS;
	
	err = init(argc, argv);
	if (err != SUCCESS)
	{
		OscLog(ERROR, "%s: Initialization failed!(%d)\n", __func__, err);
		return err;
	}
	OscLog(INFO, "Initialization successful.\n");
#ifdef HAS_CPLD	
	OscLog(INFO, "CPLD Firmware (Version: %d)\n", (int)data.firmwareRevision);
#endif /* HAS_CPLD */
	
	StateControl();
	
	Unload();
	return 0;
}
