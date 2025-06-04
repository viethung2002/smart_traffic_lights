################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Middlewares/Third_Party/SEGGER/Rec/SEGGER_UART.c 

OBJS += \
./Middlewares/Third_Party/SEGGER/Rec/SEGGER_UART.o 

C_DEPS += \
./Middlewares/Third_Party/SEGGER/Rec/SEGGER_UART.d 


# Each subdirectory must supply rules for building sources it contributes
Middlewares/Third_Party/SEGGER/Rec/%.o Middlewares/Third_Party/SEGGER/Rec/%.su Middlewares/Third_Party/SEGGER/Rec/%.cyclo: ../Middlewares/Third_Party/SEGGER/Rec/%.c Middlewares/Third_Party/SEGGER/Rec/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m3 -std=gnu11 -g -DDEBUG -DUSE_HAL_DRIVER -DSTM32F103x6 -c -I../Core/Inc -I../Drivers/STM32F1xx_HAL_Driver/Inc/Legacy -I../Drivers/STM32F1xx_HAL_Driver/Inc -I../Drivers/CMSIS/Device/ST/STM32F1xx/Include -I../Drivers/CMSIS/Include -I../Middlewares/Third_Party/FreeRTOS/org/Source/include -I../Middlewares/Third_Party/FreeRTOS/org/Source/CMSIS_RTOS -I../Middlewares/Third_Party/FreeRTOS/org/Source/portable/GCC/ARM_CM3 -I"D:/StudyDocs/Nam5Ki2/RTOS/Project/STM32C6T6/c6t6/Middlewares/Third_Party/SEGGER/Config" -I"D:/StudyDocs/Nam5Ki2/RTOS/Project/STM32C6T6/c6t6/Middlewares/Third_Party/SEGGER/OS/FreeRTOSV10" -I"D:/StudyDocs/Nam5Ki2/RTOS/Project/STM32C6T6/c6t6/Middlewares/Third_Party/SEGGER/SEGGER" -I../Middlewares/Third_Party/FreeRTOS/Source/include -I../Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS -I../Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM3 -Ofast -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfloat-abi=soft -mthumb -o "$@"

clean: clean-Middlewares-2f-Third_Party-2f-SEGGER-2f-Rec

clean-Middlewares-2f-Third_Party-2f-SEGGER-2f-Rec:
	-$(RM) ./Middlewares/Third_Party/SEGGER/Rec/SEGGER_UART.cyclo ./Middlewares/Third_Party/SEGGER/Rec/SEGGER_UART.d ./Middlewares/Third_Party/SEGGER/Rec/SEGGER_UART.o ./Middlewares/Third_Party/SEGGER/Rec/SEGGER_UART.su

.PHONY: clean-Middlewares-2f-Third_Party-2f-SEGGER-2f-Rec

