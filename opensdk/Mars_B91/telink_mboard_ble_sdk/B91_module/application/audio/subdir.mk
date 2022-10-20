################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../application/audio/adpcm.c \
../application/audio/gl_audio.c \
../application/audio/msbc_decode.c \
../application/audio/msbc_encode.c \
../application/audio/sbc_decode.c \
../application/audio/sbc_encode.c \
../application/audio/tl_audio.c 

OBJS += \
./application/audio/adpcm.o \
./application/audio/gl_audio.o \
./application/audio/msbc_decode.o \
./application/audio/msbc_encode.o \
./application/audio/sbc_decode.o \
./application/audio/sbc_encode.o \
./application/audio/tl_audio.o 

C_DEPS += \
./application/audio/adpcm.d \
./application/audio/gl_audio.d \
./application/audio/msbc_decode.d \
./application/audio/msbc_encode.d \
./application/audio/sbc_decode.d \
./application/audio/sbc_encode.d \
./application/audio/tl_audio.d 


# Each subdirectory must supply rules for building sources it contributes
application/audio/%.o: ../application/audio/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Andes C Compiler'
	$(CROSS_COMPILE)gcc -DCHIP_TYPE=CHIP_TYPE_9518 -D__PROJECT_B91_MODULE__=1 -I"/cygdrive/E/promotion/gitlab/opensdk/telink_mboard_ble_sdk" -I../drivers -I"/cygdrive/E/promotion/gitlab/opensdk/telink_mboard_ble_sdk/3rd-party/freertos-V5/include" -I"/cygdrive/E/promotion/gitlab/opensdk/telink_mboard_ble_sdk/3rd-party/freertos-V5/portable/GCC/RISC-V" -I../drivers/B91 -I../vendor/Common -I../common -O2 -flto -g3 -Wall -mcpu=d25f -ffunction-sections -fdata-sections -c -fmessage-length=0 -fno-builtin -fomit-frame-pointer -fno-strict-aliasing -fshort-wchar -fuse-ld=bfd -fpack-struct -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d) $(@:%.o=%.o)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


