################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../stack/ble/host/smp/smp.c \
../stack/ble/host/smp/smp_alg.c \
../stack/ble/host/smp/smp_peripheral.c \
../stack/ble/host/smp/smp_secureConn.c \
../stack/ble/host/smp/smp_storage.c 

OBJS += \
./stack/ble/host/smp/smp.o \
./stack/ble/host/smp/smp_alg.o \
./stack/ble/host/smp/smp_peripheral.o \
./stack/ble/host/smp/smp_secureConn.o \
./stack/ble/host/smp/smp_storage.o 

C_DEPS += \
./stack/ble/host/smp/smp.d \
./stack/ble/host/smp/smp_alg.d \
./stack/ble/host/smp/smp_peripheral.d \
./stack/ble/host/smp/smp_secureConn.d \
./stack/ble/host/smp/smp_storage.d 


# Each subdirectory must supply rules for building sources it contributes
stack/ble/host/smp/%.o: ../stack/ble/host/smp/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Andes C Compiler'
	$(CROSS_COMPILE)gcc -DCHIP_TYPE=CHIP_TYPE_9518 -D__PROJECT_B91_MODULE__=1 -I"/cygdrive/E/promotion/gitlab/opensdk/telink_mboard_ble_sdk" -I../drivers -I"/cygdrive/E/promotion/gitlab/opensdk/telink_mboard_ble_sdk/3rd-party/freertos-V5/include" -I"/cygdrive/E/promotion/gitlab/opensdk/telink_mboard_ble_sdk/3rd-party/freertos-V5/portable/GCC/RISC-V" -I../drivers/B91 -I../vendor/Common -I../common -O2 -flto -g3 -Wall -mcpu=d25f -ffunction-sections -fdata-sections -c -fmessage-length=0 -fno-builtin -fomit-frame-pointer -fno-strict-aliasing -fshort-wchar -fuse-ld=bfd -fpack-struct -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d) $(@:%.o=%.o)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


