# Top-level Makefile (Workbench Makefile Project entry)
# Targets: all / dkm / rtp / clean

ROOT_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))

.PHONY: all common dkm rtp clean

all: common dkm rtp

common:
	$(MAKE) -C $(ROOT_DIR)/common ROOT_DIR=$(ROOT_DIR)

dkm: common
	$(MAKE) -C $(ROOT_DIR)/dkm ROOT_DIR=$(ROOT_DIR)

rtp: common
	$(MAKE) -C $(ROOT_DIR)/rtp ROOT_DIR=$(ROOT_DIR)

clean:
	$(MAKE) -C $(ROOT_DIR)/common clean ROOT_DIR=$(ROOT_DIR)
	$(MAKE) -C $(ROOT_DIR)/dkm clean ROOT_DIR=$(ROOT_DIR)
	$(MAKE) -C $(ROOT_DIR)/rtp clean ROOT_DIR=$(ROOT_DIR)
