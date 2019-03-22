/** @file rtc_helpers.h
 *  @brief RTC helper functions
 *
 *  @author turchenkov@gmail.com
 *  @bug
 *  @date 11-Mar-20019
 */


#ifndef RTC_HELPERS_H
#define RTC_HELPERS_H

#ifdef __cplusplus
 extern "C" {
#endif

typedef
struct
__attribute__((packed))
        _tTime {			/*!< time holding structure */
	        uint32_t	Seconds;	/*!< NTP seconds */
		uint16_t	mSeconds;	/*!< milliseconds */
        } tTime;

typedef 	tTime	*	ptr_Time;

ErrorStatus SaveTimeToRTC(ptr_Time time_now);
ErrorStatus GetTimeFromRTC(ptr_Time time_now);

#ifdef __cplusplus
 }
#endif


#endif // RTC_HELPERS_H
