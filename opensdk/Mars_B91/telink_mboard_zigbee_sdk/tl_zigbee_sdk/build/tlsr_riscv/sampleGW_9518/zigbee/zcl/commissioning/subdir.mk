################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
E:/promotion/zigbee/20220802/telink_zigbee_sdk/tl_zigbee_sdk/zigbee/zcl/commissioning/zcl_commissioning.c \
E:/promotion/zigbee/20220802/telink_zigbee_sdk/tl_zigbee_sdk/zigbee/zcl/commissioning/zcl_commissioning_attr.c 

OBJS += \
./zigbee/zcl/commissioning/zcl_commissioning.o \
./zigbee/zcl/commissioning/zcl_commissioning_attr.o 

C_DEPS += \
./zigbee/zcl/commissioning/zcl_commissioning.d \
./zigbee/zcl/commissioning/zcl_commissioning_attr.d 


# Each subdirectory must supply rules for building sources it contributes
zigbee/zcl/commissioning/zcl_commissioning.o: /cygdrive/E/promotion/zigbee/20220802/telink_zigbee_sdk/tl_zigbee_sdk/zigbee/zcl/commissioning/zcl_commissioning.c
	@echo 'Building file: $<'
	@echo 'Invoking: Andes C Compiler'
	$(CROSS_COMPILE)gcc -DMCU_CORE_B91=1 -DCOORDINATOR=1 -D__PROJECT_TL_GW__=1 -I../../../apps/common -I../../../apps/sampleGW -I../../../platform -I../../../platform/riscv -I../../../proj/common -I../../../proj -I../../../zigbee/common/includes -I../../../zigbee/zbapi -I../../../zigbee/bdb/includes -I../../../zigbee/gp -I../../../zigbee/zcl -I../../../zigbee/ota -I../../../zbhci -O2 -mcmodel=small -fpack-struct -fshort-enums -flto -Wall -mcpu=d25f -ffunction-sections -fdata-sections -mext-dsp -mabi=ilp32f  -c -fmessage-length=0  -fomit-frame-pointer -fno-strict-aliasing -fshort-wchar -fuse-ld=bfd -Wno-nonnull-compare -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d) $(@:%.o=%.o)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

zigbee/zcl/commissioning/zcl_commissioning_attr.o: /cygdrive/E/promotion/zigbee/20220802/telink_zigbee_sdk/tl_zigbee_sdk/zigbee/zcl/commissioning/zcl_commissioning_attr.c
	@echo 'Building file: $<'
	@echo 'Invoking: Andes C Compiler'
	$(CROSS_COMPILE)gcc -DMCU_CORE_B91=1 -DCOORDINATOR=1 -D__PROJECT_TL_GW__=1 -I../../../apps/common -I../../../apps/sampleGW -I../../../platform -I../../../platform/riscv -I../../../proj/common -I../../../proj -I../../../zigbee/common/includes -I../../../zigbee/zbapi -I../../../zigbee/bdb/includes -I../../../zigbee/gp -I../../../zigbee/zcl -I../../../zigbee/ota -I../../../zbhci -O2 -mcmodel=small -fpack-struct -fshort-enums -flto -Wall -mcpu=d25f -ffunction-sections -fdata-sections -mext-dsp -mabi=ilp32f  -c -fmessage-length=0  -fomit-frame-pointer -fno-strict-aliasing -fshort-wchar -fuse-ld=bfd -Wno-nonnull-compare -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d) $(@:%.o=%.o)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


