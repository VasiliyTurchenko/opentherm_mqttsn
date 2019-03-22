/** @file ip_helpers.h
 *  @brief ip configuration helpers
 *  @author turchenkov@gmail.com
 *  @bug
 *  @date 13-Mar-2019
 */

#ifndef IP_HELPERS_H
#define IP_HELPERS_H

#ifdef __cplusplus
 extern "C" {
#endif

#include "config_files.h"

#ifndef MAKE_IP
#define MAKE_IP(a, b, c, d)                                                    \
	(((uint32_t)a) | ((uint32_t)b << 8) | ((uint32_t)c << 16) |            \
	 ((uint32_t)d << 24))
#endif


typedef	uint32_t	ip_addr_t;

typedef	struct	ip_pair {		/* !< general storage object */
		ip_addr_t	ip;
		uint16_t	port;
	}	ip_pair_t;

FRESULT ReadIPConfigFile(const Media_Desc_t * const media,
			 const char * const fname,
			 const struct IP * const template,
			 ip_pair_t * const ip_pair);

FRESULT SaveIPConfigFile(const Media_Desc_t * const media,
			 const char * const fname,
			 const struct IP * const template);

#ifdef __cplusplus
 }
#endif


#endif // IP_HELPERS_H
