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

/*! @file mainstate.h
 * @brief Definitions for main state machine
 */
	
#ifndef MAINSTATE_H
#define MAINSTATE_H

#include "rich-view.h"

enum MainStateEvents {
	FRAMESEQ_EVT,		/* frame ready to process (before setting up next frame capture) */
	FRAMEPAR_EVT,		/* frame ready to process (parallel to next capture) */
	TRIGGER_EVT,		/* Selftriggering event */ 
	CMD_GO_IDLE_EVT, 	/* Go to idle mode */
	CMD_GO_ACQ_EVT,         /* Go to acquisition mode */
	CMD_USE_INTERN_TRIGGER_EVT, /* Capture with internal trigger. */
	CMD_USE_EXTERN_TRIGGER_EVT  /* Capture with external trigger. */
};


/*typedef struct MainState MainState;*/
typedef struct MainState {
	Hsm super;
	State idle;
	State capture;
	State internal, external;
} MainState;


void MainStateConstruct(MainState *me);


#endif /*MAINSTATE_H*/
