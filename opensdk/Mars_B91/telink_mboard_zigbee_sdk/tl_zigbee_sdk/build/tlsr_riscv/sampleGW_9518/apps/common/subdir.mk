################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
E:/promotion/zigbee/20220802/telink_zigbee_sdk/tl_zigbee_sdk/apps/common/factory_reset.c \
E:/promotion/zigbee/20220802/telink_zigbee_sdk/tl_zigbee_sdk/apps/common/firmwareEncryptChk.c \
E:/promotion/zigbee/20220802/telink_zigbee_sdk/tl_zigbee_sdk/apps/common/main.c \
E:/promotion/zigbee/20220802/telink_zigbee_sdk/tl_zigbee_sdk/apps/common/module_test.c 

OBJS += \
./apps/common/factory_reset.o \
./apps/common/firmwareEncryptChk.o \
./apps/common/main.o \
./apps/common/module_test.o 

C_DEPS += \
./apps/common/factory_reset.d \
./apps/common/firmwareEncryptChk.d \
./apps/common/main.d \
./apps/common/module_test.d 


# Each subdirectory must supply rules for building sources it contributes
apps/common/factory_reset.o: /cygdrive/E/promotion/zigbee/20220802/telink_zigbee_sdk/tl_zigbee_sdk/apps/common/factory_reset.c
	@echo 'Building file: $<'
	@echo 'Invoking: Andes C Compiler'
	$(CROSS_COMPILE)gcc -DMCU_CORE_B91=1 -DCOORDINATOR=1 -D__PROJECT_TL_GW__=1 -I../../../apps/common -I../../../apps/sampleGW -I../../../platform -I../../../platform/riscv -I../../../proj/common -I../../../proj -I../../../zigbee/common/includes -I../../../zigbee/zbapi -I../../../zigbee/bdb/includes -I../../../zigbee/gp -I../../../zigbee/zcl -I../../../zigbee/ota -I../../../zbhci -O2 -mcmodel=small -fpack-struct -fshort-enums -flto -Wall -mcpu=d25f -ffunction-sections -fdata-sections -mext-dsp -mabi=ilp32f  -c -fmessage-length=0  -fomit-frame-pointer -fno-strict-aliasing -fshort-wchar -fuse-ld=bfd -Wno-nonnull-compare -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d) $(@:%.o=%.o)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

apps/common/firmwareEncryptChk.o: /cygdrive/E/promotion/zigbee/20220802/telink_zigbee_sdk/tl_zigbee_sdk/apps/common/firmwareEncryptChk.c
	@echo 'Building file: $<'
	@echo 'Invoking: Andes C Compiler'
	$(CROSS_COMPILE)gcc -DMCU_CORE_B91=1 -DCOORDINATOR=1 -D__PROJECT_TL_GW__=1 -I../../../apps/common -I../../../apps/sampleGW -I../../../platform -I../../../platform/riscv -I../../../proj/common -I../../../proj -I../../../zigbee/common/includes -I../../../zigbee/zbapi -I../../../zigbee/bdb/includes -I../../../zigbee/gp -I../../../zigbee/zcl -I../../../zigbee/ota -I../../../zbhci -O2 -mcmodel=small -fpack-struct -fshort-enums -flto -Wall -mcpu=d25f -ffunction-sections -fdata-sections -mext-dsp -mabi=ilp32f  -c -fmessage-length=0  -fomit-frame-pointer -fno-strict-aliasing -fshort-wchar -fuse-ld=bfd -Wno-nonnull-compare -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d) $(@:%.o=%.o)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

apps/common/main.o: /cygdrive/E/promotion/zigbee/20220802/telink_zigbee_sdk/tl_zigbee_sdk/apps/common/main.c
	@echo 'Building file: $<'
	@echo 'Invoking: Andes C Compiler'
	$(CROSS_COMPILE)gcc -DMCU_CORE_B91=1 -DCOORDINATOR=1 -D__PROJECT_TL_GW__=1 -I../../../apps/common -I../../../apps/sampleGW -I../../../platform -I../../../platform/riscv -I../../../proj/common -I../../../proj -I../../../zigbee/common/includes -I../../../zigbee/zbapi -I../../../zigbee/bdb/includes -I../../../zigbee/gp -I../../../zigbee/zcl -I../../../zigbee/ota -I../../../zbhci -O2 -mcmodel=small -fpack-struct -fshort-enums -flto -Wall -mcpu=d25f -ffunction-sections -fdata-sections -mext-dsp -mabi=ilp32f  -c -fmessage-length=0  -fomit-frame-pointer -fno-strict-aliasing -fshort-wchar -fuse-ld=bfd -Wno-nonnull-compare -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d) $(@:%.o=%.o)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

apps/common/module_test.o: /cygdrive/E/promotion/zigbee/20220802/telink_zigbee_sdk/tl_zigbee_sdk/apps/common/module_test.c
	@echo 'Building file: $<'
	@echo 'Invoking: Andes C Compiler'
	$(CROSS_COMPILE)gcc -DMCU_CORE_B91=1 -DCOORDINATOR=1 -D__PROJECT_TL_GW__=1 -I../../../apps/common -I../../../apps/sampleGW -I../../../platform -I../../../platform/riscv -I../../../proj/common -I../../../proj -I../../../zigbee/common/includes -I../../../zigbee/zbapi -I../../../zigbee/bdb/includes -I../../../zigbee/gp -I../../../zigbee/zcl -I../../../zigbee/ota -I../../../zbhci -O2 -mcmodel=small -fpack-struct -fshort-enums -flto -Wall -mcpu=d25f -ffunction-sections -fdata-sections -mext-dsp -mabi=ilp32f  -c -fmessage-length=0  -fomit-frame-pointer -fno-strict-aliasing -fshort-wchar -fuse-ld=bfd -Wno-nonnull-compare -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d) $(@:%.o=%.o)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


