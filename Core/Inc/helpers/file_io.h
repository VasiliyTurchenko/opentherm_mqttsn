/** @file file_io.h
 *  @brief file IO helpers
 *
 *  @author turchenkov@gmail.com
 *  @bug
 *  @date 10-Mar-2019
 */

#ifndef FILE_IO_H
#define FILE_IO_H

#include "tiny-fs.h"

#ifdef __cplusplus
 extern "C" {
#endif

FRESULT ReadBytes(const Media_Desc_t * const media,
		   const char * const name,
		   size_t fpos,
		   size_t btr,
		   size_t * const br,
		   uint8_t * buf);

FRESULT WriteBytes(const Media_Desc_t *const media,
		    const char * name,
		    size_t fpos,
		    size_t btw,
		    size_t * const bw,
		    const uint8_t * buf);

FRESULT AllocSpaceForFile(const Media_Desc_t *const media,
			  const char *name,
			  size_t req_size);

#ifdef __cplusplus
 }
#endif

#endif // FILE_IO_H
