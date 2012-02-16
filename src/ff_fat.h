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
 *  See RESTRICTIONS.TXT for extra restrictions on the use of FullFAT.       *
 *                                                                           *
 *  Removing the above notice is illegal and will invalidate this license.   *
 *****************************************************************************
 *  See http://worm.me.uk/fullfat for more information.                      *
 *  Or  http://fullfat.googlecode.com/ for latest releases and the wiki.     *
 *****************************************************************************/

/**
 *	@file		ff_fat.h
 *	@author		James Walmsley
 *	@ingroup	FAT
 **/

#ifndef _FF_FAT_H_
#define _FF_FAT_H_

#include "fat.h"
#include "ff_ioman.h"
#include "ff_blk.h"
#include "ff_types.h"

//---------- ERROR CODES


//---------- PROTOTYPES

FF_T_UINT32 FF_getRealLBA			(FF_IOMAN *pIoman, FF_T_UINT32 LBA);
FF_T_UINT32 FF_Cluster2LBA			(FF_IOMAN *pIoman, FF_T_UINT32 Cluster);
FF_T_UINT32 FF_LBA2Cluster			(FF_IOMAN *pIoman, FF_T_UINT32 Address);
FF_T_SINT32 FF_getFatEntry			(FF_IOMAN *pIoman, FF_T_UINT32 nCluster);
FF_T_BOOL	FF_isEndOfChain			(FF_IOMAN *pIoman, FF_T_UINT32 fatEntry);
FF_T_SINT8	FF_putFatEntry			(FF_IOMAN *pIoman, FF_T_UINT32 nCluster, FF_T_UINT32 Value);
FF_T_UINT32 FF_FindFreeCluster		(FF_IOMAN *pIoman);
FF_T_UINT32	FF_ExtendClusterChain	(FF_IOMAN *pIoman, FF_T_UINT32 StartCluster, FF_T_UINT16 Count);
FF_T_SINT8	FF_UnlinkClusterChain	(FF_IOMAN *pIoman, FF_T_UINT32 StartCluster, FF_T_UINT16 Count);
FF_T_UINT32	FF_TraverseFAT			(FF_IOMAN *pIoman, FF_T_UINT32 Start, FF_T_UINT32 Count);
FF_T_UINT32 FF_CreateClusterChain	(FF_IOMAN *pIoman);
FF_T_UINT32 FF_GetChainLength		(FF_IOMAN *pIoman, FF_T_UINT32 pa_nStartCluster);
FF_T_UINT32 FF_FindEndOfChain		(FF_IOMAN *pIoman, FF_T_UINT32 Start);

#endif

