################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Middlewares/Third_Party/SEGGER/OS/FreeRTOSV10/SEGGER_SYSVIEW_FreeRTOS.c 

OBJS += \
./Middlewares/Third_Party/SEGGER/OS/FreeRTOSV10/SEGGER_SYSVIEW_FreeRTOS.o 

C_DEPS += \
./Middlewares/Third_Party/SEGGER/OS/FreeRTOSV10/SEGGER_SYSVIEW_FreeRTOS.d 


# Each subdirectory must supply rules for building sources it contributes
Middlewares/Third_Party/SEGGER/OS/FreeRTOSV10/%.o Middlewares/Third_Party/SEGGER/OS/FreeRTOSV10/%.su Middlewares/Third_Party/SEGGER/OS/FreeRTOSV10/%.cyclo: ../Middlewares/Third_Party/SEGGER/OS/FreeRTOSV10/%.c Middlewares/Third_Party/SEGGER/OS/FreeRTOSV10/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m3 -std=gnu11 -g -DDEBUG -DUSE_HAL_DRIVER -DSTM32F103x6 -c -I../Core/Inc -I../Drivers/STM32F1xx_HAL_Driver/Inc/Legacy -I../Drivers/STM32F1xx_HAL_Driver/Inc -I../Drivers/CMSIS/Device/ST/STM32F1xx/Include -I../Drivers/CMSIS/Include -I../Middlewares/Third_Party/FreeRTOS/org/Source/include -I../Middlewares/Third_Party/FreeRTOS/org/Source/CMSIS_RTOS -I../Middlewares/Third_Party/FreeRTOS/org/Source/portable/GCC/ARM_CM3 -I"D:/StudyDocs/Nam5Ki2/RTOS/Project/STM32C6T6/c6t6/Middlewares/Third_Party/SEGGER/Config" -I"D:/StudyDocs/Nam5Ki2/RTOS/Project/STM32C6T6/c6t6/Middlewares/Third_Party/SEGGER/OS/FreeRTOSV10" -I"D:/StudyDocs/Nam5Ki2/RTOS/Project/STM32C6T6/c6t6/Middlewares/Third_Party/SEGGER/SEGGER" -I../Middlewares/Third_Party/FreeRTOS/Source/include -I../Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS -I../Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM3 -Ofast -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfloat-abi=soft -mthumb -o "$@"

clean: clean-Middlewares-2f-Third_Party-2f-SEGGER-2f-OS-2f-FreeRTOSV10

clean-Middlewares-2f-Third_Party-2f-SEGGER-2f-OS-2f-FreeRTOSV10:
	-$(RM) ./Middlewares/Third_Party/SEGGER/OS/FreeRTOSV10/SEGGER_SYSVIEW_FreeRTOS.cyclo ./Middlewares/Third_Party/SEGGER/OS/FreeRTOSV10/SEGGER_SYSVIEW_FreeRTOS.d ./Middlewares/Third_Party/SEGGER/OS/FreeRTOSV10/SEGGER_SYSVIEW_FreeRTOS.o ./Middlewares/Third_Party/SEGGER/OS/FreeRTOSV10/SEGGER_SYSVIEW_FreeRTOS.su

.PHONY: clean-Middlewares-2f-Third_Party-2f-SEGGER-2f-OS-2f-FreeRTOSV10

