# Copyright (c) 2017, ZHENGZHOU YUTONG BUS CO,.LTD.
# Author: Stevin Liang <liangzl@yutong.com>

CFLAGS := $(shell pkg-config --cflags --libs gstreamer-1.0 gstreamer-audio-1.0 gstreamer-video-1.0)

grecorder : grecorder.o
	@echo "[LD]    $@"
	@gcc $< -o $@ $(CFLAGS)

%.o : %.c
	@echo "[CC]    $@"
	@gcc $< -c -o $@ $(CFLAGS)

.PHONY: clean

clean: 	
	@echo "[RM]    grecorder"
	@rm -rf grecorder.o grecorder

