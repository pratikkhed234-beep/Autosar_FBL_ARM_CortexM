CC      := arm-none-eabi-gcc
OBJCOPY := arm-none-eabi-objcopy
SIZE    := arm-none-eabi-size

TARGET    := autosar_fbl
BUILD_DIR := build

SRCS := \
	startup_ARMCM4.c \
	main.c \
	App/BootManager/BootManager.c \
	MCAL/Can/Can.c \
	MCAL/Fls/Fls.c \
	BSW/Dcm/Dcm.c

INCLUDES := \
	-I. \
	-IApp/BootManager \
	-IMCAL/Can \
	-IMCAL/Fls \
	-IBSW/Dcm \
	-IBSW/CanTp/isotp

MCU_FLAGS := -mcpu=cortex-m4 -mthumb -mfloat-abi=soft
CFLAGS    := $(MCU_FLAGS) $(INCLUDES) -O2 -Wall -fdata-sections -ffunction-sections -std=c99
LDFLAGS   := $(MCU_FLAGS) -TConfig/bootloader_partition.ld -Wl,-Map=$(BUILD_DIR)/$(TARGET).map,--gc-sections

OBJS := $(addprefix $(BUILD_DIR)/, $(SRCS:.c=.o))

.PHONY: all clean init_dirs

all: init_dirs $(BUILD_DIR)/$(TARGET).elf $(BUILD_DIR)/$(TARGET).bin $(BUILD_DIR)/$(TARGET).hex

init_dirs:
	@cmd /c "if not exist $(BUILD_DIR)\App\BootManager mkdir $(BUILD_DIR)\App\BootManager"
	@cmd /c "if not exist $(BUILD_DIR)\MCAL\Can mkdir $(BUILD_DIR)\MCAL\Can"
	@cmd /c "if not exist $(BUILD_DIR)\MCAL\Fls mkdir $(BUILD_DIR)\MCAL\Fls"
	@cmd /c "if not exist $(BUILD_DIR)\BSW\Dcm mkdir $(BUILD_DIR)\BSW\Dcm"

$(BUILD_DIR)/%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/$(TARGET).elf: $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o $@
	$(SIZE) $@

$(BUILD_DIR)/$(TARGET).bin: $(BUILD_DIR)/$(TARGET).elf
	$(OBJCOPY) -O binary $< $@

$(BUILD_DIR)/$(TARGET).hex: $(BUILD_DIR)/$(TARGET).elf
	$(OBJCOPY) -O ihex $< $@

clean:
	@cmd /c "if exist $(BUILD_DIR) rd /s /q $(BUILD_DIR)"
