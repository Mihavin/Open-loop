################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../TouchGFX/generated/images/src/__generated/image_alternate_theme_images_backgrounds_480x272_puzzle-90deg.cpp \
../TouchGFX/generated/images/src/__generated/image_alternate_theme_images_widgets_boxprogress_thick_tiny.cpp \
../TouchGFX/generated/images/src/__generated/image_alternate_theme_images_widgets_gauge_tiny_backgrounds_active_filled.cpp \
../TouchGFX/generated/images/src/__generated/image_alternate_theme_images_widgets_gauge_tiny_fillers_swoop.cpp \
../TouchGFX/generated/images/src/__generated/image_alternate_theme_images_widgets_gauge_tiny_needles_rough.cpp 

OBJS += \
./TouchGFX/generated/images/src/__generated/image_alternate_theme_images_backgrounds_480x272_puzzle-90deg.o \
./TouchGFX/generated/images/src/__generated/image_alternate_theme_images_widgets_boxprogress_thick_tiny.o \
./TouchGFX/generated/images/src/__generated/image_alternate_theme_images_widgets_gauge_tiny_backgrounds_active_filled.o \
./TouchGFX/generated/images/src/__generated/image_alternate_theme_images_widgets_gauge_tiny_fillers_swoop.o \
./TouchGFX/generated/images/src/__generated/image_alternate_theme_images_widgets_gauge_tiny_needles_rough.o 

CPP_DEPS += \
./TouchGFX/generated/images/src/__generated/image_alternate_theme_images_backgrounds_480x272_puzzle-90deg.d \
./TouchGFX/generated/images/src/__generated/image_alternate_theme_images_widgets_boxprogress_thick_tiny.d \
./TouchGFX/generated/images/src/__generated/image_alternate_theme_images_widgets_gauge_tiny_backgrounds_active_filled.d \
./TouchGFX/generated/images/src/__generated/image_alternate_theme_images_widgets_gauge_tiny_fillers_swoop.d \
./TouchGFX/generated/images/src/__generated/image_alternate_theme_images_widgets_gauge_tiny_needles_rough.d 


# Each subdirectory must supply rules for building sources it contributes
TouchGFX/generated/images/src/__generated/%.o TouchGFX/generated/images/src/__generated/%.su TouchGFX/generated/images/src/__generated/%.cyclo: ../TouchGFX/generated/images/src/__generated/%.cpp TouchGFX/generated/images/src/__generated/subdir.mk
	arm-none-eabi-g++ "$<" -mcpu=cortex-m7 -std=gnu++14 -g3 -DDEBUG -DUSE_PWR_LDO_SUPPLY -DUSE_HAL_DRIVER -DSTM32H743xx -c -I../Core/Inc -I../Drivers/STM32H7xx_HAL_Driver/Inc -I../Drivers/STM32H7xx_HAL_Driver/Inc/Legacy -I../Middlewares/Third_Party/FreeRTOS/Source/include -I../Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2 -I../Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F -I../Drivers/CMSIS/Device/ST/STM32H7xx/Include -I../Drivers/CMSIS/Include -I../TouchGFX/App -I../TouchGFX/target/generated -I../TouchGFX/target -I../Middlewares/ST/touchgfx/framework/include -I../TouchGFX/generated/fonts/include -I../TouchGFX/generated/gui_generated/include -I../TouchGFX/generated/images/include -I../TouchGFX/generated/texts/include -I../TouchGFX/generated/videos/include -I../TouchGFX/gui/include -O0 -ffunction-sections -fdata-sections -fno-exceptions -fno-rtti -fno-use-cxa-atexit -Wall -femit-class-debug-always -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-TouchGFX-2f-generated-2f-images-2f-src-2f-__generated

clean-TouchGFX-2f-generated-2f-images-2f-src-2f-__generated:
	-$(RM) ./TouchGFX/generated/images/src/__generated/image_alternate_theme_images_backgrounds_480x272_puzzle-90deg.cyclo ./TouchGFX/generated/images/src/__generated/image_alternate_theme_images_backgrounds_480x272_puzzle-90deg.d ./TouchGFX/generated/images/src/__generated/image_alternate_theme_images_backgrounds_480x272_puzzle-90deg.o ./TouchGFX/generated/images/src/__generated/image_alternate_theme_images_backgrounds_480x272_puzzle-90deg.su ./TouchGFX/generated/images/src/__generated/image_alternate_theme_images_widgets_boxprogress_thick_tiny.cyclo ./TouchGFX/generated/images/src/__generated/image_alternate_theme_images_widgets_boxprogress_thick_tiny.d ./TouchGFX/generated/images/src/__generated/image_alternate_theme_images_widgets_boxprogress_thick_tiny.o ./TouchGFX/generated/images/src/__generated/image_alternate_theme_images_widgets_boxprogress_thick_tiny.su ./TouchGFX/generated/images/src/__generated/image_alternate_theme_images_widgets_gauge_tiny_backgrounds_active_filled.cyclo ./TouchGFX/generated/images/src/__generated/image_alternate_theme_images_widgets_gauge_tiny_backgrounds_active_filled.d ./TouchGFX/generated/images/src/__generated/image_alternate_theme_images_widgets_gauge_tiny_backgrounds_active_filled.o ./TouchGFX/generated/images/src/__generated/image_alternate_theme_images_widgets_gauge_tiny_backgrounds_active_filled.su ./TouchGFX/generated/images/src/__generated/image_alternate_theme_images_widgets_gauge_tiny_fillers_swoop.cyclo ./TouchGFX/generated/images/src/__generated/image_alternate_theme_images_widgets_gauge_tiny_fillers_swoop.d ./TouchGFX/generated/images/src/__generated/image_alternate_theme_images_widgets_gauge_tiny_fillers_swoop.o ./TouchGFX/generated/images/src/__generated/image_alternate_theme_images_widgets_gauge_tiny_fillers_swoop.su ./TouchGFX/generated/images/src/__generated/image_alternate_theme_images_widgets_gauge_tiny_needles_rough.cyclo ./TouchGFX/generated/images/src/__generated/image_alternate_theme_images_widgets_gauge_tiny_needles_rough.d ./TouchGFX/generated/images/src/__generated/image_alternate_theme_images_widgets_gauge_tiny_needles_rough.o ./TouchGFX/generated/images/src/__generated/image_alternate_theme_images_widgets_gauge_tiny_needles_rough.su

.PHONY: clean-TouchGFX-2f-generated-2f-images-2f-src-2f-__generated

