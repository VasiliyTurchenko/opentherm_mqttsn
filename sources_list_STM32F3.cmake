set(GROUP_DRIVERS_STM32F3XX_HAL_DRIVER_SRC
	        Drivers/STM32F3xx_HAL_Driver/Src/stm32f3xx_hal.c
		Drivers/STM32F3xx_HAL_Driver/Src/stm32f3xx_hal_adc.c
		Drivers/STM32F3xx_HAL_Driver/Src/stm32f3xx_hal_adc_ex.c
		Drivers/STM32F3xx_HAL_Driver/Src/stm32f3xx_hal_cortex.c
		Drivers/STM32F3xx_HAL_Driver/Src/stm32f3xx_hal_crc.c
		Drivers/STM32F3xx_HAL_Driver/Src/stm32f3xx_hal_crc_ex.c
		Drivers/STM32F3xx_HAL_Driver/Src/stm32f3xx_hal_dma.c
		Drivers/STM32F3xx_HAL_Driver/Src/stm32f3xx_hal_flash.c
		Drivers/STM32F3xx_HAL_Driver/Src/stm32f3xx_hal_flash_ex.c
		Drivers/STM32F3xx_HAL_Driver/Src/stm32f3xx_hal_gpio.c
#		Drivers/STM32F3xx_HAL_Driver/Src/stm32f1xx_hal_gpio_ex.c
                Drivers/STM32F3xx_HAL_Driver/Src/stm32f3xx_hal_i2c.c
		Drivers/STM32F3xx_HAL_Driver/Src/stm32f3xx_hal_i2c_ex.c
		Drivers/STM32F3xx_HAL_Driver/Src/stm32f3xx_hal_iwdg.c
		Drivers/STM32F3xx_HAL_Driver/Src/stm32f3xx_hal_pwr.c
		Drivers/STM32F3xx_HAL_Driver/Src/stm32f3xx_hal_pwr_ex.c
		Drivers/STM32F3xx_HAL_Driver/Src/stm32f3xx_hal_rcc.c
		Drivers/STM32F3xx_HAL_Driver/Src/stm32f3xx_hal_rcc_ex.c
		Drivers/STM32F3xx_HAL_Driver/Src/stm32f3xx_hal_rtc.c
		Drivers/STM32F3xx_HAL_Driver/Src/stm32f3xx_hal_rtc_ex.c
		Drivers/STM32F3xx_HAL_Driver/Src/stm32f3xx_hal_spi.c
		Drivers/STM32F3xx_HAL_Driver/Src/stm32f3xx_hal_spi_ex.c
		Drivers/STM32F3xx_HAL_Driver/Src/stm32f3xx_hal_tim.c
		Drivers/STM32F3xx_HAL_Driver/Src/stm32f3xx_hal_tim_ex.c
		Drivers/STM32F3xx_HAL_Driver/Src/stm32f3xx_hal_uart.c
		Drivers/STM32F3xx_HAL_Driver/Src/stm32f3xx_hal_uart_ex.c
)


set(GROUP_MIDDLEWARES_THIRD_PARTY_FREERTOS_SOURCE_PORTABLE_GCC_ARM_CM4F
	        Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F/port.c
)

set(GROUP_CORE_SRC_STM32F3
	        Core/Src/stm32f3/stm32f3xx_hal_msp.c
		Core/Src/stm32f3/stm32f3xx_hal_timebase_tim.c
		Core/Src/stm32f3/stm32f3xx_it.c
		Core/Src/stm32f3/system_stm32f3xx.c
)


include_directories(Drivers/STM32F3xx_HAL_Driver/Inc)
include_directories(Drivers/STM32F3xx_HAL_Driver/Inc/Legacy)
include_directories(Drivers/CMSIS/Device/ST/STM32F3xx/Include)
include_directories(Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F)

set(LIST_OF_SOURCES_STM32F3
#	        ${LIST_OF_SOURCES}
                ${GROUP_DRIVERS_STM32F3XX_HAL_DRIVER_SRC}
		${GROUP_MIDDLEWARES_THIRD_PARTY_FREERTOS_SOURCE_PORTABLE_GCC_ARM_CM4F}
		${GROUP_CORE_SRC_STM32F3}
)
