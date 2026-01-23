################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Middlewares/FreeRTOS/Source/portable/MemMang/heap_4.c 

C_DEPS += \
./Middlewares/FreeRTOS/Source/portable/MemMang/heap_4.d 

OBJS += \
./Middlewares/FreeRTOS/Source/portable/MemMang/heap_4.o 


# Each subdirectory must supply rules for building sources it contributes
Middlewares/FreeRTOS/Source/portable/MemMang/%.o Middlewares/FreeRTOS/Source/portable/MemMang/%.su Middlewares/FreeRTOS/Source/portable/MemMang/%.cyclo: ../Middlewares/FreeRTOS/Source/portable/MemMang/%.c Middlewares/FreeRTOS/Source/portable/MemMang/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m7 -std=gnu11 -g -DDEBUG -DUSE_PWR_LDO_SUPPLY -DUSE_HAL_DRIVER -DSTM32H743xx -c -I../Core/Inc -I../Drivers/STM32H7xx_HAL_Driver/Inc -I../Drivers/STM32H7xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32H7xx/Include -I../Drivers/CMSIS/Include -I../TouchGFX/App -I../TouchGFX/target/generated -I../TouchGFX/target -I../Middlewares/ST/touchgfx/framework/include -I../TouchGFX/generated/fonts/include -I../TouchGFX/generated/gui_generated/include -I../TouchGFX/generated/images/include -I../TouchGFX/generated/texts/include -I../TouchGFX/generated/videos/include -I../TouchGFX/gui/include -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Middlewares-2f-FreeRTOS-2f-Source-2f-portable-2f-MemMang

clean-Middlewares-2f-FreeRTOS-2f-Source-2f-portable-2f-MemMang:
	-$(RM) ./Middlewares/FreeRTOS/Source/portable/MemMang/heap_4.cyclo ./Middlewares/FreeRTOS/Source/portable/MemMang/heap_4.d ./Middlewares/FreeRTOS/Source/portable/MemMang/heap_4.o ./Middlewares/FreeRTOS/Source/portable/MemMang/heap_4.su

.PHONY: clean-Middlewares-2f-FreeRTOS-2f-Source-2f-portable-2f-MemMang

