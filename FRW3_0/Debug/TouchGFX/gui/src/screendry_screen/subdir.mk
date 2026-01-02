################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../TouchGFX/gui/src/screendry_screen/ScreenDryPresenter.cpp \
../TouchGFX/gui/src/screendry_screen/ScreenDryView.cpp 

OBJS += \
./TouchGFX/gui/src/screendry_screen/ScreenDryPresenter.o \
./TouchGFX/gui/src/screendry_screen/ScreenDryView.o 

CPP_DEPS += \
./TouchGFX/gui/src/screendry_screen/ScreenDryPresenter.d \
./TouchGFX/gui/src/screendry_screen/ScreenDryView.d 


# Each subdirectory must supply rules for building sources it contributes
TouchGFX/gui/src/screendry_screen/%.o TouchGFX/gui/src/screendry_screen/%.su TouchGFX/gui/src/screendry_screen/%.cyclo: ../TouchGFX/gui/src/screendry_screen/%.cpp TouchGFX/gui/src/screendry_screen/subdir.mk
	arm-none-eabi-g++ "$<" -mcpu=cortex-m7 -std=gnu++14 -g3 -DDEBUG -DUSE_PWR_LDO_SUPPLY -DUSE_HAL_DRIVER -DSTM32H743xx -c -I../Core/Inc -I../Drivers/STM32H7xx_HAL_Driver/Inc -I../Drivers/STM32H7xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32H7xx/Include -I../Drivers/CMSIS/Include -I../TouchGFX/App -I../TouchGFX/target/generated -I../TouchGFX/target -O0 -ffunction-sections -fdata-sections -fno-exceptions -fno-rtti -fno-use-cxa-atexit -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-TouchGFX-2f-gui-2f-src-2f-screendry_screen

clean-TouchGFX-2f-gui-2f-src-2f-screendry_screen:
	-$(RM) ./TouchGFX/gui/src/screendry_screen/ScreenDryPresenter.cyclo ./TouchGFX/gui/src/screendry_screen/ScreenDryPresenter.d ./TouchGFX/gui/src/screendry_screen/ScreenDryPresenter.o ./TouchGFX/gui/src/screendry_screen/ScreenDryPresenter.su ./TouchGFX/gui/src/screendry_screen/ScreenDryView.cyclo ./TouchGFX/gui/src/screendry_screen/ScreenDryView.d ./TouchGFX/gui/src/screendry_screen/ScreenDryView.o ./TouchGFX/gui/src/screendry_screen/ScreenDryView.su

.PHONY: clean-TouchGFX-2f-gui-2f-src-2f-screendry_screen

