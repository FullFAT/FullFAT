/*****************************************************************************
 *  FullFAT - High Performance, Thread-Safe Embedded FAT File-System         *
 *  Copyright (C) 2009  James Walmsley (james@worm.me.uk)                    *
 *                                                                           *
 *  This program is free software: you can redistribute it and/or modify     *
 *  it under the terms of the GNU General Public License as published by     *
 *  the Free Software Foundation, either version 3 of the License, or        *
 *  (at your option) any later version.                                      *
 *                                                                           *
 *  This program is distributed in the hope that it will be useful,          *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of           *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the            *
 *  GNU General Public License for more details.                             *
 *                                                                           *
 *  You should have received a copy of the GNU General Public License        *
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.    *
 *                                                                           *
 *  IMPORTANT NOTICE:                                                        *
 *  =================                                                        *
 *  Alternative Licensing is available directly from the Copyright holder,   *
 *  (James Walmsley). For more information consult LICENSING.TXT to obtain   *
 *  a Commercial license.                                                    *
 *                                                                           *
 *  See EXCEPTIONS.TXT for extra restrictions on the use of FullFAT.         *
 *                                                                           *
 *  Removing the above notice is illegal and will invalidate this license.   *
 *****************************************************************************
 *  See http://worm.me.uk/fullfat for more information.                      *
 *  Or  http://fullfat.googlecode.com/ for latest releases and the wiki.     *
 *****************************************************************************/

/**
 *	@file		ff_memory.c
 *	@author		James Walmsley
 *	@ingroup	MEMORY
 **/

#ifndef _FF_MEMORY_H_
#define _FF_MEMORY_H_

#include "ff_config.h"
#include "ff_types.h"

//---------- PROTOTYPES (in order of appearance)

// PUBLIC:

// PRIVATE:
FF_T_UINT8		FF_getChar	(FF_T_UINT8 *pBuffer, FF_T_UINT16 offset);
FF_T_UINT16		FF_getShort	(FF_T_UINT8 *pBuffer, FF_T_UINT16 offset);
FF_T_UINT32		FF_getLong	(FF_T_UINT8 *pBuffer, FF_T_UINT16 offset);

void			FF_putChar	(FF_T_UINT8 *pBuffer, FF_T_UINT16 offset, FF_T_UINT8 Value);
void			FF_putShort	(FF_T_UINT8 *pBuffer, FF_T_UINT16 offset, FF_T_UINT16 Value);
void			FF_putLong	(FF_T_UINT8 *pBuffer, FF_T_UINT16 offset, FF_T_UINT32 Value);


void			FF_tolower	(FF_T_INT8 *string, FF_T_UINT32 strLen);
FF_T_INT8		*FF_strtok	(FF_T_INT8 *string, FF_T_INT8 *token, FF_T_UINT16 *tokenNumber, FF_T_BOOL *last, FF_T_UINT16 Length);

#endif
