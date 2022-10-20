################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../drivers/B91/flash/flash_mid146085.c 

OBJS += \
./drivers/B91/flash/flash_mid146085.o 

C_DEPS += \
./drivers/B91/flash/flash_mid146085.d 


# Each subdirectory must supply rules for building sources it contributes
drivers/B91/flash/%.o: ../drivers/B91/flash/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Andes C Compiler'
	$(CROSS_COMPILE)gcc -DCHIP_TYPE=CHIP_TYPE_9518 -D__PROJECT_B91_MODULE__=1 -I"/cygdrive/E/promotion/gitlab/opensdk/telink_mboard_ble_sdk" -I../drivers -I"/cygdrive/E/promotion/gitlab/opensdk/telink_mboard_ble_sdk/3rd-party/freertos-V5/include" -I"/cygdrive/E/promotion/gitlab/opensdk/telink_mboard_ble_sdk/3rd-party/freertos-V5/portable/GCC/RISC-V" -I../drivers/B91 -I../vendor/Common -I../common -O2 -flto -g3 -Wall -mcpu=d25f -ffunction-sections -fdata-sections -c -fmessage-length=0 -fno-builtin -fomit-frame-pointer -fno-strict-aliasing -fshort-wchar -fuse-ld=bfd -fpack-struct -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d) $(@:%.o=%.o)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


