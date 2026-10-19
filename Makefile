# Snap: native Haiku build with the development tools that come with Haiku.
#   make              builds build-haiku/Snap
#   make package      builds artifacts/snap-<version>-<arch>.hpkg
#   make check-host   runs the platform independent tests and checks the
#                     shaders with glslangValidator, on Linux
#   make check        runs the tests on Haiku
#   make test-camera  builds SnapTestCamera, a stand-in camera for testing
# tools/build-cross.sh runs this Makefile with a cross compiler from Linux.
.DEFAULT_GOAL := all
CXX ?= g++
BUILD ?= build-haiku
CPPFLAGS += -Isrc $(CROSS_CPPFLAGS)
CXXFLAGS ?= -O2 -g
CXXFLAGS += -std=c++17 -Wall -Wextra -Wno-multichar -Wno-unused-parameter \
	-Wno-missing-field-initializers
# Resource tools; tools/build-cross.sh points these at host builds.
RC ?= rc
XRES ?= xres
MIMESET ?= mimeset

SRC = $(wildcard src/*.cpp src/*/*.cpp)
OBJ = $(SRC:%.cpp=$(BUILD)/%.o)
# be: interface, app, storage; media: camera, microphone, music; game:
# BSimpleGameSound; GL: BGLView and OpenGL; translation: PNG, WebP;
# tracker: BFilePanel.
LIBS = -lbe -lmedia -lgame -lGL -ltranslation -ltracker

# Every shader in src/effects/shaders goes into the application as a 'GLSL'
# resource named after its file (see src/render/ShaderLibrary.h).
SHADERS = $(sort $(wildcard src/effects/shaders/*.glsl src/effects/shaders/*.vert))

.PHONY: all package check check-host check-shaders icon clean test-camera

all: $(BUILD)/Snap

$(BUILD)/Shaders.rdef: $(SHADERS) Makefile
	mkdir -p $(BUILD)
	id=1000; for shader in $(notdir $(SHADERS)); do \
		echo "resource($$id, \"$$shader\") #'GLSL' import \"$$shader\";"; \
		id=$$((id + 1)); \
	done > $@

$(BUILD)/Snap.rsrc: resources/Snap.rdef $(BUILD)/Shaders.rdef \
		resources/branding/snap-icon.hvif $(SHADERS)
	$(RC) -I resources -I src/effects/shaders -o $@ resources/Snap.rdef \
		$(BUILD)/Shaders.rdef

$(BUILD)/Snap: $(OBJ) $(BUILD)/Snap.rsrc
	$(CXX) $(APP_LDFLAGS) -o $@.new $(OBJ) $(LIBS) $(APP_LDEND)
	$(XRES) -o $@.new $(BUILD)/Snap.rsrc
	$(MIMESET) -f $@.new
	mv $@.new $@

$(BUILD)/%.o: %.cpp
	mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c $< -o $@

# The sound analysis is plain C++ and is tested anywhere.
TEST_SRC = tests/AudioAnalyzerTests.cpp src/audio/AudioAnalyzer.cpp
$(BUILD)/audio_tests: $(TEST_SRC) src/audio/AudioAnalyzer.h
	mkdir -p $(BUILD)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -o $@ $(TEST_SRC)

# A stand-in camera node for machines without one (see tests/TestCamera.cpp).
test-camera: $(BUILD)/SnapTestCamera
$(BUILD)/SnapTestCamera: tests/TestCamera.cpp
	mkdir -p $(BUILD)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(APP_LDFLAGS) -o $@ $< -lbe -lmedia $(APP_LDEND)

check: $(BUILD)/audio_tests
	$(BUILD)/audio_tests

check-shaders:
	bash tools/check-shaders.sh

check-host:
	$(MAKE) BUILD=build-host CXX=g++ check
	bash tools/check-shaders.sh

icon:
	python3 tools/make-icon.py resources/branding/snap-icon.hvif \
		resources/branding/snap-icon-preview.png

package: all
	bash tools/package-haiku.sh

clean:
	rm -rf $(BUILD)

-include $(OBJ:.o=.d)
