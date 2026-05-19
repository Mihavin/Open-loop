################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../TouchGFX/gui/src/screentorque_screen/ScreenTorquePresenter.cpp \
../TouchGFX/gui/src/screentorque_screen/ScreenTorqueView.cpp 

OBJS += \
./TouchGFX/gui/src/screentorque_screen/ScreenTorquePresenter.o \
./TouchGFX/gui/src/screentorque_screen/ScreenTorqueView.o 

CPP_DEPS += \
./TouchGFX/gui/src/screentorque_screen/ScreenTorquePresenter.d \
./TouchGFX/gui/src/screentorque_screen/ScreenTorqueView.d 


# Each subdirectory must supply rules for building sources it contributes
TouchGFX/gui/src/screentorque_screen/%.o TouchGFX/gui/src/screentorque_screen/%.su TouchGFX/gui/src/screentorque_screen/%.cyclo: ../TouchGFX/gui/src/screentorque_screen/%.cpp TouchGFX/gui/src/screentorque_screen/subdir.mk
	arm-none-eabi-g++ "$<" -mcpu=cortex-m7 -std=gnu++14 -g3 -DDEBUG -DUSE_PWR_LDO_SUPPLY -DUSE_HAL_DRIVER -DSTM32H743xx -c -I../Core/Inc -I../Drivers/STM32H7xx_HAL_Driver/Inc -I../Drivers/STM32H7xx_HAL_Driver/Inc/Legacy -I../Middlewares/Third_Party/FreeRTOS/Source/include -I../Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2 -I../Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F -I../Drivers/CMSIS/Device/ST/STM32H7xx/Include -I../Drivers/CMSIS/Include -I../TouchGFX/App -I../TouchGFX/target/generated -I../TouchGFX/target -I../Middlewares/ST/touchgfx/framework/include -I../TouchGFX/generated/fonts/include -I../TouchGFX/generated/gui_generated/include -I../TouchGFX/generated/images/include -I../TouchGFX/generated/texts/include -I../TouchGFX/generated/videos/include -I../TouchGFX/gui/include -I../Drivers/CMSIS/RTOS2/Include -O0 -ffunction-sections -fdata-sections -fno-exceptions -fno-rtti -fno-use-cxa-atexit -Wall -femit-class-debug-always -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-TouchGFX-2f-gui-2f-src-2f-screentorque_screen

clean-TouchGFX-2f-gui-2f-src-2f-screentorque_screen:
	-$(RM) ./TouchGFX/gui/src/screentorque_screen/ScreenTorquePresenter.cyclo ./TouchGFX/gui/src/screentorque_screen/ScreenTorquePresenter.d ./TouchGFX/gui/src/screentorque_screen/ScreenTorquePresenter.o ./TouchGFX/gui/src/screentorque_screen/ScreenTorquePresenter.su ./TouchGFX/gui/src/screentorque_screen/ScreenTorqueView.cyclo ./TouchGFX/gui/src/screentorque_screen/ScreenTorqueView.d ./TouchGFX/gui/src/screentorque_screen/ScreenTorqueView.o ./TouchGFX/gui/src/screentorque_screen/ScreenTorqueView.su

.PHONY: clean-TouchGFX-2f-gui-2f-src-2f-screentorque_screen

