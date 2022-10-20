################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../stack/ble/controller/ll/ll.c \
../stack/ble/controller/ll/ll_adv.c \
../stack/ble/controller/ll/ll_ext_adv.c \
../stack/ble/controller/ll/ll_misc.c \
../stack/ble/controller/ll/ll_pm.c \
../stack/ble/controller/ll/ll_resolvlist.c \
../stack/ble/controller/ll/ll_scan.c \
../stack/ble/controller/ll/ll_whitelist.c 

OBJS += \
./stack/ble/controller/ll/ll.o \
./stack/ble/controller/ll/ll_adv.o \
./stack/ble/controller/ll/ll_ext_adv.o \
./stack/ble/controller/ll/ll_misc.o \
./stack/ble/controller/ll/ll_pm.o \
./stack/ble/controller/ll/ll_resolvlist.o \
./stack/ble/controller/ll/ll_scan.o \
./stack/ble/controller/ll/ll_whitelist.o 

C_DEPS += \
./stack/ble/controller/ll/ll.d \
./stack/ble/controller/ll/ll_adv.d \
./stack/ble/controller/ll/ll_ext_adv.d \
./stack/ble/controller/ll/ll_misc.d \
./stack/ble/controller/ll/ll_pm.d \
./stack/ble/controller/ll/ll_resolvlist.d \
./stack/ble/controller/ll/ll_scan.d \
./stack/ble/controller/ll/ll_whitelist.d 


# Each subdirectory must supply rules for building sources it contributes
stack/ble/controller/ll/%.o: ../stack/ble/controller/ll/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Andes C Compiler'
	$(CROSS_COMPILE)gcc -DCHIP_TYPE=CHIP_TYPE_9518 -D__PROJECT_B91_MODULE__=1 -I"/cygdrive/E/promotion/gitlab/opensdk/telink_mboard_ble_sdk" -I../drivers -I"/cygdrive/E/promotion/gitlab/opensdk/telink_mboard_ble_sdk/3rd-party/freertos-V5/include" -I"/cygdrive/E/promotion/gitlab/opensdk/telink_mboard_ble_sdk/3rd-party/freertos-V5/portable/GCC/RISC-V" -I../drivers/B91 -I../vendor/Common -I../common -O2 -flto -g3 -Wall -mcpu=d25f -ffunction-sections -fdata-sections -c -fmessage-length=0 -fno-builtin -fomit-frame-pointer -fno-strict-aliasing -fshort-wchar -fuse-ld=bfd -fpack-struct -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d) $(@:%.o=%.o)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


