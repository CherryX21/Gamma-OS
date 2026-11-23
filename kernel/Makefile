.RECIPEPREFIX := >

CXX := clang++
LD  := ld.lld
HOSTCXX ?= g++

OVMF ?= $(firstword $(wildcard $(OVMF_FD) /usr/share/ovmf/OVMF.fd /usr/share/OVMF/OVMF.fd))
LIMINE_DIR ?= ../limine

BUILD ?= build
ISO ?= iso_root
EXTRA_FLAGS ?=
QEMU_ACCEL ?= -enable-kvm -cpu host

ifdef FAULT
EXTRA_FLAGS += -DGAMMA_FAULT_$(FAULT)
endif

INC_DIRS := -Isrc -Isrc/arch -Isrc/mm -Isrc/drivers -Isrc/fs -Isrc/shell -Isrc/lib

CXXFLAGS := -target x86_64-unknown-none-elf -std=c++20 -O2 -g -Wall -Wextra \
  -ffreestanding -fno-builtin -fno-exceptions -fno-rtti -fno-stack-protector -fno-pic \
  -fno-threadsafe-statics -fno-use-cxa-atexit \
  -mno-red-zone -mcmodel=kernel -mgeneral-regs-only -MMD -MP $(INC_DIRS) $(EXTRA_FLAGS)
LDFLAGS := -nostdlib -static -z max-page-size=0x1000 -T linker.ld

SRCS := $(shell find src -name '*.cpp')
OBJS := $(SRCS:src/%.cpp=$(BUILD)/%.o)

QEMU_COMMON := -bios $(OVMF) -m 256M -net none $(QEMU_ACCEL)

.PHONY: all image run test qemu-test check clean

all: $(BUILD)/kernel.elf

$(BUILD)/%.o: src/%.cpp
> @mkdir -p $(dir $@)
> $(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/kernel.elf: $(OBJS) linker.ld
> $(LD) $(LDFLAGS) $(OBJS) -o $@

image: $(BUILD)/kernel.elf limine.conf
> rm -rf $(ISO)
> mkdir -p $(ISO)/boot $(ISO)/EFI/BOOT
> cp $(BUILD)/kernel.elf $(ISO)/boot/kernel
> cp limine.conf $(ISO)/limine.conf
> cp $(LIMINE_DIR)/BOOTX64.EFI $(ISO)/EFI/BOOT/BOOTX64.EFI

run: image
> qemu-system-x86_64 $(QEMU_COMMON) -drive format=raw,file=fat:rw:$(ISO) -serial stdio

test:
> @mkdir -p build
> $(HOSTCXX) -std=c++20 -O1 -g -Wall -Wextra $(INC_DIRS) tests/test_bitmap.cpp src/mm/bitmap.cpp -o build/test_bitmap
> ./build/test_bitmap
> $(HOSTCXX) -std=c++20 -O1 -g -Wall -Wextra -DGAMMA_HOST_TEST $(INC_DIRS) tests/test_heap.cpp src/mm/heap.cpp -o build/test_heap
> ./build/test_heap
> $(HOSTCXX) -std=c++20 -O1 -g -Wall -Wextra -DGAMMA_HOST_TEST $(INC_DIRS) tests/test_fs.cpp src/fs/fs.cpp -o build/test_fs
> ./build/test_fs

qemu-test:
> $(MAKE) BUILD=build-test ISO=iso_test EXTRA_FLAGS="-DGAMMA_AUTOTEST" image
> timeout 90 qemu-system-x86_64 $(QEMU_COMMON) -drive format=raw,file=fat:rw:iso_test -display none -serial stdio < /dev/null -no-reboot -device isa-debug-exit,iobase=0xf4,iosize=0x04; rc=$$?; if [ $$rc -eq 1 ]; then echo "QEMU TEST PASSED"; else echo "QEMU TEST FAILED (exit code $$rc)"; exit 1; fi

check: test qemu-test

clean:
> rm -rf build build-test iso_root iso_test

-include $(OBJS:.o=.d)
