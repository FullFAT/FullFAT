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
 *	@file		ff_file.c
 *	@author		James Walmsley
 *	@ingroup	FILEIO
 *
 *	@defgroup	FILEIO FILE I/O Access
 *	@brief		Provides an interface to allow File I/O on a mounted IOMAN.
 *
 *	Provides file-system interfaces for the FAT file-system.
 **/

#include "ff_file.h"
#include "ff_string.h"

/**
 *	@public
 *	@brief	Converts STDIO mode strings into the equivalent FullFAT mode.
 *
 *	@param	Mode	The mode string e.g. "rb" "rb+" "w" "a" "r" "w+" "a+" etc
 *
 *	@return	Returns the mode bits that should be passed to the FF_Open function.
 **/
FF_T_UINT8 FF_GetModeBits(FF_T_INT8 *Mode) {
	FF_T_UINT8 ModeBits = 0x00;
	while(*Mode) {
		switch(*Mode) {
			case 'r':	// Allow Read
			case 'R':
				ModeBits |= FF_MODE_READ;
				break;

			case 'w':	// Allow Write
			case 'W':
				ModeBits |= FF_MODE_WRITE;
				ModeBits |= FF_MODE_CREATE;	// Create if not exist.
				ModeBits |= FF_MODE_TRUNCATE;
				break;

			case 'a':	// Append new writes to the end of the file.
			case 'A':
				ModeBits |= FF_MODE_WRITE;
				ModeBits |= FF_MODE_APPEND;
				ModeBits |= FF_MODE_CREATE;	// Create if not exist.
				break;

			case '+':	// Update the file, don't Append!
				ModeBits |= FF_MODE_READ;	// RW Mode
				ModeBits |= FF_MODE_WRITE;	// RW Mode
				break;

			/*case 'D':	// Internal use only!
				ModeBits |= FF_MODE_DIR;
				break;*/

			default:	// b|B flags not supported (Binary mode is native anyway).
				break;
		}
		Mode++;
	}

	return ModeBits;
}

/**
 * FF_Open() Mode Information
 * - FF_MODE_WRITE
 *   - Allows WRITE access to the file.
 *   .
 * - FF_MODE_READ
 *   - Allows READ access to the file.
 *   .
 * - FF_MODE_CREATE
 *   - Create file if it doesn't exist.
 *   .
 * - FF_MODE_TRUNCATE
 *   - Erase the file if it already exists and overwrite.
 *   *
 * - FF_MODE_APPEND
 *   - Causes all writes to occur at the end of the file. (Its impossible to overwrite other data in a file with this flag set).
 *   . 
 * .
 *
 * Some sample modes:
 * - (FF_MODE_WRITE | FF_MODE_CREATE | FF_MODE_TRUNCATE)
 *   - Write access to the file. (Equivalent to "w").
 *   .
 * - (FF_MODE_WRITE | FF_MODE_READ)
 *   - Read and Write access to the file. (Equivalent to "rb+").
 *   .
 * - (FF_MODE_WRITE | FF_MODE_READ | FF_MODE_APPEND | FF_MODE_CREATE)
 *   - Read and Write append mode access to the file. (Equivalent to "a+").
 *   .
 * .
 * Be careful when choosing modes. For those using FF_Open() at the application layer
 * its best to use the provided FF_GetModeBits() function, as this complies to the same
 * behaviour as the stdio.h fopen() function.
 *
 **/


/**
 *	@public
 *	@brief	Opens a File for Access
 *
 *	@param	pIoman		FF_IOMAN object that was created by FF_CreateIOMAN().
 *	@param	path		Path to the File or object.
 *	@param	Mode		Access Mode required. Modes are a little complicated, the function FF_GetModeBits()
 *	@param	Mode		will convert a stdio Mode string into the equivalent Mode bits for this parameter.
 *	@param	pError		Pointer to a signed byte for error checking. Can be NULL if not required.
 *	@param	pError		To be checked when a NULL pointer is returned.
 *
 *	@return	NULL pointer on Error, in which case pError should be checked for more information.
 *	@return	pError can be:
 **/
FF_FILE *FF_Open(FF_IOMAN *pIoman, const FF_T_INT8 *path, FF_T_UINT8 Mode, FF_ERROR *pError) {
	FF_FILE		*pFile;
	FF_FILE		*pFileChain;
	FF_DIRENT	Object;
	FF_T_UINT32 DirCluster, FileCluster;
	FF_T_UINT32	nBytesPerCluster;
	FF_T_INT8	filename[FF_MAX_FILENAME];

	FF_T_UINT16	i;

	if(pError) {
		*pError = 0;
	}
	
	if(!pIoman) {
		if(pError) {
			*pError = FF_ERR_NULL_POINTER;
		}
		return (FF_FILE *)NULL;
	}
	pFile = FF_MALLOC(sizeof(FF_FILE));
	if(!pFile) {
		if(pError) {
			*pError = FF_ERR_NOT_ENOUGH_MEMORY;
		}
		return (FF_FILE *)NULL;
	}

	// Get the Mode Bits.
	pFile->Mode = Mode;

	i = (FF_T_UINT16) strlen(path);

	while(i != 0) {
		if(path[i] == '\\' || path[i] == '/') {
			break;
		}
		i--;
	}

	strncpy(filename, (path + i + 1), FF_MAX_FILENAME);

	if(i == 0) {
		i = 1;
	}
	

	DirCluster = FF_FindDir(pIoman, path, i);

	if(DirCluster) {
		//RetVal = //FF_FindEntry(pIoman, DirCluster, filename, &Object, FF_TRUE);
		//if(RetVal >= 0) {
			//FileCluster = Object.ObjectCluster;//FF_FindEntryInDir(pIoman, DirCluster, filename, 0x00, &Object);
		//} else {
		//	FileCluster = 0;
		//}

		FileCluster = FF_FindEntryInDir(pIoman, DirCluster, filename, 0x00, &Object);

		if(!FileCluster) {	// If 0 was returned, it might be because the file has no allocated cluster
			if(strlen(filename) == strlen(Object.FileName)) {
				if(Object.Filesize == 0 && FF_strmatch(filename, Object.FileName, (FF_T_UINT16) strlen(filename)) == FF_TRUE) {
					// The file really was found!
					FileCluster = 1;
				} 
			}
		}

		if(!FileCluster) {
			if((pFile->Mode & FF_MODE_CREATE)) {
				FileCluster = FF_CreateFile(pIoman, DirCluster, filename, &Object);
				Object.CurrentItem += 1;
			}
		}
		
		if(FileCluster) {
			if(Object.Attrib == FF_FAT_ATTR_DIR) {
				if(!(pFile->Mode & FF_MODE_DIR)) {
					// Not the object, File Not Found!
					FF_FREE(pFile);
					if(pError) {
						*pError = FF_ERR_FILE_OBJECT_IS_A_DIR;
					}
					return (FF_FILE *) NULL;
				}
			}
			
			//---------- Ensure Read-Only files don't get opened for Writing.
			if((pFile->Mode & FF_MODE_WRITE) || (pFile->Mode & FF_MODE_APPEND)) {
				if((Object.Attrib & FF_FAT_ATTR_READONLY)) {
					FF_FREE(pFile);
					if(pError) {
						*pError = FF_ERR_FILE_IS_READ_ONLY;
					}
					return (FF_FILE *) NULL;
				}
			}
			pFile->pIoman				= pIoman;
			pFile->FilePointer			= 0;
			pFile->ObjectCluster		= Object.ObjectCluster;
			pFile->Filesize				= Object.Filesize;
			pFile->CurrentCluster		= 0;
			pFile->AddrCurrentCluster	= pFile->ObjectCluster;
			//pFile->Mode					= Mode;
			pFile->Next					= NULL;
			pFile->DirCluster			= DirCluster;
			pFile->DirEntry				= Object.CurrentItem - 1;
			nBytesPerCluster			= pFile->pIoman->pPartition->SectorsPerCluster / pIoman->BlkSize;
			pFile->iChainLength			= 0;
			pFile->iEndOfChain			= 0;
			pFile->FileDeleted			= FF_FALSE;

			// File Permission Processing
			// Only "w" and "w+" mode strings can erase a file's contents.
			// Any other combinations will not cause an erase.
			if((pFile->Mode & FF_MODE_TRUNCATE)) {
				pFile->Filesize = 0;
				pFile->FilePointer = 0;
			}

			/*
				Add pFile onto the end of our linked list of FF_FILE objects.
			*/
			FF_PendSemaphore(pIoman->pSemaphore);
			{
				if(!pIoman->FirstFile) {
					pIoman->FirstFile = pFile;
				} else {
					pFileChain = (FF_FILE *) pIoman->FirstFile;
					do {
						if(pFileChain->ObjectCluster == pFile->ObjectCluster) {
							// File is already open! DON'T ALLOW IT!
							FF_ReleaseSemaphore(pIoman->pSemaphore);
							FF_FREE(pFile);
							if(pError) {
								*pError = FF_ERR_FILE_ALREADY_OPEN;
							}
							return (FF_FILE *) NULL;
						}
						if(!pFileChain->Next) {
							pFileChain->Next = pFile;
							break;
						}
						pFileChain = (FF_FILE *) pFileChain->Next;
					}while(pFileChain != NULL);
				}
			}
			FF_ReleaseSemaphore(pIoman->pSemaphore);

			return pFile;
		}else {
			FF_FREE(pFile);
			if(pError) {
				*pError = FF_ERR_FILE_NOT_FOUND;
			}
			return (FF_FILE *) NULL;
		} 
	}
	if(pError) {
		*pError = FF_ERR_FILE_INVALID_PATH;
	}

	FF_FREE(pFile);

	return (FF_FILE *)NULL;
}


/**
 *	@public
 *	@brief	Tests if a Directory contains any other files or folders.
 *
 *	@param	pIoman	FF_IOMAN object returned from the FF_CreateIOMAN() function.
 *
 **/
FF_T_BOOL FF_isDirEmpty(FF_IOMAN *pIoman, const FF_T_INT8 *Path) {
	
	FF_DIRENT	MyDir;
	FF_ERROR	RetVal = FF_ERR_NONE;
	FF_T_UINT8	i = 0;

	if(!pIoman) {
		return FF_FALSE;
	}
	
	RetVal = FF_FindFirst(pIoman, &MyDir, Path);
	while(RetVal == 0) {
		i++;
		RetVal = FF_FindNext(pIoman, &MyDir);
		if(i > 2) {
			return FF_FALSE;
		}
	}

	return FF_TRUE;
}

FF_ERROR FF_RmDir(FF_IOMAN *pIoman, const FF_T_INT8 *path) {
	FF_FILE 			*pFile;
	FF_ERROR 			Error = FF_ERR_NONE;
	FF_T_UINT8 			EntryBuffer[32];
	FF_FETCH_CONTEXT	FetchContext;
	FF_T_SINT8 			RetVal = FF_ERR_NONE;
#ifdef FF_PATH_CACHE
	FF_T_UINT32 i;
#endif

	if(!pIoman) {
		return FF_ERR_NULL_POINTER;
	}

	pFile = FF_Open(pIoman, path, FF_MODE_DIR, &Error);

	if(!pFile) {
		return Error;	// File in use or File not found!
	}

	pFile->FileDeleted = FF_TRUE;
	
	FF_lockDIR(pIoman);
	{
		if(FF_isDirEmpty(pIoman, path)) {
			FF_lockFAT(pIoman);
			{
				FF_UnlinkClusterChain(pIoman, pFile->ObjectCluster, 0);	// 0 to delete the entire chain!
			}
			FF_unlockFAT(pIoman);

			// Initialise the dirent Fetch Context object for faster removal of dirents.

			FF_InitEntryFetch(pIoman, pFile->DirCluster, &FetchContext);
			
			// Edit the Directory Entry! (So it appears as deleted);
			FF_RmLFNs(pIoman, pFile->DirEntry, &FetchContext);
			FF_FetchEntryWithContext(pIoman, pFile->DirEntry, &FetchContext, EntryBuffer);
			EntryBuffer[0] = 0xE5;
			//FF_ReleaseBuffer(pIoman, FetchContext.pBuffer);
			//FetchContext.pBuffer = NULL;
			//FF_PushEntry(pIoman, pFile->DirCluster, pFile->DirEntry, EntryBuffer);
			FF_PushEntryWithContext(pIoman, pFile->DirEntry, &FetchContext, EntryBuffer);
#ifdef FF_PATH_CACHE
			FF_PendSemaphore(pIoman->pSemaphore);	// Thread safety on shared object!
			{
				for(i = 0; i < FF_PATH_CACHE_DEPTH; i++) {
					if(FF_strmatch(pIoman->pPartition->PathCache[i].Path, path, (FF_T_UINT16)strlen(path))) {
						pIoman->pPartition->PathCache[i].Path[0] = '\0';
						pIoman->pPartition->PathCache[i].DirCluster = 0;
						FF_ReleaseSemaphore(pIoman->pSemaphore);
					}
				}
			}
			FF_ReleaseSemaphore(pIoman->pSemaphore);
#endif
			
			FF_IncreaseFreeClusters(pIoman, pFile->iChainLength);

			FF_CleanupEntryFetch(pIoman, &FetchContext);

			FF_FlushCache(pIoman);
		} else {
			RetVal = FF_ERR_DIR_NOT_EMPTY;
		}
	}
	FF_unlockDIR(pIoman);

	
	
	FF_Close(pFile); // Free the file pointer resources
	// File is now lost!
	return RetVal;
}

FF_ERROR FF_RmFile(FF_IOMAN *pIoman, const FF_T_INT8 *path) {
	FF_FILE *pFile;
	FF_ERROR Error = 0;
	FF_T_UINT8 EntryBuffer[32];
	FF_FETCH_CONTEXT FetchContext;

	pFile = FF_Open(pIoman, path, FF_MODE_READ, &Error);

	if(!pFile) {
		return Error;	// File in use or File not found!
	}

	pFile->FileDeleted = FF_TRUE;

	if(pFile->ObjectCluster) {	// Ensure there is actually a cluster chain to delete!
		FF_lockFAT(pIoman);	// Lock the FAT so its thread-safe.
		{
			FF_UnlinkClusterChain(pIoman, pFile->ObjectCluster, 0);	// 0 to delete the entire chain!
		}
		FF_unlockFAT(pIoman);
	}

	// Edit the Directory Entry! (So it appears as deleted);
	FF_lockDIR(pIoman);
	{
		FF_InitEntryFetch(pIoman, pFile->DirCluster, &FetchContext);
		FF_RmLFNs(pIoman, pFile->DirEntry, &FetchContext);
		FF_FetchEntryWithContext(pIoman, pFile->DirEntry, &FetchContext, EntryBuffer);
		EntryBuffer[0] = 0xE5;
		//FF_ReleaseBuffer(pIoman, FetchContext.pBuffer);
		//FetchContext.pBuffer = NULL;
		//FF_PushEntry(pIoman, pFile->DirCluster, pFile->DirEntry, EntryBuffer);
		FF_PushEntryWithContext(pIoman, pFile->DirEntry, &FetchContext, EntryBuffer);
		FF_CleanupEntryFetch(pIoman, &FetchContext);
	}
	FF_unlockDIR(pIoman);

	FF_FlushCache(pIoman);
	
	FF_Close(pFile); // Free the file pointer resources
	return 0;
}

/**
 *	@public
 *	@brief	Moves a file or directory from source to destination.
 *
 *	@param	pIoman				The FF_IOMAN object pointer.
 *	@param	szSourceFile		String of the source file to be moved or renamed.
 *	@param	szDestinationFile	String of the destination file to where the source should be moved or renamed.
 *
 *	@return	FF_ERR_NONE on success.
 *	@return FF_ERR_FILE_DESTINATION_EXISTS if the destination file exists.
 *	@return FF_ERR_FILE_COULD_NOT_CREATE_DIRENT if dirent creation failed (fatal error!).
 *	@return FF_ERR_FILE_DIR_NOT_FOUND if destination directory was not found.
 *	@return FF_ERR_FILE_SOURCE_NOT_FOUND if the source file was not found.
 *
 **/
FF_ERROR FF_Move(FF_IOMAN *pIoman, const FF_T_INT8 *szSourceFile, const FF_T_INT8 *szDestinationFile) {
	FF_ERROR	Error;
	FF_FILE		*pSrcFile, *pDestFile;
	FF_DIRENT	MyFile;
	FF_T_UINT8	EntryBuffer[32];
	FF_T_UINT16 i;
	FF_T_UINT32	DirCluster;
	FF_FETCH_CONTEXT	FetchContext;

	if(!pIoman) {
		return FF_ERR_NULL_POINTER;
	}

	// Check destination file doesn't exist!
	pDestFile = FF_Open(pIoman, szDestinationFile, FF_MODE_READ, &Error);

	if(pDestFile || (Error == FF_ERR_FILE_OBJECT_IS_A_DIR)) {
		FF_Close(pDestFile);
		return FF_ERR_FILE_DESTINATION_EXISTS;	// YES -- FAIL
	}

	pSrcFile = FF_Open(pIoman, szSourceFile, FF_MODE_READ, &Error);

	if(Error == FF_ERR_FILE_OBJECT_IS_A_DIR) {
		// Open a directory for moving!
		pSrcFile = FF_Open(pIoman, szSourceFile, FF_MODE_DIR, &Error);
	}

	if(pSrcFile) {

		// Create the new dirent.
		FF_InitEntryFetch(pIoman, pSrcFile->DirCluster, &FetchContext);
		FF_FetchEntryWithContext(pIoman, pSrcFile->DirEntry, &FetchContext, EntryBuffer);
		//FF_FetchEntry(pIoman, pSrcFile->DirCluster, pSrcFile->DirEntry, EntryBuffer);
		MyFile.Attrib			= FF_getChar(EntryBuffer,  (FF_T_UINT16)(FF_FAT_DIRENT_ATTRIB));
		MyFile.Filesize			= pSrcFile->Filesize;
		MyFile.ObjectCluster	= pSrcFile->ObjectCluster;
		MyFile.CurrentItem		= 0;

		i = (FF_T_UINT16) strlen(szDestinationFile);

		while(i != 0) {
			if(szDestinationFile[i] == '\\' || szDestinationFile[i] == '/') {
				break;
			}
			i--;
		}

		strncpy(MyFile.FileName, (szDestinationFile + i + 1), FF_MAX_FILENAME);

		if(i == 0) {
			i = 1;
		}
		

		DirCluster = FF_FindDir(pIoman, szDestinationFile, i);
		
		if(DirCluster) {

			// Destination Dir was found, we can now create the new entry.
			if(FF_CreateDirent(pIoman, DirCluster, &MyFile)) {
				FF_Close(pSrcFile);
				return FF_ERR_FILE_COULD_NOT_CREATE_DIRENT;	// FAILED
			}

			// Edit the Directory Entry! (So it appears as deleted);
			FF_lockDIR(pIoman);
			{

				FF_RmLFNs(pIoman, pSrcFile->DirEntry, &FetchContext);
				FF_FetchEntryWithContext(pIoman, pSrcFile->DirEntry, &FetchContext, EntryBuffer);
				//FF_FetchEntry(pIoman, pSrcFile->DirCluster, pSrcFile->DirEntry, EntryBuffer);
				EntryBuffer[0] = 0xE5;
				//FF_PushEntry(pIoman, pSrcFile->DirCluster, pSrcFile->DirEntry, EntryBuffer);
				FF_PushEntryWithContext(pIoman, pSrcFile->DirEntry, &FetchContext, EntryBuffer);
				FF_CleanupEntryFetch(pIoman, &FetchContext);
			}
			FF_unlockDIR(pIoman);
			FF_Close(pSrcFile);

			FF_FlushCache(pIoman);

			return FF_ERR_NONE;
		}

		return FF_ERR_FILE_DIR_NOT_FOUND;

	}
		
	return FF_ERR_FILE_SOURCE_NOT_FOUND; // Source not found!
}


/**
 *	@public
 *	@brief	Get's the next Entry based on the data recorded in the FF_DIRENT object.
 *
 *	@param	pFile	FF_FILE object that was created by FF_Open().
 *
 *	@return FF_TRUE if End of File was reached. FF_FALSE if not.
 *	@return FF_FALSE if a null pointer was provided.
 *
 **/
FF_T_BOOL FF_isEOF(FF_FILE *pFile) {
	if(!pFile) {
		return FF_FALSE;
	}
	if(pFile->FilePointer >= pFile->Filesize) {
		return FF_TRUE;
	} else {
		return FF_FALSE;
	}
}

static FF_T_UINT32 FF_GetSequentialClusters(FF_IOMAN *pIoman, FF_T_UINT32 StartCluster, FF_T_UINT32 Limit) {
	FF_T_UINT32 CurrentCluster;
	FF_T_UINT32 NextCluster = StartCluster;
	FF_T_UINT32 i = 0;

	do {
		CurrentCluster = NextCluster;
		NextCluster = FF_getFatEntry(pIoman, CurrentCluster);
		if(NextCluster == (CurrentCluster + 1)) {
			i++;
		} else {
			break;
		}

		if(Limit) {
			if(i == Limit) {
				break;
			}
		}
	}while(NextCluster == (CurrentCluster + 1));

	return i;
}

static FF_T_SINT32 FF_ReadClusters(FF_FILE *pFile, FF_T_UINT32 Count, FF_T_UINT8 *buffer) {
	FF_T_UINT32 Sectors;
	FF_T_UINT32 SequentialClusters = 0;
	FF_T_UINT32 nItemLBA;
	FF_T_SINT32 RetVal;	

	while(Count != 0) {
		if((Count - 1) > 0) {
			SequentialClusters = FF_GetSequentialClusters(pFile->pIoman, pFile->AddrCurrentCluster, (Count - 1));
		}
		Sectors = (SequentialClusters + 1) * pFile->pIoman->pPartition->SectorsPerCluster;
		nItemLBA = FF_Cluster2LBA(pFile->pIoman, pFile->AddrCurrentCluster);
		nItemLBA = FF_getRealLBA(pFile->pIoman, nItemLBA);

		do {
			if(pFile->pIoman->pBlkDevice->fnReadBlocks) {
#ifdef	FF_BLKDEV_USES_SEM
				FF_PendSemaphore(pFile->pIoman->pSemaphore);
#endif
				// Called from FF_Read, sem not claimed
				RetVal = pFile->pIoman->pBlkDevice->fnReadBlocks(buffer, nItemLBA, Sectors, pFile->pIoman->pBlkDevice->pParam);
#ifdef	FF_BLKDEV_USES_SEM
				FF_ReleaseSemaphore(pFile->pIoman->pSemaphore);
#endif
				if(RetVal == FF_ERR_DRIVER_BUSY) {
					FF_Yield();
					FF_Sleep(FF_DRIVER_BUSY_SLEEP);
				}
			} else {
				RetVal = FF_ERR_DEVICE_DRIVER_FAILED;
			}
			
		}while(RetVal == FF_ERR_DRIVER_BUSY);	
		
		Count -= (SequentialClusters + 1);
		pFile->AddrCurrentCluster = FF_TraverseFAT(pFile->pIoman, pFile->AddrCurrentCluster, (SequentialClusters + 1));
		pFile->CurrentCluster += (SequentialClusters + 1);
		buffer += Sectors * pFile->pIoman->BlkSize;
		SequentialClusters = 0;
	}

	return 0;
}


static FF_ERROR FF_ExtendFile(FF_FILE *pFile, FF_T_UINT32 Size) {
	FF_IOMAN	*pIoman = pFile->pIoman;
	FF_T_UINT32 nBytesPerCluster = pIoman->pPartition->BlkSize * pIoman->pPartition->SectorsPerCluster;
	FF_T_UINT32 nTotalClustersNeeded = Size / nBytesPerCluster;
	FF_T_UINT32 nClusterToExtend; 
	FF_T_UINT32 CurrentCluster, NextCluster;
	FF_T_UINT32	i;
	FF_DIRENT	OriginalEntry;

	if((pFile->Mode & FF_MODE_WRITE) != FF_MODE_WRITE) {
		return FF_ERR_FILE_NOT_OPENED_IN_WRITE_MODE;
	}

	if(pFile->Filesize == 0 && pFile->ObjectCluster == 0) {	// No Allocated clusters.
		// Create a Cluster chain!
		pFile->AddrCurrentCluster = FF_CreateClusterChain(pFile->pIoman);
		if(!FF_GetEntry(pIoman, pFile->DirEntry, pFile->DirCluster, &OriginalEntry)) {
			OriginalEntry.ObjectCluster = pFile->AddrCurrentCluster;
			FF_PutEntry(pIoman, pFile->DirEntry, pFile->DirCluster, &OriginalEntry);
		} else {
			return FF_ERR_FILE_EXTEND_FAILED;
		}
		pFile->ObjectCluster = pFile->AddrCurrentCluster;
		pFile->iChainLength = 1;
		pFile->CurrentCluster = 0;
		pFile->iEndOfChain = pFile->AddrCurrentCluster;
	}
	
	if(Size % nBytesPerCluster) {
		nTotalClustersNeeded += 1;
	}

	if(pFile->iChainLength == 0) {	// First extension requiring the chain length, 
		pFile->iChainLength = FF_GetChainLength(pIoman, pFile->ObjectCluster, &pFile->iEndOfChain);
	}

	nClusterToExtend = (nTotalClustersNeeded - pFile->iChainLength);

	if(nTotalClustersNeeded > pFile->iChainLength) {

		NextCluster = pFile->AddrCurrentCluster;
		FF_lockFAT(pIoman);
		{
			for(i = 0; i <= nClusterToExtend; i++) {
				CurrentCluster = FF_FindEndOfChain(pIoman, NextCluster);
				NextCluster = FF_FindFreeCluster(pIoman);
				if(!NextCluster) {
					FF_unlockFAT(pIoman);
					FF_DecreaseFreeClusters(pIoman, i);
					return FF_ERR_FAT_NO_FREE_CLUSTERS;
				}
				FF_putFatEntry(pIoman, CurrentCluster, NextCluster);
				FF_putFatEntry(pIoman, NextCluster, 0xFFFFFFFF);
			}
			
			pFile->iEndOfChain = FF_FindEndOfChain(pIoman, NextCluster);
		}
		FF_unlockFAT(pIoman);
		
		pFile->iChainLength += i;
		FF_DecreaseFreeClusters(pIoman, i);	// Keep Tab of Numbers for fast FreeSize()
	}

	return FF_ERR_NONE;
}

static FF_T_SINT32 FF_WriteClusters(FF_FILE *pFile, FF_T_UINT32 Count, FF_T_UINT8 *buffer) {
	FF_T_UINT32 Sectors;
	FF_T_UINT32 SequentialClusters = 0;
	FF_T_UINT32 nItemLBA;
	FF_T_SINT32 RetVal;	

	while(Count != 0) {
		if((Count - 1) > 0) {
			SequentialClusters = FF_GetSequentialClusters(pFile->pIoman, pFile->AddrCurrentCluster, (Count - 1));
		}
		Sectors = (SequentialClusters + 1) * pFile->pIoman->pPartition->SectorsPerCluster;
		nItemLBA = FF_Cluster2LBA(pFile->pIoman, pFile->AddrCurrentCluster);
		nItemLBA = FF_getRealLBA(pFile->pIoman, nItemLBA);

		do {
			if(pFile->pIoman->pBlkDevice->fnWriteBlocks) {
#ifdef	FF_BLKDEV_USES_SEM
				FF_PendSemaphore(pFile->pIoman->pSemaphore);
#endif
				// Called from FF_Write, sem not claimed
				RetVal = pFile->pIoman->pBlkDevice->fnWriteBlocks(buffer, nItemLBA, Sectors, pFile->pIoman->pBlkDevice->pParam);
#ifdef	FF_BLKDEV_USES_SEM
				FF_ReleaseSemaphore(pFile->pIoman->pSemaphore);
#endif
				if(RetVal == FF_ERR_DRIVER_BUSY) {
					FF_Yield();
					FF_Sleep(FF_DRIVER_BUSY_SLEEP);
				}
			} else {
				RetVal = FF_ERR_DEVICE_DRIVER_FAILED;
			}
			
		}while(RetVal == FF_ERR_DRIVER_BUSY);
		
		Count -= (SequentialClusters + 1);
		pFile->AddrCurrentCluster = FF_TraverseFAT(pFile->pIoman, pFile->AddrCurrentCluster, (SequentialClusters + 1));
		pFile->CurrentCluster += (SequentialClusters + 1);
		buffer += Sectors * pFile->pIoman->BlkSize;
		SequentialClusters = 0;
	}

	return 0;
}

/**
 *	@public
 *	@brief	Equivalent to fread()
 *
 *	@param	pFile		FF_FILE object that was created by FF_Open().
 *	@param	ElementSize	The size of an element to read.
 *	@param	Count		The number of elements to read.
 *	@param	buffer		A pointer to a buffer of adequate size to be filled with the requested data.
 *
 *	@return Number of bytes read.
 *
 **/
FF_T_SINT32 FF_Read(FF_FILE *pFile, FF_T_UINT32 ElementSize, FF_T_UINT32 Count, FF_T_UINT8 *buffer) {
	FF_T_UINT32 nBytes = ElementSize * Count;
	FF_T_UINT32	nBytesRead = 0;
	FF_T_UINT32 nBytesToRead;
	FF_IOMAN	*pIoman;
	FF_BUFFER	*pBuffer;
	FF_T_UINT32 nRelBlockPos;
	FF_T_UINT32	nItemLBA;
	FF_T_SINT32	RetVal = 0;
	FF_T_UINT16	sSectors;
	FF_T_UINT32 nRelClusterPos;
	FF_T_UINT32 nBytesPerCluster;
	FF_T_UINT32	nClusterDiff;

	if(!pFile) {
		return FF_ERR_NULL_POINTER;
	}

	if(!(pFile->Mode & FF_MODE_READ)) {
		return FF_ERR_FILE_NOT_OPENED_IN_READ_MODE;
	}

	pIoman = pFile->pIoman;

	if(pFile->FilePointer == pFile->Filesize) {
		return 0;
	}

	if((pFile->FilePointer + nBytes) > pFile->Filesize) {
		nBytes = pFile->Filesize - pFile->FilePointer;
	}
	
	nClusterDiff = FF_getClusterChainNumber(pFile->pIoman, pFile->FilePointer, 1) - pFile->CurrentCluster;
	if(nClusterDiff) {
		if(pFile->CurrentCluster < FF_getClusterChainNumber(pFile->pIoman, pFile->FilePointer, 1)) {
			pFile->AddrCurrentCluster = FF_TraverseFAT(pIoman, pFile->AddrCurrentCluster, nClusterDiff);
			pFile->CurrentCluster += nClusterDiff;
		}
	}

	nRelBlockPos = FF_getMinorBlockEntry(pIoman, pFile->FilePointer, 1); // Get the position within a block.
	
	nItemLBA = FF_Cluster2LBA(pIoman, pFile->AddrCurrentCluster);
	nItemLBA = FF_getRealLBA(pIoman, nItemLBA + FF_getMajorBlockNumber(pIoman, pFile->FilePointer, 1)) + FF_getMinorBlockNumber(pIoman, pFile->FilePointer, 1);

	if((nRelBlockPos + nBytes) < pIoman->BlkSize) {	// Bytes to read are within a block and less than a block size.
		pBuffer = FF_GetBuffer(pIoman, nItemLBA, FF_MODE_READ);
		{
			memcpy(buffer, (pBuffer->pBuffer + nRelBlockPos), nBytes);
		}
		FF_ReleaseBuffer(pIoman, pBuffer);

		pFile->FilePointer += nBytes;
		
		return nBytes;		// Return the number of bytes read.

	} else {

		//---------- Read (memcpy) to a Sector Boundary
		if(nRelBlockPos != 0) {	// Not on a sector boundary, at this point the LBA is known.
			nBytesToRead = pIoman->BlkSize - nRelBlockPos;
			pBuffer = FF_GetBuffer(pIoman, nItemLBA, FF_MODE_READ);
			{
				// Here we copy to the sector boudary.
				memcpy(buffer, (pBuffer->pBuffer + nRelBlockPos), nBytesToRead);
			}
			FF_ReleaseBuffer(pIoman, pBuffer);

			nBytes				-= nBytesToRead;
			nBytesRead			+= nBytesToRead;
			pFile->FilePointer	+= nBytesToRead;
			buffer				+= nBytesToRead;
			
		}

		//---------- Read to a Cluster Boundary
		
		nRelClusterPos = FF_getClusterPosition(pIoman, pFile->FilePointer, 1);
		nBytesPerCluster = (pIoman->pPartition->SectorsPerCluster * pIoman->BlkSize);
		if(nRelClusterPos != 0 && nBytes >= nBytesPerCluster) { // Need to get to cluster boundary
			
			nClusterDiff = FF_getClusterChainNumber(pFile->pIoman, pFile->FilePointer, 1) - pFile->CurrentCluster;
			if(nClusterDiff) {
				if(pFile->CurrentCluster < FF_getClusterChainNumber(pFile->pIoman, pFile->FilePointer, 1)) {
					pFile->AddrCurrentCluster = FF_TraverseFAT(pIoman, pFile->AddrCurrentCluster, nClusterDiff);
					pFile->CurrentCluster += nClusterDiff;
				}
			}
		
			nItemLBA = FF_Cluster2LBA(pIoman, pFile->AddrCurrentCluster);
			nItemLBA = FF_getRealLBA(pIoman, nItemLBA + FF_getMajorBlockNumber(pIoman, pFile->FilePointer, 1)) + FF_getMinorBlockNumber(pIoman, pFile->FilePointer, 1);

			sSectors = (FF_T_UINT16) (pIoman->pPartition->SectorsPerCluster - (nRelClusterPos / pIoman->BlkSize));
			
			do {
				if(pIoman->pBlkDevice->fnReadBlocks) {
#ifdef	FF_BLKDEV_USES_SEM
					FF_PendSemaphore(pFile->pIoman->pSemaphore);
#endif
					RetVal = pFile->pIoman->pBlkDevice->fnReadBlocks(buffer, nItemLBA, sSectors, pIoman->pBlkDevice->pParam);
#ifdef	FF_BLKDEV_USES_SEM
					FF_ReleaseSemaphore(pFile->pIoman->pSemaphore);
#endif
				}
				if(RetVal == FF_ERR_DRIVER_BUSY) {
					FF_Yield();
					FF_Sleep(FF_DRIVER_BUSY_SLEEP);
				}
			}while(RetVal == FF_ERR_DRIVER_BUSY);
			
			nBytesToRead		 = sSectors * pIoman->BlkSize;
			nBytes				-= nBytesToRead;
			buffer				+= nBytesToRead;
			nBytesRead			+= nBytesToRead;
			pFile->FilePointer	+= nBytesToRead;

		}

		//---------- Read Clusters
		if(nBytes >= nBytesPerCluster) {
			//----- Thanks to Christopher Clark of DigiPen Institute of Technology in Redmond, US adding this traversal check.
			nClusterDiff = FF_getClusterChainNumber(pFile->pIoman, pFile->FilePointer, 1) - pFile->CurrentCluster;
			if(nClusterDiff) {
				if(pFile->CurrentCluster < FF_getClusterChainNumber(pFile->pIoman, pFile->FilePointer, 1)) {
					pFile->AddrCurrentCluster = FF_TraverseFAT(pIoman, pFile->AddrCurrentCluster, nClusterDiff);
					pFile->CurrentCluster += nClusterDiff;
				}
			}
			//----- End of Contributor fix.

			FF_ReadClusters(pFile, (nBytes / nBytesPerCluster), buffer);
			nBytesToRead = (nBytesPerCluster *  (nBytes / nBytesPerCluster));

			pFile->FilePointer	+= nBytesToRead;

			nBytes			-= nBytesToRead;
			buffer			+= nBytesToRead;
			nBytesRead		+= nBytesToRead;
		}

		//---------- Read Remaining Blocks
		if(nBytes >= pIoman->BlkSize) {
			sSectors = (FF_T_UINT16) (nBytes / pIoman->BlkSize);
			
			nClusterDiff = FF_getClusterChainNumber(pFile->pIoman, pFile->FilePointer, 1) - pFile->CurrentCluster;
			if(nClusterDiff) {
				if(pFile->CurrentCluster < FF_getClusterChainNumber(pFile->pIoman, pFile->FilePointer, 1)) {
					pFile->AddrCurrentCluster = FF_TraverseFAT(pIoman, pFile->AddrCurrentCluster, nClusterDiff);
					pFile->CurrentCluster += nClusterDiff;
				}
			}
			
			nItemLBA = FF_Cluster2LBA(pIoman, pFile->AddrCurrentCluster);
			nItemLBA = FF_getRealLBA(pIoman, nItemLBA + FF_getMajorBlockNumber(pIoman, pFile->FilePointer, 1)) + FF_getMinorBlockNumber(pIoman, pFile->FilePointer, 1);
			
			do {
				if(pIoman->pBlkDevice->fnReadBlocks) {
#ifdef	FF_BLKDEV_USES_SEM
					FF_PendSemaphore(pFile->pIoman->pSemaphore);
#endif
					RetVal = pFile->pIoman->pBlkDevice->fnReadBlocks(buffer, nItemLBA, sSectors, pIoman->pBlkDevice->pParam);
#ifdef	FF_BLKDEV_USES_SEM
					FF_ReleaseSemaphore(pFile->pIoman->pSemaphore);
#endif
				}
				if(RetVal == FF_ERR_DRIVER_BUSY) {
					FF_Yield();
					FF_Sleep(FF_DRIVER_BUSY_SLEEP);
				}
			}while(RetVal == FF_ERR_DRIVER_BUSY);
			
			nBytesToRead = sSectors * pIoman->BlkSize;
			pFile->FilePointer	+= nBytesToRead;
			nBytes				-= nBytesToRead;
			buffer				+= nBytesToRead;
			nBytesRead			+= nBytesToRead;
		}

		//---------- Read (memcpy) Remaining Bytes
		if(nBytes > 0) {
			
			nClusterDiff = FF_getClusterChainNumber(pFile->pIoman, pFile->FilePointer, 1) - pFile->CurrentCluster;
			if(nClusterDiff) {
				if(pFile->CurrentCluster < FF_getClusterChainNumber(pFile->pIoman, pFile->FilePointer, 1)) {
					pFile->AddrCurrentCluster = FF_TraverseFAT(pIoman, pFile->AddrCurrentCluster, nClusterDiff);
					pFile->CurrentCluster += nClusterDiff;
				}
			}
			
			nItemLBA = FF_Cluster2LBA(pIoman, pFile->AddrCurrentCluster);
			nItemLBA = FF_getRealLBA(pIoman, nItemLBA + FF_getMajorBlockNumber(pIoman, pFile->FilePointer, 1)) + FF_getMinorBlockNumber(pIoman, pFile->FilePointer, 1);
			pBuffer = FF_GetBuffer(pIoman, nItemLBA, FF_MODE_READ);
			{
				memcpy(buffer, pBuffer->pBuffer, nBytes);
			}
			FF_ReleaseBuffer(pIoman, pBuffer);

			nBytesToRead = nBytes;
			pFile->FilePointer	+= nBytesToRead;
			nBytes				-= nBytesToRead;
			buffer				+= nBytesToRead;
			nBytesRead			+= nBytesToRead;

		}
	}

	return nBytesRead;
}




/**
 *	@public
 *	@brief	Equivalent to fgetc()
 *
 *	@param	pFile		FF_FILE object that was created by FF_Open().
 *
 *	@return The character that was read (cast as a 32-bit interger). -1 on EOF.
 *	@return -2 If a null file pointer was provided.
 *	@return -3 Device access failed.
 *
 **/
FF_T_SINT32 FF_GetC(FF_FILE *pFile) {
	FF_T_UINT32		fileLBA;
	FF_BUFFER		*pBuffer;
	FF_T_UINT8		retChar;
	FF_T_UINT32		relMinorBlockPos;
	FF_T_UINT32     clusterNum;
	FF_T_UINT32		nClusterDiff;
	
	
	if(!pFile) {
		return FF_ERR_NULL_POINTER;
	}

	if(!(pFile->Mode & FF_MODE_READ)) {
		return FF_ERR_FILE_NOT_OPENED_IN_READ_MODE;
	}
	
	if(pFile->FilePointer >= pFile->Filesize) {
		return -1; // EOF!	
	}

	relMinorBlockPos	= FF_getMinorBlockEntry(pFile->pIoman, pFile->FilePointer, 1);
	clusterNum			= FF_getClusterChainNumber(pFile->pIoman, pFile->FilePointer, 1);

	nClusterDiff = FF_getClusterChainNumber(pFile->pIoman, pFile->FilePointer, 1) - pFile->CurrentCluster;
	if(nClusterDiff) {
		if(pFile->CurrentCluster < FF_getClusterChainNumber(pFile->pIoman, pFile->FilePointer, 1)) {
			pFile->AddrCurrentCluster = FF_TraverseFAT(pFile->pIoman, pFile->AddrCurrentCluster, nClusterDiff);
			pFile->CurrentCluster += nClusterDiff;
		}
	}
	

	fileLBA = FF_Cluster2LBA(pFile->pIoman, pFile->AddrCurrentCluster)	+ FF_getMajorBlockNumber(pFile->pIoman, pFile->FilePointer, (FF_T_UINT16) 1);
	fileLBA = FF_getRealLBA (pFile->pIoman, fileLBA)		+ FF_getMinorBlockNumber(pFile->pIoman, pFile->FilePointer, (FF_T_UINT16) 1);
	
	pBuffer = FF_GetBuffer(pFile->pIoman, fileLBA, FF_MODE_READ);
	{
		if(!pBuffer) {
			return -3;
		}
		retChar = pBuffer->pBuffer[relMinorBlockPos];
	}
	FF_ReleaseBuffer(pFile->pIoman, pBuffer);

	pFile->FilePointer += 1;

	return (FF_T_INT32) retChar;
}

FF_T_UINT32 FF_Tell(FF_FILE *pFile) {
	return pFile->FilePointer;
}


/**
 *	@public
 *	@brief	Writes data to a File.
 *
 *	@param	pFile			FILE Pointer.
 *	@param	ElementSize		Size of an Element of Data to be copied. (in bytes). 
 *	@param	Count			Number of Elements of Data to be copied. (ElementSize * Count must not exceed ((2^31)-1) bytes. (2GB). For best performance, multiples of 512 bytes or Cluster sizes are best.
 *	@param	buffer			Byte-wise buffer containing the data to be written.
 *
 *	@return
 **/
FF_T_SINT32 FF_Write(FF_FILE *pFile, FF_T_UINT32 ElementSize, FF_T_UINT32 Count, FF_T_UINT8 *buffer) {
	FF_T_UINT32 nBytes = ElementSize * Count;
	FF_T_UINT32	nBytesWritten = 0;
	FF_T_UINT32 nBytesToWrite;
	FF_IOMAN	*pIoman;
	FF_BUFFER	*pBuffer;
	FF_T_UINT32 nRelBlockPos;
	FF_T_UINT32	nItemLBA;
	FF_T_SINT32	RetVal = 0;
	FF_T_UINT16	sSectors;
	FF_T_UINT32 nRelClusterPos;
	FF_T_UINT32 nBytesPerCluster, nClusterDiff, nClusters;
	FF_ERROR	Error;

	if(!pFile) {
		return FF_ERR_NULL_POINTER;
	}

	if(!(pFile->Mode & FF_MODE_WRITE)) {
		return FF_ERR_FILE_NOT_OPENED_IN_WRITE_MODE;
	}

	// Make sure a write is after the append point.
	if((pFile->Mode & FF_MODE_APPEND)) {
		if(pFile->FilePointer < pFile->Filesize) {
			FF_Seek(pFile, 0, FF_SEEK_END);
		}
	}

	pIoman = pFile->pIoman;

	nBytesPerCluster = (pIoman->pPartition->SectorsPerCluster * pIoman->BlkSize);

	// Extend File for atleast nBytes!
	// Handle file-space allocation
	Error = FF_ExtendFile(pFile, pFile->FilePointer + nBytes);

	if(Error) {
		return Error;	
	}

	nRelBlockPos = FF_getMinorBlockEntry(pIoman, pFile->FilePointer, 1); // Get the position within a block.
	
	nClusterDiff = FF_getClusterChainNumber(pFile->pIoman, pFile->FilePointer, 1) - pFile->CurrentCluster;
	if(nClusterDiff) {
		if(pFile->CurrentCluster != FF_getClusterChainNumber(pFile->pIoman, pFile->FilePointer, 1)) {
			pFile->AddrCurrentCluster = FF_TraverseFAT(pIoman, pFile->AddrCurrentCluster, nClusterDiff);
			pFile->CurrentCluster += nClusterDiff;
		}
	}
	
	nItemLBA = FF_Cluster2LBA(pIoman, pFile->AddrCurrentCluster);
	nItemLBA = FF_getRealLBA(pIoman, nItemLBA + FF_getMajorBlockNumber(pIoman, pFile->FilePointer, 1)) + FF_getMinorBlockNumber(pIoman, pFile->FilePointer, 1);

	if((nRelBlockPos + nBytes) < pIoman->BlkSize) {	// Bytes to read are within a block and less than a block size.
		pBuffer = FF_GetBuffer(pIoman, nItemLBA, FF_MODE_WRITE);
		{
			memcpy((pBuffer->pBuffer + nRelBlockPos), buffer, nBytes);
		}
		FF_ReleaseBuffer(pIoman, pBuffer);

		pFile->FilePointer += nBytes;
		nBytesWritten = nBytes;
		//return nBytes;		// Return the number of bytes read.

	} else {

		//---------- Write (memcpy) to a Sector Boundary
		if(nRelBlockPos != 0) {	// Not on a sector boundary, at this point the LBA is known.
			nBytesToWrite = pIoman->BlkSize - nRelBlockPos;
			pBuffer = FF_GetBuffer(pIoman, nItemLBA, FF_MODE_WRITE);
			{
				// Here we copy to the sector boudary.
				memcpy((pBuffer->pBuffer + nRelBlockPos), buffer, nBytesToWrite);
			}
			FF_ReleaseBuffer(pIoman, pBuffer);

			nBytes				-= nBytesToWrite;
			nBytesWritten		+= nBytesToWrite;
			pFile->FilePointer	+= nBytesToWrite;
			buffer				+= nBytesToWrite;
		}

		//---------- Write to a Cluster Boundary
		
		nRelClusterPos = FF_getClusterPosition(pIoman, pFile->FilePointer, 1);
		if(nRelClusterPos != 0 && nBytes >= nBytesPerCluster) { // Need to get to cluster boundary
			
			nClusterDiff = FF_getClusterChainNumber(pFile->pIoman, pFile->FilePointer, 1) - pFile->CurrentCluster;
			if(nClusterDiff) {
				if(pFile->CurrentCluster < FF_getClusterChainNumber(pFile->pIoman, pFile->FilePointer, 1)) {
					pFile->AddrCurrentCluster = FF_TraverseFAT(pIoman, pFile->AddrCurrentCluster, nClusterDiff);
					pFile->CurrentCluster += nClusterDiff;
				}
			}
		
			nItemLBA = FF_Cluster2LBA(pIoman, pFile->AddrCurrentCluster);
			nItemLBA = FF_getRealLBA(pIoman, nItemLBA + FF_getMajorBlockNumber(pIoman, pFile->FilePointer, 1)) + FF_getMinorBlockNumber(pIoman, pFile->FilePointer, 1);

			sSectors = (FF_T_UINT16) (pIoman->pPartition->SectorsPerCluster - (nRelClusterPos / pIoman->BlkSize));
			
			do {
				if(pIoman->pBlkDevice->fnWriteBlocks) {
#ifdef	FF_BLKDEV_USES_SEM
					FF_PendSemaphore(pFile->pIoman->pSemaphore);
#endif
					RetVal = pFile->pIoman->pBlkDevice->fnWriteBlocks(buffer, nItemLBA, sSectors, pIoman->pBlkDevice->pParam);
#ifdef	FF_BLKDEV_USES_SEM
					FF_ReleaseSemaphore(pFile->pIoman->pSemaphore);
#endif
				}
				if(RetVal == FF_ERR_DRIVER_BUSY) {
					FF_Yield();
					FF_Sleep(FF_DRIVER_BUSY_SLEEP);
				}
			}while(RetVal == FF_ERR_DRIVER_BUSY);
			
			nBytesToWrite		 = sSectors * pIoman->BlkSize;
			nBytes				-= nBytesToWrite;
			buffer				+= nBytesToWrite;
			nBytesWritten		+= nBytesToWrite;
			pFile->FilePointer	+= nBytesToWrite;

		}

		//---------- Write Clusters
		if(nBytes >= nBytesPerCluster) {
			//----- Thanks to Christopher Clark of DigiPen Institute of Technology in Redmond, US adding this traversal check.
			nClusterDiff = FF_getClusterChainNumber(pFile->pIoman, pFile->FilePointer, 1) - pFile->CurrentCluster;
			if(nClusterDiff) {
				if(pFile->CurrentCluster < FF_getClusterChainNumber(pFile->pIoman, pFile->FilePointer, 1)) {
					pFile->AddrCurrentCluster = FF_TraverseFAT(pIoman, pFile->AddrCurrentCluster, nClusterDiff);
					pFile->CurrentCluster += nClusterDiff;
				}
			}
			//----- End of Contributor fix.

			nClusters = (nBytes / nBytesPerCluster);
			
			FF_WriteClusters(pFile, nClusters, buffer);
			
			nBytesToWrite = (nBytesPerCluster *  nClusters);
			
			pFile->FilePointer	+= nBytesToWrite;

			nBytes				-= nBytesToWrite;
			buffer				+= nBytesToWrite;
			nBytesWritten		+= nBytesToWrite;
		}

		//---------- Write Remaining Blocks
		if(nBytes >= pIoman->BlkSize) {
			sSectors = (FF_T_UINT16) (nBytes / pIoman->BlkSize);
			
			nClusterDiff = FF_getClusterChainNumber(pFile->pIoman, pFile->FilePointer, 1) - pFile->CurrentCluster;
			if(nClusterDiff) {
				if(pFile->CurrentCluster < FF_getClusterChainNumber(pFile->pIoman, pFile->FilePointer, 1)) {
					pFile->AddrCurrentCluster = FF_TraverseFAT(pIoman, pFile->AddrCurrentCluster, nClusterDiff);
					pFile->CurrentCluster += nClusterDiff;
				}
			}			
			
			nItemLBA = FF_Cluster2LBA(pIoman, pFile->AddrCurrentCluster);
			nItemLBA = FF_getRealLBA(pIoman, nItemLBA + FF_getMajorBlockNumber(pIoman, pFile->FilePointer, 1)) + FF_getMinorBlockNumber(pIoman, pFile->FilePointer, 1);
			
			do {
				if(pIoman->pBlkDevice->fnWriteBlocks) {
#ifdef	FF_BLKDEV_USES_SEM
					FF_PendSemaphore(pFile->pIoman->pSemaphore);
#endif
					RetVal = pFile->pIoman->pBlkDevice->fnWriteBlocks(buffer, nItemLBA, sSectors, pIoman->pBlkDevice->pParam);
#ifdef	FF_BLKDEV_USES_SEM
					FF_ReleaseSemaphore(pFile->pIoman->pSemaphore);
#endif
				}
				if(RetVal == FF_ERR_DRIVER_BUSY) {
					FF_Yield();
					FF_Sleep(FF_DRIVER_BUSY_SLEEP);
				}
			}while(RetVal == FF_ERR_DRIVER_BUSY);
			
			nBytesToWrite = sSectors * pIoman->BlkSize;
			pFile->FilePointer	+= nBytesToWrite;
			nBytes				-= nBytesToWrite;
			buffer				+= nBytesToWrite;
			nBytesWritten		+= nBytesToWrite;

		}

		//---------- Write (memcpy) Remaining Bytes
		if(nBytes > 0) {
			
			nClusterDiff = FF_getClusterChainNumber(pFile->pIoman, pFile->FilePointer, 1) - pFile->CurrentCluster;
			if(nClusterDiff) {
				if(pFile->CurrentCluster < FF_getClusterChainNumber(pFile->pIoman, pFile->FilePointer, 1)) {
					pFile->AddrCurrentCluster = FF_TraverseFAT(pIoman, pFile->AddrCurrentCluster, nClusterDiff);
					pFile->CurrentCluster += nClusterDiff;
				}
			}
			
			nItemLBA = FF_Cluster2LBA(pIoman, pFile->AddrCurrentCluster);
			nItemLBA = FF_getRealLBA(pIoman, nItemLBA + FF_getMajorBlockNumber(pIoman, pFile->FilePointer, 1)) + FF_getMinorBlockNumber(pIoman, pFile->FilePointer, 1);
			pBuffer = FF_GetBuffer(pIoman, nItemLBA, FF_MODE_WRITE);
			{
				memcpy(pBuffer->pBuffer, buffer, nBytes);
			}
			FF_ReleaseBuffer(pIoman, pBuffer);

			nBytesToWrite = nBytes;
			pFile->FilePointer	+= nBytesToWrite;
			nBytes				-= nBytesToWrite;
			buffer				+= nBytesToWrite;
			nBytesWritten			+= nBytesToWrite;

		}
	}

	if(pFile->FilePointer > pFile->Filesize) {
		pFile->Filesize = pFile->FilePointer;
	}

	return nBytesWritten;
}


/**
 *	@public
 *	@brief	Writes a char to a FILE.
 *
 *	@param	pFile		FILE Pointer.
 *	@param	pa_cValue	Char to be placed in the file.
 *
 *	@return	Returns the value written to the file, or a value less than 0.
 *
 **/
FF_T_SINT32 FF_PutC(FF_FILE *pFile, FF_T_UINT8 pa_cValue) {
	FF_BUFFER	*pBuffer;
	FF_T_UINT32 iItemLBA;
	FF_T_UINT32 iRelPos;
	FF_T_UINT32 nClusterDiff;
	
	if(!pFile) {	// Ensure we don't have a Null file pointer on a Public interface.
		return FF_ERR_NULL_POINTER;
	}

	if(!(pFile->Mode & FF_MODE_WRITE)) {
		return FF_ERR_FILE_NOT_OPENED_IN_WRITE_MODE;
	}

	// Make sure a write is after the append point.
	if((pFile->Mode & FF_MODE_APPEND)) {
		if(pFile->FilePointer < pFile->Filesize) {
			FF_Seek(pFile, 0, FF_SEEK_END);
		}
	}

	iRelPos = FF_getMinorBlockEntry(pFile->pIoman, pFile->FilePointer, 1);
	
	// Handle File Space Allocation.
	FF_ExtendFile(pFile, pFile->FilePointer + 1);
	
	nClusterDiff = FF_getClusterChainNumber(pFile->pIoman, pFile->FilePointer, 1) - pFile->CurrentCluster;
	if(nClusterDiff) {
		if(pFile->CurrentCluster < FF_getClusterChainNumber(pFile->pIoman, pFile->FilePointer, 1)) {
			pFile->AddrCurrentCluster = FF_TraverseFAT(pFile->pIoman, pFile->AddrCurrentCluster, nClusterDiff);
			pFile->CurrentCluster += nClusterDiff;
		}
	}

	iItemLBA = FF_Cluster2LBA(pFile->pIoman, pFile->AddrCurrentCluster) + FF_getMajorBlockNumber(pFile->pIoman, pFile->FilePointer, (FF_T_UINT16) 1);
	iItemLBA = FF_getRealLBA (pFile->pIoman, iItemLBA)			+ FF_getMinorBlockNumber(pFile->pIoman, pFile->FilePointer, (FF_T_UINT16) 1);
	
	pBuffer = FF_GetBuffer(pFile->pIoman, iItemLBA, FF_MODE_WRITE);
	{
		if(!pBuffer) {
			return FF_ERR_DEVICE_DRIVER_FAILED;
		}
		FF_putChar(pBuffer->pBuffer, (FF_T_UINT16) iRelPos, pa_cValue);
	}
	FF_ReleaseBuffer(pFile->pIoman, pBuffer);

	pFile->FilePointer += 1;
	if(pFile->Filesize < (pFile->FilePointer)) {
		pFile->Filesize += 1;
	}
	return pa_cValue;
}



/**
 *	@public
 *	@brief	Equivalent to fseek()
 *
 *	@param	pFile		FF_FILE object that was created by FF_Open().
 *	@param	Offset		An integer (+/-) to seek to, from the specified origin.
 *	@param	Origin		Where to seek from. (FF_SEEK_SET seek from start, FF_SEEK_CUR seek from current position, or FF_SEEK_END seek from end of file).
 *
 *	@return 0 on Sucess, 
 *	@return -2 if offset results in an invalid position in the file. 
 *	@return -1 if a FF_FILE pointer was not recieved.
 *	@return -3 if an invalid origin was provided.
 *	
 **/
FF_ERROR FF_Seek(FF_FILE *pFile, FF_T_SINT32 Offset, FF_T_INT8 Origin) {
	
	if(!pFile) {
		return FF_ERR_NULL_POINTER;
	}

	switch(Origin) {
		case FF_SEEK_SET:
			if((FF_T_UINT32) Offset <= pFile->Filesize && Offset >= 0) {
				pFile->FilePointer = Offset;
				pFile->CurrentCluster = FF_getClusterChainNumber(pFile->pIoman, pFile->FilePointer, 1);
				pFile->AddrCurrentCluster = FF_TraverseFAT(pFile->pIoman, pFile->ObjectCluster, pFile->CurrentCluster);
			} else {
				return -2;
			}
			break;

		case FF_SEEK_CUR:
			if((Offset + pFile->FilePointer) <= pFile->Filesize && (Offset + (FF_T_SINT32) pFile->FilePointer) >= 0) {
				pFile->FilePointer = Offset + pFile->FilePointer;
				pFile->CurrentCluster = FF_getClusterChainNumber(pFile->pIoman, pFile->FilePointer, 1);
				pFile->AddrCurrentCluster = FF_TraverseFAT(pFile->pIoman, pFile->ObjectCluster, pFile->CurrentCluster);
			} else {
				return -2;
			}
			break;
	
		case FF_SEEK_END:
			if((Offset + (FF_T_SINT32) pFile->Filesize) >= 0 && (Offset + pFile->Filesize) <= pFile->Filesize) {
				pFile->FilePointer = Offset + pFile->Filesize;
				pFile->CurrentCluster = FF_getClusterChainNumber(pFile->pIoman, pFile->FilePointer, 1);
				pFile->AddrCurrentCluster = FF_TraverseFAT(pFile->pIoman, pFile->ObjectCluster, pFile->CurrentCluster);
			} else {
				return -2;
			}
			break;

		default:
			return -3;
		
	}

	return 0;
}


/**
 *	@public
 *	@brief	Equivalent to fclose()
 *
 *	@param	pFile		FF_FILE object that was created by FF_Open().
 *
 *	@return 0 on sucess.
 *	@return -1 if a null pointer was provided.
 *
 **/
FF_ERROR FF_Close(FF_FILE *pFile) {

	FF_FILE		*pFileChain;
	FF_DIRENT	OriginalEntry;
	FF_ERROR	Error;

	if(!pFile) {
		return FF_ERR_NULL_POINTER;	
	}
	// UpDate Dirent if File-size has changed?

	// Update the Dirent!
	Error = FF_GetEntry(pFile->pIoman, pFile->DirEntry, pFile->DirCluster, &OriginalEntry);
	if(Error) {
		return Error;
	}
	
	if(!pFile->FileDeleted) {
		if(pFile->Filesize != OriginalEntry.Filesize) {
			OriginalEntry.Filesize = pFile->Filesize;
			FF_PutEntry(pFile->pIoman, pFile->DirEntry, pFile->DirCluster, &OriginalEntry);
		}
	}

	//if(pFile->Mode == FF_MODE_WRITE) {
	FF_FlushCache(pFile->pIoman);		// Ensure all modfied blocks are flushed to disk!
	//}
	
	// Handle Linked list!
	FF_PendSemaphore(pFile->pIoman->pSemaphore);
	{	// Semaphore is required, or linked list could become corrupted.
		if(pFile->pIoman->FirstFile == pFile) {
			pFile->pIoman->FirstFile = pFile->Next;
		} else {
			pFileChain = (FF_FILE *) pFile->pIoman->FirstFile;
			while(pFileChain->Next != pFile) {
				pFileChain = pFileChain->Next;
			}
			pFileChain->Next = pFile->Next;
		}
	}	// Semaphore released, linked list was shortened!
	FF_ReleaseSemaphore(pFile->pIoman->pSemaphore);

	// If file written, flush to disk
	FF_FREE(pFile);
	// Simply free the pointer!
	return FF_ERR_NONE;
}
