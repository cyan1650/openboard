################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../drivers/B91/lib/src/emi.c \
../drivers/B91/lib/src/plic.c \
../drivers/B91/lib/src/pm.c \
../drivers/B91/lib/src/pm_32k_rc.c \
../drivers/B91/lib/src/pm_32k_xtal.c \
../drivers/B91/lib/src/rf.c \
../drivers/B91/lib/src/swire.c \
../drivers/B91/lib/src/sys.c \
../drivers/B91/lib/src/trng.c 

OBJS += \
./drivers/B91/lib/src/emi.o \
./drivers/B91/lib/src/plic.o \
./drivers/B91/lib/src/pm.o \
./drivers/B91/lib/src/pm_32k_rc.o \
./drivers/B91/lib/src/pm_32k_xtal.o \
./drivers/B91/lib/src/rf.o \
./drivers/B91/lib/src/swire.o \
./drivers/B91/lib/src/sys.o \
./drivers/B91/lib/src/trng.o 

C_DEPS += \
./drivers/B91/lib/src/emi.d \
./drivers/B91/lib/src/plic.d \
./drivers/B91/lib/src/pm.d \
./drivers/B91/lib/src/pm_32k_rc.d \
./drivers/B91/lib/src/pm_32k_xtal.d \
./drivers/B91/lib/src/rf.d \
./drivers/B91/lib/src/swire.d \
./drivers/B91/lib/src/sys.d \
./drivers/B91/lib/src/trng.d 


# Each subdirectory must supply rules for building sources it contributes
drivers/B91/lib/src/%.o: ../drivers/B91/lib/src/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Andes C Compiler'
	$(CROSS_COMPILE)gcc -DCHIP_TYPE=CHIP_TYPE_9518 -D__PROJECT_B91_MODULE__=1 -I"/cygdrive/E/promotion/gitlab/opensdk/telink_mboard_ble_sdk" -I../drivers -I"/cygdrive/E/promotion/gitlab/opensdk/telink_mboard_ble_sdk/3rd-party/freertos-V5/include" -I"/cygdrive/E/promotion/gitlab/opensdk/telink_mboard_ble_sdk/3rd-party/freertos-V5/portable/GCC/RISC-V" -I../drivers/B91 -I../vendor/Common -I../common -O2 -flto -g3 -Wall -mcpu=d25f -ffunction-sections -fdata-sections -c -fmessage-length=0 -fno-builtin -fomit-frame-pointer -fno-strict-aliasing -fshort-wchar -fuse-ld=bfd -fpack-struct -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d) $(@:%.o=%.o)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


