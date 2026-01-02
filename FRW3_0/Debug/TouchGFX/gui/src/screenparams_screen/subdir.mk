################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../TouchGFX/gui/src/screenparams_screen/ScreenParamsPresenter.cpp \
../TouchGFX/gui/src/screenparams_screen/ScreenParamsView.cpp 

OBJS += \
./TouchGFX/gui/src/screenparams_screen/ScreenParamsPresenter.o \
./TouchGFX/gui/src/screenparams_screen/ScreenParamsView.o 

CPP_DEPS += \
./TouchGFX/gui/src/screenparams_screen/ScreenParamsPresenter.d \
./TouchGFX/gui/src/screenparams_screen/ScreenParamsView.d 


# Each subdirectory must supply rules for building sources it contributes
TouchGFX/gui/src/screenparams_screen/%.o TouchGFX/gui/src/screenparams_screen/%.su TouchGFX/gui/src/screenparams_screen/%.cyclo: ../TouchGFX/gui/src/screenparams_screen/%.cpp TouchGFX/gui/src/screenparams_screen/subdir.mk
	arm-none-eabi-g++ "$<" -mcpu=cortex-m7 -std=gnu++14 -g3 -DDEBUG -DUSE_PWR_LDO_SUPPLY -DUSE_HAL_DRIVER -DSTM32H743xx -c -I../Core/Inc -I../Drivers/STM32H7xx_HAL_Driver/Inc -I../Drivers/STM32H7xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32H7xx/Include -I../Drivers/CMSIS/Include -I../TouchGFX/App -I../TouchGFX/target/generated -I../TouchGFX/target -O0 -ffunction-sections -fdata-sections -fno-exceptions -fno-rtti -fno-use-cxa-atexit -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-TouchGFX-2f-gui-2f-src-2f-screenparams_screen

clean-TouchGFX-2f-gui-2f-src-2f-screenparams_screen:
	-$(RM) ./TouchGFX/gui/src/screenparams_screen/ScreenParamsPresenter.cyclo ./TouchGFX/gui/src/screenparams_screen/ScreenParamsPresenter.d ./TouchGFX/gui/src/screenparams_screen/ScreenParamsPresenter.o ./TouchGFX/gui/src/screenparams_screen/ScreenParamsPresenter.su ./TouchGFX/gui/src/screenparams_screen/ScreenParamsView.cyclo ./TouchGFX/gui/src/screenparams_screen/ScreenParamsView.d ./TouchGFX/gui/src/screenparams_screen/ScreenParamsView.o ./TouchGFX/gui/src/screenparams_screen/ScreenParamsView.su

.PHONY: clean-TouchGFX-2f-gui-2f-src-2f-screenparams_screen

