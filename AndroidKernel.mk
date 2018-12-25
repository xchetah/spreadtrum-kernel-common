KERNEL_OUT := $(TARGET_OUT_INTERMEDIATES)/KERNEL
KERNEL_CONFIG := $(KERNEL_OUT)/.config
KERNEL_MODULES_OUT := $(TARGET_ROOT_OUT)/lib/modules

ifeq ($(TARGET_ARCH),arm64)
KERNEL_ARCH_ := arm64
KERNEL_CROSS_COMPILE_ := aarch64-linux-android-
else
KERNEL_ARCH_ := arm
KERNEL_CROSS_COMPILE_ := arm-eabi-
endif

ifeq ($(strip $(CONFIG_64KERNEL_32FRAMEWORK)),true)
KERNEL_ARCH_ := arm64
KERNEL_CROSS_COMPILE_ := $(FIX_CROSS_COMPILE)
endif

JOBS := $(shell if [ $(cat /proc/cpuinfo | grep processor | wc -l) -gt 8 ]; then echo 8; else echo 4; fi)

ifeq ($(USES_UNCOMPRESSED_KERNEL),true)
TARGET_PREBUILT_KERNEL := $(KERNEL_OUT)/arch/$(KERNEL_ARCH_)/boot/Image
else
TARGET_PREBUILT_KERNEL := $(KERNEL_OUT)/arch/$(KERNEL_ARCH_)/boot/zImage
endif

$(KERNEL_OUT):
	@echo "==== Start Kernel Compiling ... ===="


$(KERNEL_CONFIG): kernel/arch/$(KERNEL_ARCH_)/configs/$(KERNEL_DEFCONFIG)
	echo "KERNEL_OUT = $(KERNEL_OUT),  KERNEL_DEFCONFIG = $(KERNEL_DEFCONFIG)"
	mkdir -p $(KERNEL_OUT)
	$(MAKE) ARCH=$(KERNEL_ARCH_) -C kernel O=../$(KERNEL_OUT) $(KERNEL_DEFCONFIG)

ifeq ($(TARGET_BUILD_VARIANT),user)
DEBUGMODE := BUILD=no
USER_CONFIG := $(TARGET_OUT)/dummy
TARGET_DEVICE_USER_CONFIG := $(PLATCOMM)/user_diff_config
TARGET_DEVICE_CUSTOM_CONFIG := device/sprd/$(TARGET_DEVICE)/ProjectConfig.mk
TARGET_DEVICE_LOW_RAM_CONFIG := $(PLATCOMM)/low_ram_diff_config
ifeq ($(PRODUCT_RAM),low)
$(USER_CONFIG) : $(KERNEL_CONFIG)
	$(info $(shell ./kernel/scripts/sprd_custom_config_kernel.sh $(KERNEL_CONFIG) $(TARGET_DEVICE_CUSTOM_CONFIG)))
	$(info $(shell ./kernel/scripts/sprd_create_user_config.sh $(KERNEL_CONFIG) $(TARGET_DEVICE_USER_CONFIG)))
	$(info $(shell ./kernel/scripts/sprd_create_user_config.sh $(KERNEL_CONFIG) $(TARGET_DEVICE_LOW_RAM_CONFIG)))
else
$(USER_CONFIG) : $(KERNEL_CONFIG)
	$(info $(shell ./kernel/scripts/sprd_custom_config_kernel.sh $(KERNEL_CONFIG) $(TARGET_DEVICE_CUSTOM_CONFIG)))
	$(info $(shell ./kernel/scripts/sprd_create_user_config.sh $(KERNEL_CONFIG) $(TARGET_DEVICE_USER_CONFIG)))
endif
else
DEBUGMODE := $(DEBUGMODE)
USER_CONFIG  := $(TARGET_OUT)/dummy
TARGET_DEVICE_CUSTOM_CONFIG := device/sprd/$(TARGET_DEVICE)/ProjectConfig.mk
TARGET_DEVICE_LOW_RAM_CONFIG := $(PLATCOMM)/low_ram_diff_config
ifeq ($(PRODUCT_RAM),low)
$(USER_CONFIG) : $(KERNEL_CONFIG)
	$(info $(shell ./kernel/scripts/sprd_custom_config_kernel.sh $(KERNEL_CONFIG) $(TARGET_DEVICE_CUSTOM_CONFIG)))
	$(info $(shell ./kernel/scripts/sprd_create_user_config.sh $(KERNEL_CONFIG) $(TARGET_DEVICE_LOW_RAM_CONFIG)))
else
$(USER_CONFIG) : $(KERNEL_CONFIG)
	$(info $(shell ./kernel/scripts/sprd_custom_config_kernel.sh $(KERNEL_CONFIG) $(TARGET_DEVICE_CUSTOM_CONFIG)))
endif
endif

$(TARGET_PREBUILT_KERNEL) : $(KERNEL_OUT) $(USER_CONFIG)  | $(KERNEL_CONFIG)
	$(MAKE) -C kernel O=../$(KERNEL_OUT) ARCH=$(KERNEL_ARCH_) CROSS_COMPILE=$(KERNEL_CROSS_COMPILE_) headers_install
	$(MAKE) -C kernel O=../$(KERNEL_OUT) ARCH=$(KERNEL_ARCH_) CROSS_COMPILE=$(KERNEL_CROSS_COMPILE_) -j${JOBS}
	$(MAKE) -C kernel O=../$(KERNEL_OUT) ARCH=$(KERNEL_ARCH_) CROSS_COMPILE=$(KERNEL_CROSS_COMPILE_) modules
	@-mkdir -p $(KERNEL_MODULES_OUT)
	@-find $(TARGET_OUT_INTERMEDIATES) -name *.ko ! -name mali.ko | xargs -I{} cp {} $(KERNEL_MODULES_OUT)
	@-find $(KERNEL_MODULES_OUT) -name *.ko ! -name mali.ko -exec $(KERNEL_CROSS_COMPILE_)strip -d --strip-unneeded {} \;

kernelheader:
	mkdir -p $(KERNEL_OUT)
	$(MAKE) ARCH=$(KERNEL_ARCH_) -C kernel O=../$(KERNEL_OUT) headers_install
