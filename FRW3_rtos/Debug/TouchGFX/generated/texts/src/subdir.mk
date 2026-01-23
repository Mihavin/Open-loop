################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../TouchGFX/generated/texts/src/LanguageGb.cpp \
../TouchGFX/generated/texts/src/Texts.cpp \
../TouchGFX/generated/texts/src/TypedTextDatabase.cpp 

OBJS += \
./TouchGFX/generated/texts/src/LanguageGb.o \
./TouchGFX/generated/texts/src/Texts.o \
./TouchGFX/generated/texts/src/TypedTextDatabase.o 

CPP_DEPS += \
./TouchGFX/generated/texts/src/LanguageGb.d \
./TouchGFX/generated/texts/src/Texts.d \
./TouchGFX/generated/texts/src/TypedTextDatabase.d 


# Each subdirectory must supply rules for building sources it contributes
TouchGFX/generated/texts/src/%.o TouchGFX/generated/texts/src/%.su TouchGFX/generated/texts/src/%.cyclo: ../TouchGFX/generated/texts/src/%.cpp TouchGFX/generated/texts/src/subdir.mk
	arm-none-eabi-g++ "$<" -mcpu=cortex-m7 -std=gnu++14 -g -c -O0 -ffunction-sections -fdata-sections -fno-exceptions -fno-rtti -fno-use-cxa-atexit -Wall -femit-class-debug-always -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-TouchGFX-2f-generated-2f-texts-2f-src

clean-TouchGFX-2f-generated-2f-texts-2f-src:
	-$(RM) ./TouchGFX/generated/texts/src/LanguageGb.cyclo ./TouchGFX/generated/texts/src/LanguageGb.d ./TouchGFX/generated/texts/src/LanguageGb.o ./TouchGFX/generated/texts/src/LanguageGb.su ./TouchGFX/generated/texts/src/Texts.cyclo ./TouchGFX/generated/texts/src/Texts.d ./TouchGFX/generated/texts/src/Texts.o ./TouchGFX/generated/texts/src/Texts.su ./TouchGFX/generated/texts/src/TypedTextDatabase.cyclo ./TouchGFX/generated/texts/src/TypedTextDatabase.d ./TouchGFX/generated/texts/src/TypedTextDatabase.o ./TouchGFX/generated/texts/src/TypedTextDatabase.su

.PHONY: clean-TouchGFX-2f-generated-2f-texts-2f-src

