################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../TouchGFX/generated/gui_generated/src/screenturns_screen/ScreenTurnsViewBase.cpp 

OBJS += \
./TouchGFX/generated/gui_generated/src/screenturns_screen/ScreenTurnsViewBase.o 

CPP_DEPS += \
./TouchGFX/generated/gui_generated/src/screenturns_screen/ScreenTurnsViewBase.d 


# Each subdirectory must supply rules for building sources it contributes
TouchGFX/generated/gui_generated/src/screenturns_screen/%.o TouchGFX/generated/gui_generated/src/screenturns_screen/%.su TouchGFX/generated/gui_generated/src/screenturns_screen/%.cyclo: ../TouchGFX/generated/gui_generated/src/screenturns_screen/%.cpp TouchGFX/generated/gui_generated/src/screenturns_screen/subdir.mk
	arm-none-eabi-g++ "$<" -mcpu=cortex-m7 -std=gnu++14 -g3 -DDEBUG -DUSE_PWR_LDO_SUPPLY -DUSE_HAL_DRIVER -DSTM32H743xx -c -I../Core/Inc -I../Drivers/STM32H7xx_HAL_Driver/Inc -I../Drivers/STM32H7xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32H7xx/Include -I../Drivers/CMSIS/Include -I../TouchGFX/App -I../TouchGFX/target/generated -I../TouchGFX/target -O0 -ffunction-sections -fdata-sections -fno-exceptions -fno-rtti -fno-use-cxa-atexit -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-TouchGFX-2f-generated-2f-gui_generated-2f-src-2f-screenturns_screen

clean-TouchGFX-2f-generated-2f-gui_generated-2f-src-2f-screenturns_screen:
	-$(RM) ./TouchGFX/generated/gui_generated/src/screenturns_screen/ScreenTurnsViewBase.cyclo ./TouchGFX/generated/gui_generated/src/screenturns_screen/ScreenTurnsViewBase.d ./TouchGFX/generated/gui_generated/src/screenturns_screen/ScreenTurnsViewBase.o ./TouchGFX/generated/gui_generated/src/screenturns_screen/ScreenTurnsViewBase.su

.PHONY: clean-TouchGFX-2f-generated-2f-gui_generated-2f-src-2f-screenturns_screen

