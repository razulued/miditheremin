################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
C:/Freescale/SDK_2.0_FRDM-K82F/boards/frdmk82f/rtos_examples/freertos_hello/freertos_hello.c 

OBJS += \
./source/freertos_hello.o 

C_DEPS += \
./source/theremin.d 


# Each subdirectory must supply rules for building sources it contributes
source/freertos_hello.o: C:/Freescale/SDK_2.0_FRDM-K82F/boards/frdmk82f/rtos_examples/freertos_hello/freertos_hello.c
	@echo 'Building file: $<'
	@echo 'Invoking: Cross ARM C Compiler'
	arm-none-eabi-gcc -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16 -O0 -fmessage-length=0 -fsigned-char -ffunction-sections -fdata-sections -Wall  -g -DDEBUG -DCPU_MK82FN256VDC15 -DFSL_RTOS_FREE_RTOS -DFRDM_K82F -DFREEDOM -I../../../../../../rtos/freertos_8.2.3/Source/portable/GCC/ARM_CM4F -I../../../../../../rtos/freertos_8.2.3/Source/include -I../../../../../../CMSIS/Include -I../../../../../../devices -I../../../../../../devices/MK82F25615/drivers -I../.. -I../../../../../../devices/MK82F25615/utilities -I../../../../../../rtos/freertos_8.2.3/Source -I../../../.. -I../../../../../../devices/MK82F25615 -std=gnu99 -fno-common  -ffreestanding  -fno-builtin  -mapcs -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" -c -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


