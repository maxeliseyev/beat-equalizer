# One-command build for Beat Equalizer.
#
#   make                 Release tests + plugin formats for this OS
#   make debug           Same, Debug
#   make test            DSP tests only
#   make vst3|au|standalone
#   make run             Open Standalone
#   make icon FILE=art.png   App icon from a 1024x1024 PNG (macOS)
#   make app             Signed .app and .dmg in dist/ (macOS)
#   make release-dmg     Same, notarised and stapled (macOS)
#   make where           Artefact and install paths
#   make clean
#
# CONFIG=debug|release (default: release)

CONFIG ?= release
PRESET := $(CONFIG)
BUILD_DIR := build/$(CONFIG)
VERSION := $(shell tr -d ' \n' < VERSION)

ifeq ($(CONFIG),debug)
ARTEFACT_CONFIG := Debug
else
ARTEFACT_CONFIG := Release
endif

ARTEFACT_DIR := $(BUILD_DIR)/src/plugin/BeatEqualizer_artefacts/$(ARTEFACT_CONFIG)
PLUGIN_NAME := Beat Equalizer

ifeq ($(OS),Windows_NT)
HOST := windows
PLUGIN_TARGETS := BeatEqualizer_VST3 BeatEqualizer_Standalone
STANDALONE_BIN := $(ARTEFACT_DIR)/Standalone/$(PLUGIN_NAME).exe
else
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
HOST := macos
PLUGIN_TARGETS := BeatEqualizer_VST3 BeatEqualizer_AU BeatEqualizer_Standalone
STANDALONE_BIN := $(ARTEFACT_DIR)/Standalone/$(PLUGIN_NAME).app
else
HOST := linux
PLUGIN_TARGETS := BeatEqualizer_VST3 BeatEqualizer_Standalone
STANDALONE_BIN := $(ARTEFACT_DIR)/Standalone/$(PLUGIN_NAME)
endif
endif

.PHONY: all debug plugin test vst3 au standalone run icon app release-dmg where version configure clean help

all: test plugin where

debug:
	$(MAKE) CONFIG=debug all

configure:
	cmake --preset $(PRESET)

plugin: configure
	cmake --build --preset $(PRESET) --target $(PLUGIN_TARGETS)

test: configure
	cmake --build --preset $(PRESET) --target beat_tests
	ctest --test-dir $(BUILD_DIR) --output-on-failure

test-gui: configure
	cmake --build --preset $(PRESET) --target beat_gui_tests
	"$(BUILD_DIR)/tests/beat_gui_tests"

vst3: configure
	cmake --build --preset $(PRESET) --target BeatEqualizer_VST3

au: configure
ifeq ($(HOST),macos)
	cmake --build --preset $(PRESET) --target BeatEqualizer_AU
else
	$(error AU is macOS-only)
endif

standalone: configure
	cmake --build --preset $(PRESET) --target BeatEqualizer_Standalone

run: standalone
ifeq ($(HOST),macos)
	open "$(STANDALONE_BIN)"
else ifeq ($(HOST),windows)
	"$(STANDALONE_BIN)"
else
	"$(STANDALONE_BIN)"
endif

# Иконка: картинка 1024x1024, уже в форме иконки macOS. Геометрия и весь
# порядок подписи — docs/packaging-macos.md.
icon:
ifeq ($(HOST),macos)
	@scripts/make-icon.sh "$(FILE)"
else
	$(error icon is macOS-only)
endif

# Подписанный .app и образ диска. Без нотаризации: на своей машине хватает,
# наружу нужен release-dmg.
app:
ifeq ($(HOST),macos)
	CONFIG=$(CONFIG) scripts/package-macos.sh
else
	$(error app is macOS-only)
endif

release-dmg:
ifeq ($(HOST),macos)
	CONFIG=$(CONFIG) scripts/package-macos.sh --notarize
else
	$(error release-dmg is macOS-only)
endif

version:
	@echo $(VERSION)

where:
	@echo "version:   $(VERSION)"
	@echo "host:      $(HOST)"
	@echo "config:    $(CONFIG)"
	@echo "build:     $(BUILD_DIR)"
	@echo "artefacts: $(ARTEFACT_DIR)"
	@echo "VST3:      $(ARTEFACT_DIR)/VST3/$(PLUGIN_NAME).vst3"
ifeq ($(HOST),macos)
	@echo "AU:        $(ARTEFACT_DIR)/AU/$(PLUGIN_NAME).component"
	@echo "install:   ~/Library/Audio/Plug-Ins/VST3/"
	@echo "           ~/Library/Audio/Plug-Ins/Components/"
endif
	@echo "standalone: $(STANDALONE_BIN)"
ifeq ($(HOST),macos)
	@echo "dmg:       dist/$(PLUGIN_NAME) $(VERSION).dmg"
endif

clean:
	rm -rf build/debug build/release dist

help:
	@echo "make              Release tests + plugins for $(HOST)"
	@echo "make debug        Debug tests + plugins"
	@echo "make test         beat_tests"
	@echo "make test-gui     render oscilloscope to PNG"
	@echo "make vst3|au|standalone"
	@echo "make run          Open Standalone"
ifeq ($(HOST),macos)
	@echo "make icon FILE=x.png  App icon from a 1024x1024 PNG"
	@echo "make app          Signed .app and .dmg in dist/"
	@echo "make release-dmg  Same, notarised and stapled"
endif
	@echo "make where        Print output paths"
	@echo "make version      Print VERSION"
	@echo "make clean        Remove build/debug, build/release and dist"
	@echo "CONFIG=debug|release (default release)"
