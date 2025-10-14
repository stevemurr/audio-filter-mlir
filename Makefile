.PHONY: clean build setup test all help

.DEFAULT_GOAL := help

help: ## Show this help message
	@echo "Usage: make [target]"
	@echo ""
	@echo "Available targets:"
	@grep -E '^[a-zA-Z_-]+:.*?## .*$$' $(MAKEFILE_LIST) | awk 'BEGIN {FS = ":.*?## "}; {printf "  \033[36m%-15s\033[0m %s\n", $$1, $$2}'

all: build ## Build the project (default)

clean: ## Remove all build artifacts
	rm -rf build
	rm -f compile_commands.json

build: ## Build the project
	cmake -B build
	cmake --build build
	@ln -sf build/compile_commands.json compile_commands.json

setup: ## Initial project setup (generate compile_commands.json symlink)
	cmake -B build
	@ln -sf build/compile_commands.json compile_commands.json

test: build ## Build and run tests
	cd build && ctest --output-on-failure

install: build ## Install audio-util to /usr/local/bin
	@if [ -w /usr/local/bin ]; then \
		cp build/audio-util /usr/local/bin/audio-util; \
		echo "✓ Installed audio-util to /usr/local/bin/audio-util"; \
	else \
		echo "Installing to /usr/local/bin (requires sudo):"; \
		sudo cp build/audio-util /usr/local/bin/audio-util; \
		echo "✓ Installed audio-util to /usr/local/bin/audio-util"; \
	fi

uninstall: ## Uninstall audio-util from /usr/local/bin
	@if [ -f /usr/local/bin/audio-util ]; then \
		if [ -w /usr/local/bin ]; then \
			rm /usr/local/bin/audio-util; \
			echo "✓ Uninstalled audio-util"; \
		else \
			sudo rm /usr/local/bin/audio-util; \
			echo "✓ Uninstalled audio-util"; \
		fi \
	else \
		echo "audio-util is not installed"; \
	fi
