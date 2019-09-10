/** @file rtc_helpers.c
 *  @brief RTC helper functions
 *
 *  @author turchenkov@gmail.com
 *  @bug
 *  @date 11-Mar-20019
 */

#include <stdbool.h>
#include <time.h>

#include "rtc.h"
#include "rtc_helpers.h"

RTC_TimeTypeDef myRTC_Time;
#ifdef STM32F103xB
static HAL_StatusTypeDef RTC_EnterInitMode(RTC_HandleTypeDef *hrtc);
#endif
static HAL_StatusTypeDef RTC_ExitInitMode(RTC_HandleTypeDef *hrtc);

#ifdef STM32F303xC
static void UT_to_RTC(RTC_TimeTypeDef *tm, RTC_DateTypeDef *dt,
		   tTime_p unix_epoch);
static void RTC_to_UT(RTC_TimeTypeDef *tm, RTC_DateTypeDef *dt,
		      tTime_p unix_epoch);
static inline bool isLeapYear(uint32_t year);
static inline uint8_t BCD_to_HEX(uint8_t bcd);
static inline uint8_t HEX_to_BCD(uint8_t hex);
#endif

void HAL_RTCEx_RTCEventCallback(RTC_HandleTypeDef *hrtc)
{
	//	HAL_RTC_GetTime(hrtc, &myRTC_Time, RTC_FORMAT_BIN);
}

/**
  * @brief  SaveTimeToRTC saves unix NTP time into RTC counter
  * @note   also writes zero to the ms counter of the RTC
  * @param  ptr_Time time_now
  * @retval ERROR or SUCCESS
  */
ErrorStatus SaveTimeToRTC(tTime_p time_now)
{
	ErrorStatus result;
	result = ERROR;
	/* Check input parameters */
	if (time_now == NULL) {
		goto fExit;
	}
#ifdef STM32F103xB
	/* Set Initialization mode */
	if (RTC_EnterInitMode(&hrtc) != HAL_OK) {
		goto fExit;
	} else {
		/* Set RTC COUNTER MSB word */
		WRITE_REG(hrtc.Instance->CNTH, (time_now->Seconds >> 16U));
		/* Set RTC COUNTER LSB word */
		WRITE_REG(hrtc.Instance->CNTL,
			  ((time_now->Seconds) & RTC_CNTL_RTC_CNT));
		/* Wait for synchro */
		if (RTC_ExitInitMode(&hrtc) == HAL_OK) {
			result = SUCCESS;
		}
	}
#elif STM32F303xC
	// HAL_StatusTypeDef HAL_RTC_SetTime(RTC_HandleTypeDef *hrtc, RTC_TimeTypeDef *sTime, uint32_t Format)
	/* convert unix time to RTC_TimeTypeDef *sTime */
	RTC_TimeTypeDef tmp_time;
	RTC_DateTypeDef tmp_date;
	UT_to_RTC(&tmp_time, &tmp_date, time_now);
	tmp_time.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
	tmp_time.StoreOperation = RTC_STOREOPERATION_RESET;
	if (HAL_RTC_SetTime(&hrtc, &tmp_time, RTC_FORMAT_BIN) != HAL_OK) {
		goto fExit;
	}
	if (HAL_RTC_SetDate(&hrtc, &tmp_date, RTC_FORMAT_BIN) != HAL_OK) {
		goto fExit;
	}
	result = SUCCESS;
#else
#error "MCU TARGET NOT DEFINED!"
#endif

fExit:
	return result;
}
/* end of the function SaveTimeToRTC */

/**
  * @brief  GetTimeFromRTC seconds and milliseconds since 01-01-1970 00:00:00 from the RTC counter
  * @note
  * @param  ptr_Time time_now
  * @retval ERROR or SUCCESS
  */
ErrorStatus GetTimeFromRTC(tTime_p time_now)
{
	ErrorStatus result;
	result = ERROR;
#ifdef STM32F103xB
	uint16_t high1 = 0U;
	uint16_t high2 = 0U;
	uint16_t low = 0U;
	uint32_t timecounter = 0U;
#endif
	/* Check input parameters */
	if (time_now == NULL) {
		goto fExit;
	}
#ifdef STM32F103xB
	high1 = READ_REG(hrtc.Instance->CNTH & RTC_CNTH_RTC_CNT);
	low = READ_REG(hrtc.Instance->CNTL & RTC_CNTL_RTC_CNT);
	high2 = READ_REG(hrtc.Instance->CNTH & RTC_CNTH_RTC_CNT);

	if (high1 != high2) {
		/* In this case the counter roll over during reading of CNTL and CNTH registers,
	read again CNTL register then return the counter value */
		timecounter =
			(((uint32_t)high2 << 16U) |
			 READ_REG(hrtc.Instance->CNTL & RTC_CNTL_RTC_CNT));
	} else {
		/* No counter roll over during reading of CNTL and CNTH registers, counter
       value is equal to first value of CNTL and CNTH */
		timecounter = (((uint32_t)high1 << 16U) | low);
	}
	time_now->Seconds = timecounter;
	time_now->mSeconds =
		(READ_REG(hrtc.Instance->DIVL & RTC_DIVL_RTC_DIV) / 33U);
	result = SUCCESS;
#elif STM32F303xC
	RTC_DateTypeDef tmp_date0;
	if (HAL_RTC_GetDate(&hrtc, &tmp_date0, RTC_FORMAT_BIN) != HAL_OK) {
		goto fExit;
	}
	RTC_TimeTypeDef tmp_time;
	if (HAL_RTC_GetTime(&hrtc, &tmp_time, RTC_FORMAT_BIN) != HAL_OK) {
		goto fExit;
	}
	RTC_DateTypeDef tmp_date1;
	if (HAL_RTC_GetDate(&hrtc, &tmp_date1, RTC_FORMAT_BIN) != HAL_OK) {
		goto fExit;
	}
	/* check that tade and tima are from the same day */
	if (tmp_date0.Date != tmp_date1.Date) {
		if (HAL_RTC_GetTime(&hrtc, &tmp_time, RTC_FORMAT_BIN) !=
		    HAL_OK) {
			goto fExit;
		}
	}

	result = SUCCESS;
	RTC_to_UT(&tmp_time, &tmp_date1, time_now);
#endif
fExit:
	return result;
}

/**
  * @brief  Enters the RTC Initialization mode.
  * @param  hrtc   pointer to a RTC_HandleTypeDef structure that contains
  *                the configuration information for RTC.
  * @retval HAL status
  */
#ifdef STM32F103xB
static HAL_StatusTypeDef RTC_EnterInitMode(RTC_HandleTypeDef *hrtc)
{
	uint32_t tickstart = 0U;

	tickstart = HAL_GetTick();
	/* Wait till RTC is in INIT state and if Time out is reached exit */
	while ((hrtc->Instance->CRL & RTC_CRL_RTOFF) == (uint32_t)RESET) {
		if ((HAL_GetTick() - tickstart) > RTC_TIMEOUT_VALUE) {
			return HAL_TIMEOUT;
		}
	}

	/* Disable the write protection for RTC registers */
	__HAL_RTC_WRITEPROTECTION_DISABLE(hrtc);

	return HAL_OK;
}
#endif

/**
  * @brief  Exit the RTC Initialization mode.
  * @param  hrtc   pointer to a RTC_HandleTypeDef structure that contains
  *                the configuration information for RTC.
  * @retval HAL status
  */
static HAL_StatusTypeDef RTC_ExitInitMode(RTC_HandleTypeDef *hrtc)
{
	uint32_t tickstart = 0U;
#ifdef STM32F103xB
	/* Disable the write protection for RTC registers */
	__HAL_RTC_WRITEPROTECTION_ENABLE(hrtc);

	tickstart = HAL_GetTick();
	/* Wait till RTC is in INIT state and if Time out is reached exit */
	while ((hrtc->Instance->CRL & RTC_CRL_RTOFF) == (uint32_t)RESET) {
		if ((HAL_GetTick() - tickstart) > RTC_TIMEOUT_VALUE) {
			return HAL_TIMEOUT;
		}
	}
#endif
	return HAL_OK;
}

#ifdef STM32F303xC

static const uint8_t days_in_month[] = {
	0,  31, 28, 31, 30, 31, 30, /* Yan - Jun */
	31, 31, 30, 31, 30, 31
}; /* Jul - Dec */
static const size_t leap_seconds = 27U;
static const size_t days_since_1970_up_to_2000 = 10957U;
/**
 * @brief UT_to_RTC converts unix epoch to RTC_TimeTypeDef
 * @param tm stm32 RTC time
 * @param dt stm32 RTC date
 * @param unix_epoch time value provided
 * @return none
 */
static void UT_to_RTC(RTC_TimeTypeDef *tm, RTC_DateTypeDef *dt, tTime_p unix_epoch)
{
	uint32_t sec = unix_epoch->Seconds - leap_seconds;
	uint32_t min = sec / 60U;

	tm->Seconds = sec % 60U; /* seconds now */

	/* hours since 01 01 1970 00:00:00 */
	uint32_t hours = min / 60U;

	tm->Minutes = min % 60U; /* minutes now */
	tm->Hours = hours % 24U; /* hours now */

	/* days since 01 01 2000 */
	uint32_t days_left = (hours / 24U) - days_since_1970_up_to_2000;

	uint8_t year = 0U;	/* 2000 */
	uint32_t days_in_year = 365U;	/* for 2000 */
	while (days_left > days_in_year) {
		if (isLeapYear(year + 2000U)) {
			days_in_year = 366U;
		} else {
			days_in_year = 365U;
		}
		days_left -= days_in_year;
		year++;
	}
	dt->Year = year;
	uint8_t month = 1U;
	while (days_left > days_in_month[month]) {
		uint8_t leap_day = ((month == 2U) && (isLeapYear(year + 2000U))) ? 1U : 0U;
		days_left -= (days_in_month[month] + leap_day);
		month++;
	}
	dt->Month = HEX_to_BCD(month);
	dt->Date = (uint8_t)days_left + 1U;
	dt->WeekDay = 0U;
	return;

}

/**
 * @brief RTC2UT converts RTC_TimeTypeDef to unix epoch 01/01/1970
 * @param tm stm32 RTC time
 * @param dt stm32 RTC date
 * @param unix_epoch where to store the result
 * @return none
 */
static void RTC_to_UT(RTC_TimeTypeDef *tm, RTC_DateTypeDef *dt,
		      tTime_p unix_epoch)
{
	uint8_t month = BCD_to_HEX(dt->Month);
	if (month == 0U) {
		month = 1U;
	}
	uint32_t year = 2000U + (uint32_t)dt->Year;
	/* how many FULL years are gone till the moment from the start of the century */
	 /*0th (=2000th) year is leap */
	uint32_t leap_years = ((uint32_t)dt->Year / 4U) + 1U;

	if (isLeapYear(year)) {
	leap_years--;
	}

	uint32_t ord_years = (uint32_t)dt->Year - leap_years;

	/* curent year's days */
	uint32_t days = 0U;
	for (uint8_t i = 1U; i < month; i++) {
		days += days_in_month[i];
	}
	/* 29th feb check */
	if (isLeapYear(year) && (month) > 2U) {
		days++;
	}
	/* add current month's completed days */
	days += (uint32_t)dt->Date;
	/* total days since year 2000 */
	uint32_t total_days_since_2000;
	total_days_since_2000 = (365U * ord_years) + (366U * leap_years) + days;
	/* total days since year 1970 */
	uint32_t total_days_since_1970;
	total_days_since_1970 = days_since_1970_up_to_2000 + total_days_since_2000;

	/* hours , minutes, seconds since 01 01 1970 00:00:00 */
	uint32_t hours = (total_days_since_1970 - 1U) * 24U + tm->Hours;
	uint32_t minutes = hours * 60U + tm->Minutes;
	uint32_t seconds = leap_seconds + minutes * 60U + tm->Seconds;
	unix_epoch->Seconds = seconds;
	unix_epoch->mSeconds = 1000U / (tm->SecondFraction + 1U);
	return;
}

/**
 * @brief BCD_to_HEX
 * @param bcd
 * @return hex value of bcd input or 0xFF in  case of bad bcd value
 */
static inline uint8_t BCD_to_HEX(uint8_t bcd)
{
	uint8_t unit = bcd & 0x0FU;
	uint8_t dec = (uint8_t)(bcd >> 4);
	if ((unit <= 9U) && (dec <= 9)) {
		return 10U * dec + unit;
	} else {
		return 0xFFU; /* bad bcd value */
	}
}

/**
 * @brief HEX_to_BCD
 * @param hex
 * @return bcd repersentation of the hex value 0xFF in  case of bad hex value
 */
static inline uint8_t HEX_to_BCD(uint8_t hex)
{
	if (hex > 99U) {
		return 0xFFU;
	}
	return (hex % 10U) + (uint8_t)((hex / 10U) << 4);
}

/**
 * @brief isLeapYear returns true fi year provided is leap
 * @param year
 * @return true or false
 */
static inline bool isLeapYear(uint32_t year)
{
	/*
	if (year is not divisible by 4) then (it is a common year)
	else if (year is not divisible by 100) then (it is a leap year)
	else if (year is not divisible by 400) then (it is a common year)
	else (it is a leap year)
	*/
	if ((year % 4U) != 0U) {
		return false;
	}
	if ((year % 100U) != 0U) {
		return true;
	}
	if ((year % 400U) != 0) {
		return false;
	}
	return true;
}
#endif
