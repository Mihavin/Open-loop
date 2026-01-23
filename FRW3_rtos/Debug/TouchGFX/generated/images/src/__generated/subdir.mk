################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../TouchGFX/generated/images/src/__generated/image_alternate_theme_images_backgrounds_480x272_puzzle-90deg.cpp 

OBJS += \
./TouchGFX/generated/images/src/__generated/image_alternate_theme_images_backgrounds_480x272_puzzle-90deg.o 

CPP_DEPS += \
./TouchGFX/generated/images/src/__generated/image_alternate_theme_images_backgrounds_480x272_puzzle-90deg.d 


# Each subdirectory must supply rules for building sources it contributes
TouchGFX/generated/images/src/__generated/%.o TouchGFX/generated/images/src/__generated/%.su TouchGFX/generated/images/src/__generated/%.cyclo: ../TouchGFX/generated/images/src/__generated/%.cpp TouchGFX/generated/images/src/__generated/subdir.mk
	arm-none-eabi-g++ "$<" -mcpu=cortex-m7 -std=gnu++14 -g -c -O0 -ffunction-sections -fdata-sections -fno-exceptions -fno-rtti -fno-use-cxa-atexit -Wall -femit-class-debug-always -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-TouchGFX-2f-generated-2f-images-2f-src-2f-__generated

clean-TouchGFX-2f-generated-2f-images-2f-src-2f-__generated:
	-$(RM) ./TouchGFX/generated/images/src/__generated/image_alternate_theme_images_backgrounds_480x272_puzzle-90deg.cyclo ./TouchGFX/generated/images/src/__generated/image_alternate_theme_images_backgrounds_480x272_puzzle-90deg.d ./TouchGFX/generated/images/src/__generated/image_alternate_theme_images_backgrounds_480x272_puzzle-90deg.o ./TouchGFX/generated/images/src/__generated/image_alternate_theme_images_backgrounds_480x272_puzzle-90deg.su

.PHONY: clean-TouchGFX-2f-generated-2f-images-2f-src-2f-__generated

