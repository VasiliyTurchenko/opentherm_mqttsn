/** @file tiny-fs.c
 *  @brief Tiny filesystem
 *
 *  @author turchenkov@gmail.com
 *  @bug
 *  @date 26-Jan-2019
 *  @date 08-Mar-2019
 */

#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os.h"

#include "mutex_helpers.h"

#include <stddef.h>
//#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <limits.h>

#include "tiny-fs.h"
#include "crc32_helpers.h"

#define SILENT

#if defined(osCMSIS)

#define TINY_FS_LOCK                                                           \
	do {                                                                   \
		if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {   \
			taskENTER_CRITICAL();                                  \
		}                                                              \
	} while (0)
#define TINY_FS_UNLOCK                                                         \
	do {                                                                   \
		if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {   \
			taskEXIT_CRITICAL();                                   \
		}                                                              \
	} while (0)

#else

#define TINY_FS_LOCK
#define TINY_FS_UNLOCK

#endif

#ifndef MCU_TARGET

#endif

#if defined(STM32F103xB)

#define CRC32(BUF,SIZ,INI) CRC32_helper((BUF),(SIZ),(INI))

#elif defined(STM32F303xC)

#define CRC32(BUF,SIZ,INI) CRC32_helper((BUF),(SIZ),(INI))

#endif

NDEBUG_STATIC size_t getClusterTableOffset(void);

NDEBUG_STATIC uint32_t RenewDirCRC32(const Media_Desc_t * const media);
static size_t inline cluster2Addr(uint32_t cluster);
static size_t inline addr2Cluster(uint32_t addr);
static ErrorStatus inline toggleClusters(uint8_t *clusterTable, size_t by,
					 size_t bi, size_t reqClusters);
NDEBUG_STATIC size_t FindFreeSlot(const Media_Desc_t* const media);
NDEBUG_STATIC size_t SaveDirEntry(const Media_Desc_t* media, size_t entryIndex,
				  const DIR_Entry_p entry);

NDEBUG_STATIC void decodeFName(const char *name[MAX_FILENAME_LEN], char *sname);
NDEBUG_STATIC void encodeFName(char *name[MAX_FILENAME_LEN], const char *sname);
NDEBUG_STATIC size_t getDIR_EntryOffset(size_t fileIndex);
NDEBUG_STATIC ErrorStatus freeClusters(uint8_t *clusterTable,
				       const size_t clusterTableSize,
				       const size_t fileAddress,
				       const size_t fileSize);

NDEBUG_STATIC ErrorStatus readFAT(const Media_Desc_t * const media, uint8_t *buf);

NDEBUG_STATIC ErrorStatus writeFAT(const Media_Desc_t * media, uint8_t *buf);

NDEBUG_STATIC FRESULT isFileValid(FIL *fp);
NDEBUG_STATIC bool isPtrValid(const void *p);
NDEBUG_STATIC bool isMediaValid(const Media_Desc_t *media);
NDEBUG_STATIC uint32_t allocateClusters(uint8_t *clusterTable,
					size_t clusterTableSize,
					size_t requestedSize);
NDEBUG_STATIC uint32_t findEntry(const Media_Desc_t * const media,
				 const DIR_Entry_p entry);

static uint8_t DTA[FS_DTA_SIZE]; /* data trahsfer area for driver's needs */

/*  */

/* mutexes */
static uint32_t mutexes[FS_CONCURRENT_FILES];

extern osMutexId FS_Mutex01Handle;
extern osMutexId CRC_MutexHandle;

/**
 * @brief getClusterTableOffset
 * @return offset of cluster table in bytes
 */
NDEBUG_STATIC size_t getClusterTableOffset()
{
	return (sizeof(DIR_Entry_t) * DIR_ENTRIES + sizeof(FAT_Header_t));
}

/**
 * @brief getNumClusters returns number of the clusters
 * @param mediaSize size in bytes
 * @return number of clusters
 */
NDEBUG_STATIC size_t getNumClusters(const Media_Desc_t * const media)
{
	size_t tot_clu =
		(media->MediaSize - getClusterTableOffset()) / FS_CLUSTER_SIZE;
	return (tot_clu - (tot_clu % CHAR_BIT));
}

/**
 * @brief getClusterFileSize returns cluster file size in bytes
 * @param clusterTableSize in bytes
 * @return clusterFileSize in bytes
 */
size_t getClusterFileSize(size_t clusterTableSize)
{
	size_t clusterFileSize = clusterTableSize / FS_CLUSTER_SIZE;
	if ((clusterTableSize % FS_CLUSTER_SIZE) != 0) {
		clusterFileSize++;
	}
	clusterFileSize *= FS_CLUSTER_SIZE;
	return clusterFileSize;
}

/**
 * @brief getClusterTableSize returns size in bytes
 * @param media
 * @return size
 */
NDEBUG_STATIC size_t getClusterTableSize(const Media_Desc_t * const media)
{
	size_t numClusters = getNumClusters(media);
	uint8_t add = ((numClusters % CHAR_BIT) != 0) ? 1U : 0U;
	return ((numClusters / CHAR_BIT) + add);
}

/**
 * @brief InitFS initializes internal structures
 */
void InitFS(void)
{
	for (size_t i = 0U; i < FS_CONCURRENT_FILES; i++) {
		mutexes[i] = 0U;
	}
	memset(DTA, 0U, FS_DTA_SIZE);
}

/**
 * @brief Format
 * @param media
 * @return
 */
ErrorStatus Format(const Media_Desc_t * const media)
{
	ErrorStatus retVal = ERROR;

	/* take mutex */
	TAKE_MUTEX(FS_Mutex01Handle);

	if (isMediaValid(media) == false) {
		goto fExit;
	}

	retVal = SUCCESS;
#ifdef DEBUG
#ifndef SILENT
	xprintf("Format. Reserved area size = %d bytes \n", RESERVED_AREA);
#endif
#endif
	FAT_Begin_p f_begin = (FAT_Begin_p)DTA;
	DIR_Entry_p p = (DIR_Entry_p)(&f_begin->entry0);

	for (size_t i = 0U; i < DIR_ENTRIES; i++) {
		memset(p->FileName, 0, MAX_FILENAME_LEN + 1U);
		p->FileSize = 0x00U;
		p->FileCRC32 = 0x00U;
		p->FileStatus = FStateNoFile;
		p->FileAddress = 0x00U;
		/* save initialized FAT entry */
		size_t mediaAddr = getDIR_EntryOffset(i);
#ifdef DEBUG
#ifndef SILENT
		xprintf("Format. Writing %d bytes to address %d\n",
		       sizeof(DIR_Entry_t), mediaAddr);
#endif
#endif
		retVal = media->writeFunc((uint8_t *)p, mediaAddr,
					  sizeof(DIR_Entry_t));
		if (retVal != SUCCESS) {
			break;
		}
	}
	if (retVal != SUCCESS) {
		goto fExit;
	}
	/* initialize cluster table */
	size_t clusterTableSize = getClusterTableSize(media);
#ifdef DEBUG
#ifndef SILENT
	const size_t numClusters = getNumClusters(media);
	xprintf("Format. Number of clusters = %d; cluster size = %d bytes; clusterTableSize =  %d bytes\n",
	       numClusters, FS_CLUSTER_SIZE, clusterTableSize);
#endif
#endif
	/* the first file is always cluster table */
	const char *FATFile = "$$FAT$$";
	memcpy(p->FileName, FATFile, strlen(FATFile));
	p->FileSize = (uint16_t)clusterTableSize;
	p->FileStatus = FStateFAT;
	p->FileAddress = getClusterTableOffset();
	/* write DIR entry of the "$$FAT$$" file */
	retVal = media->writeFunc((uint8_t *)p, offsetof(FAT_Begin_t, entry0),
				  sizeof(DIR_Entry_t));
	if (retVal != SUCCESS) {
		goto fExit;
	}
	/* prepare cluster table image */
	size_t clusterFileSize = getClusterFileSize(clusterTableSize);

#ifdef DEBUG
#ifndef SILENT
	xprintf("Format. clusterFileSize = %d\n", clusterFileSize);
#endif
#endif
	/** @todo Add check DTA size against clusterFileSize */

	if (clusterTableSize > FS_DTA_SIZE) {
#ifdef DEBUG
#ifndef SILENT
		xprintf("panic at line %d", __LINE__);
#endif
#endif

#ifndef MCU_TARGET
		exit(-1);
#else
		clusterTableSize = FS_DTA_SIZE;
#endif
	}

	memset(DTA, 0, clusterTableSize); /* clear table of clusters */
	/* allocate clusters and mark them as busy in the cluster table */
	size_t FATFileOffset =
		allocateClusters(DTA, clusterTableSize, clusterFileSize);
	if (FATFileOffset != 0U) {
#ifdef DEBUG
#ifndef SILENT
		printf("panic at line %d", __LINE__);
#endif
#endif

#ifndef MCU_TARGET
		exit(-1);
#else
	FATFileOffset = 0U;
#endif

	}
	/* save cluster table */
	retVal = media->writeFunc(DTA, getClusterTableOffset(),
				  clusterTableSize);
	if (retVal != SUCCESS) {
		goto fExit;
	}
	TAKE_MUTEX(CRC_MutexHandle);
	uint32_t cTable_CRC = CRC32(DTA, clusterTableSize, 0x00U);
	GIVE_MUTEX(CRC_MutexHandle);
	size_t CRCOffset = offsetof(FAT_Begin_t, entry0);
	CRCOffset += offsetof(DIR_Entry_t, FileCRC32);
	retVal = media->writeFunc((uint8_t *)&cTable_CRC, CRCOffset,
				  sizeof(uint32_t));
	if (retVal != SUCCESS) {
		goto fExit;
	}

	/* write FAT header */
	f_begin->h.FS_version = FS_VERSION;
	f_begin->h.DIR_CRC32 = 0x00U;
	f_begin->h.FAT_ClusterTableSize = clusterTableSize;

#ifdef DEBUG
#ifndef SILENT
	xprintf("Format. Witing %d bytes to address %d\n", sizeof(FAT_Header_t),
	       0U);
#endif
#endif
	retVal = media->writeFunc((uint8_t *)f_begin, 0U, sizeof(FAT_Header_t));

	f_begin->h.DIR_CRC32 = RenewDirCRC32(media);
	if (f_begin->h.DIR_CRC32 == UINT32_MAX) {
		retVal = ERROR;
	}

#ifdef DEBUG
#ifndef SILENT
	xprintf("Format. DIR_CRC32:%08x\n", f_begin->h.DIR_CRC32);
#endif
#endif

fExit:
	GIVE_MUTEX(FS_Mutex01Handle);
	return retVal;
}

/**
 * @brief f_open FAT FS similar fubction call
 * @param fp pointer to the file struct
 * @param path name of the file
 * @param mode of opening the file
 * @return FRESULT
 */
FRESULT f_open(FIL *fp, const TCHAR *path, BYTE mode)
{
	return NewFile(fp, path, 0U, (fMode_t)mode);
}

// once created, file cannot change its clusters allocation
/**
 * @brief NewFile creates new or opens existing file
 * @param file pointer to the file struct
 * @param name name of the file
 * @param size  requested file size if new file requested
 * @param mode  of opening the file
 * @return FRESULT
 */
FRESULT NewFile(fHandle_p file, const char *name, size_t size, fMode_t mode)
{
	FRESULT retVal = FR_INVALID_PARAMETER;

	TAKE_MUTEX(FS_Mutex01Handle);

	if ((file == NULL) || (file->media == NULL) || (name == NULL) ||
	    ((mode != FModeRead) && (mode != FModeWrite))) {
		return retVal;
	}

	TINY_FS_LOCK;
	size_t i = 0;
	for (i = 0U; i < FS_CONCURRENT_FILES; i++) {
		if (mutexes[i] != 0U) {
			continue;
		} else {
			mutexes[i] = 1U;
			break;
		}
	}
	TINY_FS_UNLOCK;
	if (i == FS_CONCURRENT_FILES) {
		retVal = FR_TOO_MANY_OPEN_FILES;
		goto fExit;
	}
	DIR_Entry_t tmp_entry;
	/* read the cluster file */
	if (file->media->readFunc((uint8_t *)&tmp_entry,
				  offsetof(FAT_Begin_t, entry0),
				  sizeof(DIR_Entry_t)) == ERROR) {
		/* read error */
		retVal = FR_DISK_ERR;
		goto fExit;
	}

	if (file->media->readFunc(DTA, tmp_entry.FileAddress,
				  tmp_entry.FileSize) == ERROR) {
		/* read error */
		retVal = FR_DISK_ERR;
		goto fExit;
	}
	/* DTA now contains cluster table */

	/* look up the FAT for the file name */

	size_t entry_index; /* index of found FAT entry */
	strncpy((char *)&file->fileDir.FileName, name, MAX_FILENAME_LEN);
	file->fileDir.FileName[MAX_FILENAME_LEN] = '\0';

	file->fileDir.FileSize = (uint16_t)size;

	entry_index = findEntry(file->media, &file->fileDir);
	if (entry_index == UINT32_MAX) {
		/* read error */
		retVal = FR_DISK_ERR;
		goto fExit;
	}
	if (entry_index == 0U) {
		/* no file? create the new one */
		/*1. find free slot */
		entry_index = FindFreeSlot(file->media);
		if (entry_index == UINT32_MAX) {
			/* read error */
			retVal = FR_DISK_ERR;
			goto fExit;
		}
		/*2. reserve clusters */
		// TODO BUG HERE!
		file->fileDir.FileAddress =
			allocateClusters(DTA, getClusterTableSize(file->media),
					 file->fileDir.FileSize);
		/*
* @return 0 if no space needs to be allocated; UINT32_MAX in case of no room or error;
* @return start address of the media to write data
*/
		if (file->fileDir.FileAddress == UINT32_MAX) {
			/* I/O error */
			retVal = FR_DISK_ERR;
			goto fExit;
		}

		/* Update FAT */

		if (file->media->writeFunc(
			    (uint8_t *)&DTA, getClusterTableOffset(),
			    getClusterTableSize(file->media)) == ERROR) {
			/* I/O error */
			retVal = FR_DISK_ERR;
			goto fExit;
		}

		file->fileDir.FileAddress += getClusterTableOffset();
		file->filePtr = 0U;
		file->fileDir.FileCRC32 = 0U;

		if (mode == FModeRead) {
			file->fileDir.FileStatus = FStateOpenedR;
		} else {
			file->fileDir.FileStatus = FStateOpenedW;
		}
		/*
		retVal = FR_OK;

		if (SaveDirEntry(file->media, entry_index, &file->fileDir) != entry_index) {
			retVal = FR_DISK_ERR;
			goto fExit;
		}
*/
	} else {
		/* open existing */
		size_t dirEntryOffset = getDIR_EntryOffset(entry_index);
		if (file->media->readFunc((uint8_t *)&file->fileDir,
					  dirEntryOffset,
					  sizeof(DIR_Entry_t)) != SUCCESS) {
			retVal = FR_DISK_ERR;
			goto fExit;
		}

		/* 1. check is file not locked */
		if (file->fileDir.FileStatus != FStateClosed) {
			retVal = FR_LOCKED;
			goto fExit;
		}
		if (mode == FModeRead) {
			file->fileDir.FileStatus = FStateOpenedR;
		} else {
			file->fileDir.FileStatus = FStateOpenedW;
		}
		/* 2. init file pointer */
		file->filePtr = 0U;
		/* 3. exit */
		/*
		retVal = FR_OK;
*/
	}
	if (SaveDirEntry(file->media, entry_index, &file->fileDir) !=
	    entry_index) {
		retVal = FR_DISK_ERR;
	} else {
		retVal = FR_OK;
	}

fExit:
	if (retVal != FR_OK) {
		mutexes[i] = 0U;
	} else {
		file->pmutex = &mutexes[i];
	}

	GIVE_MUTEX(FS_Mutex01Handle);

	return retVal;
}

/**
 * @brief f_close FAT_FS similar function call
 * @param fp pointer to the file handle
 * @return FRESULT
 */
FRESULT f_close(FIL *fp)
{
	return CloseFile(fp);
}

/**
 * @brief CloseFile tries to close opened file
 * @param file pointer to file handle
 * @return FRESULT
 */
FRESULT CloseFile(fHandle_p file)
{
	TAKE_MUTEX(FS_Mutex01Handle);

	FRESULT retVal;
	retVal = isFileValid(file);
	if (retVal != FR_OK) {
		goto fExit;
	}

	/* check if file exists */
	size_t fileIndex;
	fileIndex = findEntry(file->media, &file->fileDir);
	if ((fileIndex == 0U) || (fileIndex == UINT32_MAX)) {
		retVal = FR_NO_FILE;
		goto fExit;
	}
	if ((file->fileDir.FileStatus != FStateOpenedR) &&
	    (file->fileDir.FileStatus != FStateOpenedW)) {
		retVal = FR_LOCKED;
		goto fExit;
	}

	DIR_Entry_t tmp_entry;
	size_t mediaAddr = getDIR_EntryOffset(fileIndex);
	if (file->media->readFunc((uint8_t *)&tmp_entry, mediaAddr,
				  sizeof(DIR_Entry_t)) == ERROR) {
		retVal = FR_DISK_ERR;
		goto fExit;
	}
	if (tmp_entry.FileStatus != file->fileDir.FileStatus) {
		retVal = FR_DENIED;
		goto fExit;
	}

	file->fileDir.FileStatus = FStateClosed;
	if (SaveDirEntry(file->media, fileIndex, &file->fileDir) ==
	    UINT32_MAX) {
		retVal = FR_DISK_ERR;
		goto fExit;
	}
	/* release handle */
	file->fileDir.FileStatus = FStateNoFile;
	*(file->pmutex) = 0U;
	file->pmutex = NULL;
	retVal = FR_OK;
	/* unlock */

fExit:
	GIVE_MUTEX(FS_Mutex01Handle);
	return retVal;
}

/**
 * @brief getDIR_EntryOffset returns offset of requested entry from the begin of the media
 * @param fileIndex
 * @return offset
 */
NDEBUG_STATIC size_t getDIR_EntryOffset(size_t fileIndex)
{
	const size_t fnameOffset = offsetof(DIR_Entry_t, FileName);
	const size_t FATOffset = offsetof(FAT_Begin_t, entry0);
	return (FATOffset + fnameOffset + fileIndex * sizeof(DIR_Entry_t));
}

/**
 * @brief f_unlink FAT_FS similar function call
 * @param path
 * @return FRESULT
 */
FRESULT f_unlink(const TCHAR *path)
{
	(void)path;
	return FR_INVALID_OBJECT;
}

// deletes a file
/**
 * @brief DeleteFile
 * @param media media description table
 * @param name name of the file
 * @return FRESULT
 */
FRESULT DeleteFile(const Media_Desc_t * media, const char *name)
{
	TAKE_MUTEX(FS_Mutex01Handle);

	FRESULT retVal;
	retVal = FR_INVALID_PARAMETER;

	if (isMediaValid(media) == false) {
		goto fExit;
	}
	if (isPtrValid(name) == false) {
		goto fExit;
	}

	/* look up the FAT for the file name */
	static DIR_Entry_t tmp_entry;
	size_t entry_index; /* index of found FAT entry */
	strncpy((char *)&tmp_entry.FileName, name, MAX_FILENAME_LEN);
	tmp_entry.FileName[MAX_FILENAME_LEN] = '\0';
	entry_index = findEntry(media, &tmp_entry);
	if ((entry_index == UINT32_MAX) || (entry_index == 0U)) {
		retVal = FR_NO_FILE;
		goto fExit;
	}
	size_t mediaAddr = getDIR_EntryOffset(entry_index);
	if (media->readFunc((uint8_t *)&tmp_entry, mediaAddr,
			    sizeof(DIR_Entry_t)) == ERROR) {
		retVal = FR_DISK_ERR;
		goto fExit;
	}
#if(0)
	if (tmp_entry.FileStatus != FStateClosed) {
		retVal = FR_LOCKED;
		goto fExit;
	}
#endif
	/* free clusters */
	if (readFAT(media, (uint8_t *)&DTA) == ERROR) {
		retVal = FR_DISK_ERR;
		goto fExit;
	}
	if (freeClusters((uint8_t *)&DTA, getClusterTableSize(media),
			 tmp_entry.FileAddress, tmp_entry.FileSize) == ERROR) {
		retVal = FR_DISK_ERR;
		goto fExit;
	}
	if (writeFAT(media, (uint8_t *)&DTA) == ERROR) {
		retVal = FR_DISK_ERR;
		goto fExit;
	}

	memset(&tmp_entry, 0x00U, sizeof(DIR_Entry_t));
	tmp_entry.FileStatus = FStateNoFile;
	if (SaveDirEntry(media, entry_index, &tmp_entry) == UINT32_MAX) {
		retVal = FR_DISK_ERR;
	} else {
		retVal = FR_OK;
	}
fExit:
	GIVE_MUTEX(FS_Mutex01Handle);

	return retVal;
}

/**
 * @brief freeClusters marks clusters as free
 * @param clusterTable pointer to the cluster table
 * @param clusterTableSize size of the table
 * @param fileAddress address of the file being deleted
 * @param fileSize size of the file being deleted
 * @return ERROR or SUCCESS
 */
NDEBUG_STATIC ErrorStatus freeClusters(uint8_t *clusterTable,
				       const size_t clusterTableSize,
				       const size_t fileAddress,
				       const size_t fileSize)
{
	ErrorStatus retVal;
	retVal = SUCCESS;
	if ((clusterTable == NULL) || (clusterTableSize == 0U) ||
	    (fileAddress < getClusterTableOffset())) {
		retVal = ERROR;
		goto fExit;
	}
	if (fileSize > 0U) {
		size_t reqClusters = fileSize / FS_CLUSTER_SIZE;
		if ((fileSize % FS_CLUSTER_SIZE) != 0) {
			reqClusters++;
		}

		if (reqClusters > (clusterTableSize * CHAR_BIT)) {
			retVal = ERROR;
			goto fExit;
		}
		uint32_t cl =
			addr2Cluster(fileAddress - getClusterTableOffset());
		uint32_t by = cl / CHAR_BIT;
		uint32_t bi = cl % CHAR_BIT;

		retVal = toggleClusters(clusterTable, by, bi, reqClusters);
	}
fExit:
	return retVal;
}

/**
 * @brief findEntry searches for requested entry in the FAT table on the media
 * @param media
 * @param entry
 * @return index of found file in the table; 0 if no file; UINT32_MAX in case of error
 */
NDEBUG_STATIC uint32_t findEntry(const Media_Desc_t* const media,
				 const DIR_Entry_p entry)
{
	uint32_t retVal = 0U;
	ErrorStatus mediaError = SUCCESS;

	const size_t fnameOffset = offsetof(DIR_Entry_t, FileName);
	char tmpFName[MAX_FILENAME_LEN + 1U];
	size_t i;
	for (i = 0U; i < DIR_ENTRIES; i++) {
		size_t mediaAddr = getDIR_EntryOffset(i) + fnameOffset;
#ifdef DEBUG
		//	printf("findEntry: read address %d\n", mediaAddr);
#endif
		mediaError = media->readFunc((uint8_t *)tmpFName, mediaAddr,
					     sizeof(tmpFName));
		if (mediaError != SUCCESS) {
			retVal = UINT32_MAX;
			break;
		}
		if (strcmp(tmpFName, entry->FileName) == 0) {
			retVal = i;
			break;
		}
	}
	if (mediaError == SUCCESS) {
		switch (retVal) {
		case (0U): {
#ifdef DEBUG
			//		printf("findEntry: entry not found\n");
#endif
			break;
		}
		default:
#ifdef DEBUG
			//		printf("findEntry: entry found: %d\n", retVal);
#endif
			break;
		}
	}
	return retVal;
}

/**
 * @brief findMaxFreeBlock returns maximal available contigous storage block
 * @param media
 * @return
 * @note TEST only!
 */
NDEBUG_STATIC size_t findMaxFreeBlock(const Media_Desc_t * media)
{
#ifdef DEBUG
	size_t retVal;
	retVal = 0U;
	static uint8_t locDTA[FS_DTA_SIZE];
	if (readFAT(media, locDTA) == SUCCESS) {
		size_t clusterTableSize = getClusterTableSize(media);

		size_t zeroes = 0U;
		size_t max_zeroes = 0U;
		size_t bi = 0U;
		size_t by = 0U;

		for (size_t i = 0U; i < clusterTableSize; i++) {
			uint8_t b = locDTA[i];
			for (size_t j = 0U; j < CHAR_BIT; j++) {
				if ((b & 0x01U) == 0U) {
					if (zeroes == 0U) {
						bi = j;
						by = i;
					}
					zeroes++;
					max_zeroes = (zeroes > max_zeroes) ? zeroes : max_zeroes;
				} else {
					zeroes = 0U;
					bi = 0U;
					by = 0U;
				}
				b = b >> 01U;
			}
		}

		retVal = max_zeroes * FS_CLUSTER_SIZE;
	}
	return retVal;
#else
	return UINT32_MAX;
#endif
}

/**
 * @brief allocateClusters allocates free clusters from the pool
 * @param clusterTable pointer to the cluster table (pool)
 * @param clusterTableSize size of the table
 * @param requestedSize bytes to be allocated from the pool
 * @return 0 if no space needs to be allocated; UINT32_MAX in case of no room or error;
 * @return start address of the media to write data
 */
NDEBUG_STATIC uint32_t allocateClusters(uint8_t *clusterTable,
					size_t clusterTableSize,
					size_t requestedSize)
{
	uint32_t retVal;
	retVal = UINT32_MAX;
	size_t bi = 0U;
	size_t by = 0U;
	size_t reqClusters = 0U;

	if ((clusterTable == NULL) || (clusterTableSize == 0U)) {
		goto fExit;
	}

	/*if zero bytes requested, allocate 1 cluster */
	requestedSize = (requestedSize == 0U) ? 1U : requestedSize;

	reqClusters = requestedSize / FS_CLUSTER_SIZE;
	if ((requestedSize % FS_CLUSTER_SIZE) != 0) {
		reqClusters++;
	}

	if (reqClusters > (clusterTableSize * CHAR_BIT)) {
		goto fExit;
	}

	/* we have to find a hole of zeroes which length >= reqClusters */
	/* scan for zeroes... */
	size_t zeroes = 0U;

	for (size_t i = 0U; i < clusterTableSize; i++) {
		uint8_t b = clusterTable[i];
		for (size_t j = 0U; j < CHAR_BIT; j++) {
			if ((b & 0x01U) == 0U) {
				if (zeroes == 0U) {
					bi = j;
					by = i;
				}
				zeroes++;
				if (zeroes == reqClusters) {
					retVal = cluster2Addr(by * CHAR_BIT +
							      bi);
					goto fExit;
				}
			} else {
				zeroes = 0U;
				bi = 0U;
				by = 0U;
			}
			b = b >> 01U;
		}
	}
fExit:
	/* mark allocated clusters as busy */
	if (retVal != UINT32_MAX) {
		if (toggleClusters(clusterTable, by, bi, reqClusters) !=
		    SUCCESS) {
			retVal = UINT32_MAX;
		}
	}
	return retVal;
}

/**
 * @brief cluster2Addr returns absolute address for given cluster #
 * @param cluster
 * @return address
 */
static size_t inline cluster2Addr(uint32_t cluster)
{
	return cluster * FS_CLUSTER_SIZE;
}

/**
 * @brief addr2Cluster returns # of cluster for the given address
 * @param addr
 * @return #of cluster
 */
static size_t inline addr2Cluster(uint32_t addr)
{
	return addr / FS_CLUSTER_SIZE;
}

/**
 * @brief toggleClusters toggles requested clusters (busy/free)
 * @param clusterTable pointer to table
 * @param by first byte
 * @param bi first bit
 * @param reqClusters #of clusters to mark
 * @return ERROR or SUCCESS
 */
static ErrorStatus inline toggleClusters(uint8_t *clusterTable, size_t by,
					 size_t bi, size_t reqClusters)
{
	ErrorStatus retVal;
	retVal = SUCCESS;
	if (clusterTable == NULL) {
		retVal = ERROR;
		goto fExit;
	}

	while (reqClusters > 0U) {
		while ((bi < 8U) && (reqClusters > 0U)) {
			uint8_t mask = (uint8_t)(0x01U << bi);
			clusterTable[by] ^= mask;
			bi++;
			reqClusters--;
		}
		bi = 0U;
		by++;
	}
	retVal = SUCCESS;
fExit:
	return retVal;
}

/**
 * @brief FindFreeSlot returns first available slot in directory table
 * @param media media description table
 * @return index of the slot or UINT32_MAX in case of error
 */
NDEBUG_STATIC size_t FindFreeSlot(const Media_Desc_t* const media)
{
	size_t retVal;
	retVal = UINT32_MAX;
	DIR_Entry_t tmp_entry;
	for (size_t i = 1U; i < DIR_ENTRIES; i++) {
		size_t mediaAddr = getDIR_EntryOffset(i);
		if (media->readFunc((uint8_t *)&tmp_entry, mediaAddr,
				    sizeof(DIR_Entry_t)) != SUCCESS) {
			break;
		}
		if (tmp_entry.FileStatus == FStateNoFile) {
			retVal = i;
			break;
		}
	}
	return retVal;
}

/**
 * @brief SaveDirEntry saves entry to the media
 * @param media  media description table
 * @param entryIndex index to save to
 * @param entry the entry to save
 * @return entryIndex if all is OK, UINT32_MAX otherwise
 */
NDEBUG_STATIC size_t SaveDirEntry(const Media_Desc_t * media, size_t entryIndex,
				  const DIR_Entry_p entry)
{
	/* the func is static, so do not check pointers against NULL */
	size_t retVal;
	retVal = UINT32_MAX;
	if (entryIndex >= (DIR_ENTRIES - 1U)) {
		goto fExit;
	}
	size_t mediaAddr = getDIR_EntryOffset(entryIndex);
	if (media->writeFunc((uint8_t *)entry, mediaAddr,
			     sizeof(DIR_Entry_t)) != SUCCESS) {
		goto fExit;
	}

	/* renew DIR CRC32 */
	if (RenewDirCRC32(media) != UINT32_MAX) {
		retVal = entryIndex;
	}
fExit:
	return retVal;
}

/**
 * @brief RenewDirCRC32 renews CRC32 of root directory
 * @param media media description table
 * @return new CRC32 or UINT32_MAX in case of error
 */
NDEBUG_STATIC uint32_t RenewDirCRC32(const Media_Desc_t * const media)
{
	/* the func is static, so do not check pointers against NULL */
	uint32_t retVal;
	retVal = UINT32_MAX;
	uint32_t crc32 = 0x00U;
	DIR_Entry_t dirEntry;

	TAKE_MUTEX(CRC_MutexHandle);

	for (size_t i = 0U; i < DIR_ENTRIES; i++) {
		size_t mediaAddr = getDIR_EntryOffset(i);
		if (media->readFunc((uint8_t *)&dirEntry, mediaAddr,
				    sizeof(DIR_Entry_t)) != SUCCESS) {
			goto fExit;
		}
		crc32 = CRC32((uint8_t *)&dirEntry, sizeof(DIR_Entry_t), crc32);
	}

	GIVE_MUTEX(CRC_MutexHandle);

	/* write header */
	FAT_Header_t header;
	if (media->readFunc((uint8_t *)&header, 0U, sizeof(FAT_Header_t)) !=
	    SUCCESS) {
		goto fExit;
	}
	header.DIR_CRC32 = crc32;
	if (media->writeFunc((uint8_t *)&header, 0U, sizeof(FAT_Header_t)) ==
	    SUCCESS) {
		retVal = crc32;
	}
fExit:
	return retVal;
}

/**
 * @brief encodeFName encodes given name to array of bytes
 * @param name pointer to target array
 * @param sname source string
 * @note string longer than limit will be truncated
 * @note 0123456789 ABCDEFGHIJ KLMNOPQRST UVWXYZ ! # $ % & ' ( ) - @ ^ _ ` { } ~
 */
NDEBUG_STATIC void encodeFName(char *name[MAX_FILENAME_LEN], const char * sname)
{
	(void)name;
	(void)sname;
}

/**
 * @brief decodeFName
 * @param name
 * @param sname
 */
NDEBUG_STATIC void decodeFName(const char *name[MAX_FILENAME_LEN], char * sname)
{
	(void)name;
	(void)sname;
}

/**
 * @brief readFAT reads the FAT into the buffer
 * @param media
 * @param buf
 * @return ERROR or SUCCESS
 */
NDEBUG_STATIC ErrorStatus readFAT(const Media_Desc_t * const media, uint8_t *buf)
{
	/* the func is static, so do not check pointers against NULL */
	ErrorStatus retVal;
	retVal = media->readFunc(buf, getClusterTableOffset(),
				 getClusterTableSize(media));
	return retVal;
}

/**
 * @brief readFAT writes the FAT on the media from the buffer
 * @param media
 * @param buf
 * @return ERROR or SUCCESS
 */
NDEBUG_STATIC ErrorStatus writeFAT(const Media_Desc_t * media, uint8_t *buf)
{
	/* the func is static, so do not check pointers against NULL */
	ErrorStatus retVal;
	retVal = media->writeFunc(buf, getClusterTableOffset(),
				  getClusterTableSize(media));
	return retVal;
}

/**
 * @brief FRESULT_String returns string representation of FRESULT
 * @param res
 * @return pointer to the string
 */
const char *FRESULT_String(FRESULT res)
{
	const char *retVal;

	switch (res) {
	case FR_OK: {
		retVal = "FR_OK";
		break;
	}
	case FR_DISK_ERR: {
		retVal = "FR_DISK_ERR";
		break;
	}
	case FR_INT_ERR: {
		retVal = "FR_INT_ERR";
		break;
	}
	case FR_NOT_READY: {
		retVal = "FR_NOT_READY";
		break;
	}
	case FR_NO_FILE: {
		retVal = "FR_NO_FILE";
		break;
	}
	case FR_NO_PATH: {
		retVal = "FR_NO_PATH";
		break;
	}
	case FR_INVALID_NAME: {
		retVal = "FR_INVALID_NAME";
		break;
	}
	case FR_DENIED: {
		retVal = "FR_DENIED";
		break;
	}
	case FR_EXIST: {
		retVal = "FR_EXIST";
		break;
	}
	case FR_INVALID_OBJECT: {
		retVal = "FR_INVALID_OBJECT";
		break;
	}
	case FR_WRITE_PROTECTED: {
		retVal = "FR_WRITE_PROTECTED";
		break;
	}
	case FR_INVALID_DRIVE: {
		retVal = "FR_INVALID_DRIVE";
		break;
	}
	case FR_NOT_ENABLED: {
		retVal = "FR_NOT_ENABLED";
		break;
	}
	case FR_NO_FILESYSTEM: {
		retVal = "FR_NO_FILESYSTEM";
		break;
	}
	case FR_MKFS_ABORTED: {
		retVal = "FR_MKFS_ABORTED";
		break;
	}
	case FR_TIMEOUT: {
		retVal = "FR_TIMEOUT";
		break;
	}
	case FR_LOCKED: {
		retVal = "FR_LOCKED";
		break;
	}
	case FR_NOT_ENOUGH_CORE: {
		retVal = "FR_NOT_ENOUGH_CORE";
		break;
	}
	case FR_TOO_MANY_OPEN_FILES: {
		retVal = "FR_TOO_MANY_OPEN_FILES";
		break;
	}
	case FR_INVALID_PARAMETER: {
		retVal = "FR_INVALID_PARAMETER";
		break;
	}
	default: {
		retVal = "<bad enum>";
	}
	}
	return retVal;
}

/* Write data to a file */
/**
 * @brief f_write writes data to an open file
 * @param fp pointer to th file handle
 * @param buff pointer to data buffer to be written
 * @param btw bytes to erite
 * @param bw bytes actually written
 * @return FRESULT
 */
FRESULT f_write(FIL *fp, void *const buff, UINT btw, UINT *bw)
{
	TAKE_MUTEX(FS_Mutex01Handle);

	FRESULT retVal;
	retVal = FR_INVALID_PARAMETER;

	if (isPtrValid(buff) != true) {
		goto fExit;
	}

	if (isPtrValid(bw) != true) {
		goto fExit;
	}

	/* validate file */
	retVal = isFileValid(fp);
	if (retVal != FR_OK) {
		goto fExit;
	}

	if (fp->fileDir.FileStatus != FStateOpenedW) {
		retVal = FR_DENIED;
		goto fExit;
	}
	int32_t avail;		/* how many bytes can be written into existing file size */
	int32_t freeTail;	/* how many bytes are in the last cluster beyond file end */

	/* fileDir.FileSize = 0 guarantees 1 allocated cluster */
	if ((int32_t)fp->fileDir.FileSize == 0U) {
		avail = 0U;
		freeTail = FS_CLUSTER_SIZE;
	} else {
		avail = (int32_t)fp->fileDir.FileSize - (int32_t)fp->filePtr; /* w/o possible cluster tail */
		freeTail = ((fp->fileDir.FileSize % FS_CLUSTER_SIZE) == 0U) \
		? 0 : (FS_CLUSTER_SIZE - (fp->fileDir.FileSize % FS_CLUSTER_SIZE));
	}
	avail += freeTail;

	uint32_t to_write;

	to_write = ((int32_t)btw <= avail) ? btw : (uint32_t)avail;

	if (fp->media->writeFunc((uint8_t*)buff, fp->fileDir.FileAddress + fp->filePtr, to_write) ==
	    ERROR) {
		retVal = FR_DISK_ERR;
		goto fExit;
	}
	fp->filePtr += to_write;
	fp->fileDir.FileSize = (fp->filePtr > fp->fileDir.FileSize) ?
				       (uint16_t)fp->filePtr :
				       fp->fileDir.FileSize;
	*bw = to_write;
	retVal = FR_OK;
fExit:
	GIVE_MUTEX(FS_Mutex01Handle);
	return retVal;
}

/* Read data from a file */
/**
 * @brief f_read reads dat afrom the file
 * @param fp pointer to th file handle
 * @param buff pointer to buffer to put data
 * @param btr bytes to read
 * @param br bytes actually read
 * @return FRESULT
 */
FRESULT f_read(FIL *fp, void *buff, UINT btr, UINT *br)
{
	TAKE_MUTEX(FS_Mutex01Handle);

	FRESULT retVal;
	retVal = FR_INVALID_PARAMETER;

	if (isPtrValid(buff) != true) {
		goto fExit;
	}

	if (isPtrValid(br) != true) {
		goto fExit;
	}

	/* validate file */
	retVal = isFileValid(fp);
	if (retVal != FR_OK) {
		goto fExit;
	}

	if (fp->fileDir.FileStatus != FStateOpenedR) {
		retVal = FR_DENIED;
		goto fExit;
	}

	if (btr == 0U) {
		retVal = FR_OK;
		*br = 0U;
		goto fExit;
	}
	/* all the parameters are OK */
	int32_t avail;
	avail = (int32_t)fp->fileDir.FileSize - (int32_t)fp->filePtr;
	avail = (avail < 0) ? 0 : avail;
	uint32_t to_read;
	to_read = ((int32_t)btr <= avail) ? btr : (uint32_t)avail;

	if (fp->media->readFunc((uint8_t*)buff, fp->fileDir.FileAddress + fp->filePtr, to_read) ==
	    ERROR) {
		retVal = FR_DISK_ERR;
		goto fExit;
	}
	fp->filePtr += to_read;
	*br = to_read;
	retVal = FR_OK;
fExit:
	GIVE_MUTEX(FS_Mutex01Handle);
	return retVal;
}

/**
 * @brief f_tell returns current file pointer
 * @param fp fp pointer to the file handle
 * @return position
 */
FSIZE_t f_tell(FIL *fp)
{
	FSIZE_t retVal;
	retVal = isFileValid(fp);
	if (retVal != FR_OK) {
		retVal = UINT32_MAX;
	} else {
		retVal = fp->filePtr;
	}
	return retVal;
}

/**
 * @brief f_rewind sets the file pointer to zero offset
 * @param fp pointer to the file handle
 * @return FRESULT
 */
FRESULT f_rewind(FIL *fp)
{
	return f_lseek(fp, 0U);
}

/**
 * @brief f_lseek moves file pointer to requested position
 * @param fp pointer to the file handle
 * @param ofs new pointer position
 * @return FRESULT
 */
FRESULT f_lseek(FIL *fp, FSIZE_t ofs)
{
	TAKE_MUTEX(FS_Mutex01Handle);

	FRESULT retVal;
	retVal = isFileValid(fp);
	if (retVal != FR_OK) {
		goto fExit;
	}

	switch (fp->fileDir.FileStatus) {
	case FStateOpenedR: {
		fp->filePtr = (ofs >= fp->fileDir.FileSize) ?
				      fp->fileDir.FileSize :
				      ofs;
		retVal = FR_OK;
		break;
	}
	case FStateOpenedW: {
		/* it is not possible to write after the initial file size...*/
		if (ofs < fp->fileDir.FileSize) {
			fp->filePtr = ofs;
			retVal = FR_OK;
		} else {
			retVal = FR_INVALID_PARAMETER;
		}
		break;
	}
	default: {
		retVal = FR_INVALID_PARAMETER;
		break;
	}
	}

fExit:
	GIVE_MUTEX(FS_Mutex01Handle);
	return retVal;
}

/**
 * @brief validateFile validates file handle supplied
 * @param fp pointer to the file handle
 * @return FRESULT
 */
NDEBUG_STATIC FRESULT isFileValid(FIL *fp)
{
	FRESULT retVal;
	retVal = FR_INVALID_PARAMETER;

	if (isPtrValid(fp) != true) {
		goto fExit;
	}

	if (isMediaValid(fp->media) != true) {
		goto fExit;
	}
	if (fp->fileDir.FileName[0] == '\0') {
		retVal = FR_NO_FILE;
		goto fExit;
	}

	/* file must exist */

	/* file must be opened */
	retVal = FR_OK;
fExit:
	return retVal;
}

/**
 * @brief validatePtr checks ptr for NULL
 * @param p pointer to be checked
 * @return true if pointer is valid, false otherwize
 * @note panics if ptr == NULL
 */
NDEBUG_STATIC bool isPtrValid(const void *p)
{
	if (p == NULL) {
		//#ifdef DEBUG
		//		printf("Panic! Null pointer detected!\n");
		//		exit(-3);
		//#endif
		return false;
	} else {
		return true;
	}
}

/**
 * @brief validateMedia checks the media supplied for validity
 * @param media
 * @return true or false
 */
NDEBUG_STATIC bool isMediaValid(const Media_Desc_t *media)
{
	bool retVal;
	retVal = false;
	if (isPtrValid(media) == false) {
		goto fExit;
	}
	if (isPtrValid((void *)media->readFunc) == false) {
		goto fExit;
	}
	if ((isPtrValid((void *)media->writeFunc) == false) &&
	    (media->mode != MEDIA_RO)) {
		goto fExit;
	}
	retVal = true;
fExit:
	return retVal;
}

/**
 * @brief f_checkFS
 * @param media
 * @return
 */
FRESULT f_checkFS(const Media_Desc_t *media)
{
	TAKE_MUTEX(FS_Mutex01Handle);

	FRESULT retVal = FR_INVALID_OBJECT;
	if (isMediaValid(media) == false) {
		goto fExit;
	}
	DIR_Entry_t FATFile;
	/* 0th file must be "$$FAT$$" */
	size_t mediaAddr = getDIR_EntryOffset(0U);
	ErrorStatus mediaError;

	mediaError = media->readFunc((uint8_t *)&FATFile, mediaAddr,
				     sizeof(DIR_Entry_t));
	if (mediaError != SUCCESS) {
		retVal = FR_DISK_ERR;
		goto fExit;
	}

	if (strcmp((char *)&FATFile.FileName, "$$FAT$$") != 0) {
		retVal = FR_NO_FILE;
		goto fExit;
	}
	if (FATFile.FileStatus != FStateFAT) {
		retVal = FR_NO_FILE;
		goto fExit;
	}
	uint32_t DIR_CRC = 0U; /* ROOT directory's CRC32 */

	TAKE_MUTEX(CRC_MutexHandle);

	for (size_t i = 0U; i < DIR_ENTRIES; ++i) {
		size_t mediaAddr = getDIR_EntryOffset(i);
		memset(&FATFile, 0, sizeof(DIR_Entry_t));

		mediaError = media->readFunc((uint8_t *)&FATFile, mediaAddr,
					     sizeof(DIR_Entry_t));
		if (mediaError != SUCCESS) {
			retVal = FR_DISK_ERR;
			goto fExit;
		} else {
			DIR_CRC = CRC32((uint8_t *)&FATFile,
					sizeof(DIR_Entry_t), DIR_CRC);
		}
	}

	GIVE_MUTEX(CRC_MutexHandle);

	/* read CRC32 from header */
	FAT_Header_t header;
	mediaError =
		media->readFunc((uint8_t *)&header, 0U, sizeof(FAT_Header_t));
	if (mediaError != SUCCESS) {
		retVal = FR_DISK_ERR;
		goto fExit;
	}
	if (header.DIR_CRC32 != DIR_CRC) {
		retVal = FR_NO_FILESYSTEM;
	} else {
		retVal = FR_OK;
	}
fExit:
	GIVE_MUTEX(FS_Mutex01Handle);
	return retVal;
}
