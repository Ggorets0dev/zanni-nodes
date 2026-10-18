TARGET ?= 
BIN_OUTPUT ?= ./bin

build: $(BIN_OUTPUT)
	$(if $(strip $(TARGET)),,$(error TARGET is required. Available: linux, openwrt))
	@if [ "$(TARGET)" = "linux" ]; then \
		cmake -S node-linux -B node-linux/build -DCMAKE_BUILD_TYPE=Release && \
		cmake --build node-linux/build --parallel; \
	elif [ "$(TARGET)" = "openwrt" ]; then \
		echo "OpenWRT is not support yet"; \
	else \
		echo "Unknown TARGET: $(TARGET)"; exit 1; \
	fi
	cp node-linux/build/zanni-node-linux bin/ZanniGateLinux

clean:
	@if [ "$(TARGET)" = "linux" ]; then \
		rm -rf node-linux/build; \
	elif [ "$(TARGET)" = "openwrt" ]; then \
		node-openwrt/build; \
	else \
		make clean TARGET=linux; \
		make clean TARGET=openwrt; \
	fi

$(BIN_OUTPUT):
	mkdir -p $(BIN_OUTPUT)

.PHONY: build clean
