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

RTC_TimeTypeDef	myRTC_Time;
#ifdef STM32F103xB
static HAL_StatusTypeDef RTC_EnterInitMode(RTC_HandleTypeDef* hrtc);
#endif
static HAL_StatusTypeDef RTC_ExitInitMode(RTC_HandleTypeDef* hrtc);

static void 	ut2RTC(RTC_TimeTypeDef * tm, RTC_DateTypeDef * dt, ptr_Time unix_epoch);
static void	RTC2UT(RTC_TimeTypeDef * tm, RTC_DateTypeDef * dt, ptr_Time unix_epoch);
static inline bool isLeapYear(uint32_t year);

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
ErrorStatus SaveTimeToRTC(ptr_Time time_now)
{
	ErrorStatus	result;
	result = ERROR;
	/* Check input parameters */
	if (time_now == NULL) {
		goto fExit;
	}
#ifdef STM32F103xB
	/* Set Initialization mode */
	if(RTC_EnterInitMode(&hrtc) != HAL_OK) {
		goto fExit;
	} else {
		/* Set RTC COUNTER MSB word */
		WRITE_REG(hrtc.Instance->CNTH, (time_now->Seconds >> 16U));
		/* Set RTC COUNTER LSB word */
		WRITE_REG(hrtc.Instance->CNTL, ( (time_now->Seconds) & RTC_CNTL_RTC_CNT) );
		/* Wait for synchro */
		if(RTC_ExitInitMode(&hrtc) == HAL_OK) {
			result = SUCCESS;
		}
	}
#elif STM32F303xC
// HAL_StatusTypeDef HAL_RTC_SetTime(RTC_HandleTypeDef *hrtc, RTC_TimeTypeDef *sTime, uint32_t Format)
/* convert unix time to RTC_TimeTypeDef *sTime */
	RTC_TimeTypeDef tmp_time;
	RTC_DateTypeDef tmp_date;
	ut2RTC(&tmp_time, &tmp_date, time_now);
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
ErrorStatus GetTimeFromRTC(ptr_Time time_now)
{
	ErrorStatus	result;
	result = ERROR;
	uint16_t high1 = 0U;
	uint16_t high2 = 0U;
	uint16_t low = 0U;
	uint32_t timecounter = 0U;

	/* Check input parameters */
	if (time_now == NULL) {
		goto fExit;
	}
#ifdef STM32F103xB
	high1 = READ_REG(hrtc.Instance->CNTH & RTC_CNTH_RTC_CNT);
	low   = READ_REG(hrtc.Instance->CNTL & RTC_CNTL_RTC_CNT);
	high2 = READ_REG(hrtc.Instance->CNTH & RTC_CNTH_RTC_CNT);

	if (high1 != high2) {
	/* In this case the counter roll over during reading of CNTL and CNTH registers,
	read again CNTL register then return the counter value */
		timecounter = (((uint32_t) high2 << 16U) | READ_REG(hrtc.Instance->CNTL & RTC_CNTL_RTC_CNT));
	} else {
	/* No counter roll over during reading of CNTL and CNTH registers, counter
       value is equal to first value of CNTL and CNTH */
		timecounter = (((uint32_t) high1 << 16U) | low);
	}
	time_now->Seconds = timecounter;
	time_now->mSeconds = ( READ_REG(hrtc.Instance->DIVL & RTC_DIVL_RTC_DIV) / 33U );
	result = SUCCESS;
#elif STM32F303xC
	RTC_TimeTypeDef tmp_time;
	if (HAL_RTC_GetTime(&hrtc, &tmp_time, RTC_FORMAT_BIN) != HAL_OK) {
		goto fExit;
	}
	RTC_DateTypeDef tmp_date;
	if (HAL_RTC_GetDate(&hrtc, &tmp_date, RTC_FORMAT_BIN) != HAL_OK ) {
		goto fExit;
	}
	result = SUCCESS;
	RTC2UT(&tmp_time, &tmp_date, time_now);
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
static HAL_StatusTypeDef RTC_EnterInitMode(RTC_HandleTypeDef* hrtc)
{
  uint32_t tickstart = 0U;

  tickstart = HAL_GetTick();
  /* Wait till RTC is in INIT state and if Time out is reached exit */
  while((hrtc->Instance->CRL & RTC_CRL_RTOFF) == (uint32_t)RESET)
  {
    if((HAL_GetTick() - tickstart) >  RTC_TIMEOUT_VALUE)
    {
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
static HAL_StatusTypeDef RTC_ExitInitMode(RTC_HandleTypeDef* hrtc)
{
  uint32_t tickstart = 0U;
#ifdef STM32F103xB
  /* Disable the write protection for RTC registers */
  __HAL_RTC_WRITEPROTECTION_ENABLE(hrtc);

  tickstart = HAL_GetTick();
  /* Wait till RTC is in INIT state and if Time out is reached exit */
  while((hrtc->Instance->CRL & RTC_CRL_RTOFF) == (uint32_t)RESET)
  {
    if((HAL_GetTick() - tickstart) >  RTC_TIMEOUT_VALUE)
    {
      return HAL_TIMEOUT;
    }
  }
#endif
  return HAL_OK;
}


static const uint8_t days_in_month[] = {0,
					31,28,31,30,31,30,  /* Yan - Jun */
					31,31,30,31,30,31}; /* Jul - Dec */
static const size_t leap_seconds = 27U;
/**
 * @brief ut2RTC converts unix epoch to RTC_TimeTypeDef
 * @param tm
 */
static void 	ut2RTC(RTC_TimeTypeDef * tm, RTC_DateTypeDef * dt, ptr_Time unix_epoch)
{

}

/**
 * @brief RTC2UT converts RTC_TimeTypeDef to unix epoch 01/01/1970
 * @param tm
 * @return
 */
static void	RTC2UT(RTC_TimeTypeDef * tm, RTC_DateTypeDef * dt, ptr_Time unix_epoch)
{
	uint32_t	year = 2000U + (uint32_t)dt->Year;
	const uint32_t	startYear = 1970U;	/* not leap */

	uint32_t	leap_years = ((uint32_t)dt->Year / 4U) + 1U;	/*0th (=2000th) year is leap */
	uint32_t	ord_years = (uint32_t)dt->Year - leap_years;



	/* curent year's days */
	uint32_t	days = 0U;
	for (uint8_t i = 1U; i < dt->Month; i++) {
		days += days_in_month[i];
		if (i == 2U) {
			days++;
		}
	}
	days += (uint32_t)dt->Date;
	uint32_t	total_days;
	total_days = (365U * ord_years) + (366U * leap_years) + days;


}


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
	return  true;
}
