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

install-service:
	@if [ "$$(id -u)" -ne 0 ]; then echo "Error: install-service must be run as root (e.g. sudo make install-service)"; exit 1; fi
	install -D -m 755 bin/ZanniGateLinux /usr/local/bin/zanni-node-linux
	install -D -m 644 node-linux/services/zanni-node-linux.service /etc/systemd/system/zanni-node-linux.service
	mkdir -p /etc/zannigate
	@if [ ! -f /etc/zanni-node/config.json ]; then \
		install -m 644 core/config/config.example.json /etc/zanni-node/config.json; \
		echo "Installed sample config to /etc/zanni-node/config.json"; \
	fi
	systemctl daemon-reload
	@echo "Service installed successfully. Enable and start with: sudo systemctl enable --now zanni-node-linux"

.PHONY: build clean install-service
