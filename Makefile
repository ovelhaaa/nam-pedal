BUILD_TYPE ?= Debug
CLOCK_PROFILE ?= PERFORMANCE_480MHZ
BUILD_DIR ?= build

.PHONY: all bootloader bringup xip clean

all: bootloader bringup xip

bootloader:
	cmake -S . -B $(BUILD_DIR)/bootloader -G Ninja -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) -DNAM_IMAGE=bootloader -DCLOCK_PROFILE=$(CLOCK_PROFILE)
	cmake --build $(BUILD_DIR)/bootloader

bringup:
	cmake -S . -B $(BUILD_DIR)/bringup -G Ninja -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) -DNAM_IMAGE=bringup -DCLOCK_PROFILE=$(CLOCK_PROFILE)
	cmake --build $(BUILD_DIR)/bringup

xip:
	cmake -S . -B $(BUILD_DIR)/xip -G Ninja -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) -DNAM_IMAGE=xip -DCLOCK_PROFILE=PERFORMANCE_480MHZ
	cmake --build $(BUILD_DIR)/xip

clean:
	cmake -E remove_directory $(BUILD_DIR)
