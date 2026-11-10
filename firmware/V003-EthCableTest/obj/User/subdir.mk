################################################################################
# MRS Version: 2.5.0
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../User/system.c 

C_DEPS += \
./User/system.d 

CPP_SRCS += \
../User/CDisplay.cpp \
../User/CEncoder.cpp \
../User/CPins.cpp \
../User/main.cpp 

CPP_DEPS += \
./User/CDisplay.d \
./User/CEncoder.d \
./User/CPins.d \
./User/main.d 

OBJS += \
./User/CDisplay.o \
./User/CEncoder.o \
./User/CPins.o \
./User/main.o \
./User/system.o 

DIR_OBJS += \
./User/*.o \

DIR_DEPS += \
./User/*.d \

DIR_EXPANDS += \
./User/*.234r.expand \


# Each subdirectory must supply rules for building sources it contributes
User/%.o: ../User/%.c
	@	riscv-none-embed-gcc -march=rv32ecxw -mabi=ilp32e -msmall-data-limit=0 -msave-restore -fmax-errors=20 -Os -fmessage-length=0 -fsigned-char -ffunction-sections -fdata-sections -fno-common -Wunused -Wuninitialized -g -I"c:/WI/CH32/V003/V003-EthCableTest/User" -std=gnu99 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -c -o "$@" "$<"

User/%.o: ../User/%.cpp
	@	riscv-none-embed-g++ -march=rv32ecxw -mabi=ilp32e -msmall-data-limit=0 -msave-restore -fmax-errors=20 -Os -fmessage-length=0 -fsigned-char -ffunction-sections -fdata-sections -fno-common -Wunused -Wuninitialized -g -DF_CPU=48000000 -I"c:/WI/CH32/V003/V003-EthCableTest/User" -std=c++1y -fabi-version=0 -fno-exceptions -fno-rtti -fno-use-cxa-atexit -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -c -o "$@" "$<"

