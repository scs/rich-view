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

/*! @file rich-view.h
 * @brief Global header file for the Rich-View application.
 * 
 * Target side application to provide a TCP feed of image data. On
 * a host a GUI is used to visualize the feed.
 * GUI controlled configuration of the smart camera is supported 
 * (eg. IP address, image orientation, exposure time, ...).
 */
#ifndef RICH_VIEW_H
#define RICH_VIEW_H

/*--------------------------- Includes -----------------------------*/
#include "inc/oscar.h"
#include "inc/oscar_target_type.h"
#include "communication.h"
#include "version.h"
#include <stdio.h>

/*------------------------ Board dependency ------------------------*/
#ifdef TARGET_TYPE_INDXCAM
	#define HAS_CPLD
#endif /* TARGET_TYPE_INDXCAM */

/*--------------------------- Settings ------------------------------*/
/*! @brief The number of frame buffers used. */
#define NR_FRAME_BUFFERS 2

/*! @brief Timeout (ms) when waiting for a new picture. */
#define CAMERA_TIMEOUT 1

/*! @brief defines the timeout for CMOS sensor */
#define TIMEOUT 100
/*! @brief defines the time constant for time-outs (CPU speed dependent) */
#define VERTICAL_BLANK_CYCLES 763000 /* Vertical blank time (1.42 ms) converted to CPU cycles. */

/*! @brief File name of the configuration */
#define CONFIG_FILE_NAME	"config" 

/*! @brief Max size of the file in bytes */
#define CONFIG_FILE_SIZE	1024

/*! @brief Default Exposure TIME (if not defined in config file) */
#define DEFAULT_EXPOSURE_TIME 441 /* units???? */
#ifdef HAS_CPLD
	/*! @brief Default Exposure Delay (if not defined in config file) */
	#define DEFAULT_EXPOSURE_DELAY 0
#endif /* HAS_CPLD */

/*--------------------------- Commands ------------------------------*/
/*! @brief command to start live-view mode */
#define CmdLiveMode		76			/* 'L' = Live-Mode Start (self triggering) */
/*! @brief command to start calibration mode */
#define CmdCalibMode	67			/* 'C' = Calibration-Mode Start (external trigger)*/
/*! @brief command to stop either mode. */
#define CmdStopCmd		83			/* 'S' = Stop cmd */
/*! @brief command to change brightness (exposure time) */
#define CmdBrightness	66			/* 'B' = Brightness cmd */
/*! @brief command to store MAC and IP addressin confugration */
#define CmdMacAddr		77			/* 'M' = MAC address cmd */
/*! @brief command to set the delay to Feintakt */
#define CmdDelay		68			/* 'D' = Delay cmd */
/*! @brief command to exit the program */
#define CmdExit			69			/* 'E' = Exit cmd */
/*! @brief command to store the delay and brightness (exposure time) to configuration */
#define CmdStoreDelay	84			/* 'T' = sTore Delay cmd */
/*! @brief command to store the camera perspective to configuration */
#define CmdPerspective	80			/* 'P' = Perspective (store) cmd */

/*--------------------------- Constants -----------------------------*/
/*! @brief Granularity of fine clocks within one clock tick */
#define FINECLK2CLK_RATIO 100

/*! @brief Image size in pixels: width * height */
#define IMAGE_AERA (OSC_CAM_MAX_IMAGE_WIDTH*OSC_CAM_MAX_IMAGE_HEIGHT)

/*------------------- Main data object and members ------------------*/

/*! @brief The structure storing all important variables of the application.
 * */
struct DATA
{
  /*! @brief The frame buffers for the frame capture device driver.*/
  uint8 u8FrameBuffers[NR_FRAME_BUFFERS][OSC_CAM_MAX_IMAGE_HEIGHT*OSC_CAM_MAX_IMAGE_WIDTH];
  /*! @brief A buffer to hold the resulting color image. */
  uint8 u8ResultImage[3*OSC_CAM_MAX_IMAGE_WIDTH*OSC_CAM_MAX_IMAGE_HEIGHT];

  /*! @brief The last raw image captured. Always points to one of the frame
   * buffers. */
  uint8* pCurRawImg;
	
  /*! @brief Handle to the framework instance. */
  void *hFramework;
	
  /*! @brief Handle to the configuration file */
  CFG_FILE_CONTENT_HANDLE hConfig;	
    
  /*! Firmware revision number */
  uint8 firmwareRevision;

  /*! @brief Camera-Scene perspective */
  enum EnOscCamPerspective perspective;
#ifdef HAS_CPLD		
  /*! @brief Fine clock delay value. */
  uint8 exposureDelay;
#endif /* HAS_CPLD */
  /*! @brief Exposure time [us] */
  uint32 exposureTime;	
  
		
  /*! @brief Socket for incomming UDP command packets */
  int cmdsock;						
  /*! @brief Socket for outgoing TCP data packets */
  int datasock;						
  /*! @brief Socket for outgoing TCP after connection to host */
  int connsock;	
  /*! @brief Recent UDP command packet */
  struct UdpCmdPacket cmdpkt;

};

extern struct DATA data;

/*-------------------------- Functions --------------------------------*/
/*********************************************************************//*!
 * @brief Unload everything before exiting.
 * 
 * @return SUCCESS or an appropriate error code.
 *//*********************************************************************/
OSC_ERR Unload();

/*********************************************************************//*!
 * @brief Shutdown and clean up the application.
 * 
 *//*********************************************************************/
void Terminate();

/*********************************************************************//*!
 * @brief Give control to statemachine.
 * 
 * @return SUCCESS or an appropriate error code otherwise
 * 
 * The function does never return normally except in error case.
 *//*********************************************************************/
OSC_ERR StateControl(void);


OSC_ERR StoreMac(void);
OSC_ERR StoreIp(void);
OSC_ERR StoreDelay(void);
OSC_ERR StoreExposure(void);
OSC_ERR StorePerspective(void);

/*********************************************************************//*!
 * @brief Process a newly captured frame.
 * 
 * In the case of this template, this consists just of debayering the
 * image and writing the result to the result image buffer. This should
 * be the starting point where you add your code.
 * 
 * @param pRawImg The raw image to process.
 *//*********************************************************************/
void ProcessFrame(uint8 *pRawImg);

/*********************************************************************//*!
 * @brief Get software version numbers
 * 
 * Used scheme: major.minor[.revsion]
 * 
 * The major number is used for significant changes in functionality or 
 * supported hardware. The minor number decodes small feature changes.
 * The patch number is intended for bug fixes.
 * 
 * @param hMajor Pointer to major version number.
 * @param hMinor Pointer to minor version number.
 * @param hPatch Pointer to patch number.
 * @return SUCCESS or an appropriate error code.
 *//*********************************************************************/
OSC_ERR GetVersionNumber(char *hMajor, char *hMinor, char *hPatch);

/*********************************************************************//*!
 * @brief Get software version string
 * 
 * Version string format: v<major>.<minor>[-p<patch>]  eg: v1.3  or v1.3-p1
 * The patch number is not printed if no bug-fixes are available (patch=0).
 *  
 * See @see GetVersionNumber for number interpretation.
 * 
 * @param hMajor Pointer to formated version string.
 * @return SUCCESS or an appropriate error code.
 *//*********************************************************************/
OSC_ERR GetVersionString(char *hVersion);

#endif	/* RICH_VIEW_H */
