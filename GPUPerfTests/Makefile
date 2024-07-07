# BUILD CONFIGURATION
CC := gcc
CXX := g++
LD := g++
SHADERC := glslangValidator
CFLAGS += $(CPPFLAGS) $(INC_FILES)
CXXFLAGS += $(CPPFLAGS) $(INC_FILES) -std=c++11
LDFLAGS = -lvulkan -lpthread
LDFLAGS_GUI = -lGL -lglfw -lfreetype -lz `pkg-config --libs gtk+-3.0` -ldl
DEFINES =

# OUTPUT
OUT_DIR = x64
BUILD_DIR = $(OUT_DIR)/build
BINARY = $(OUT_DIR)/GPUPerfTests

# LIBRARIES
LIBRARY_VULKAN = ../Libraries/Vulkan
LIBRARY_IMGUI = ../Libraries/imgui-1.88
LIBRARY_FREETYPE = ../Libraries/freetype-2.12.1
LIBRARY_GLEW = ../Libraries/glew-2.1.0
LIBRARY_GLFW = ../Libraries/glfw-3.3.2.bin.WIN64
LIBRARY_LIBSPNG = ../Libraries/libspng-0.7.2
LIBRARY_NATIVEFILEDIALOGEXTENDED = ../Libraries/nativefiledialog-extended-1.0.1
LINKER_EXTRA_LIBS = $(LIBRARY_NATIVEFILEDIALOGEXTENDED)/build/src/libnfd.a $(LIBRARY_GLEW)/lib/Linux/libGLEW.a

# EMBEDDED RESOURCES
BINARIZER = ../AssetBinarizer/x64/AssetBinarizer

SHADER_DIR = $(SRC_DIR)/shaders
ICON_DIR = icon
LANG_DIR = $(SRC_DIR)/lang
FONT_DIR = font
LICENSE_FILE = ../LICENSE
ABOUT_FILE = ../ABOUT.md
CHANGELOG_FILE = ../CHANGELOG.md

EMBEDDED_RESOURCES_DIR = embedded_resources
EMBEDDED_SHADERS_HEADER = $(EMBEDDED_RESOURCES_DIR)/ab_embedded_shaders.h
EMBEDDED_LANG_HEADER = $(EMBEDDED_RESOURCES_DIR)/ab_embedded_languages.h
EMBEDDED_ICONS_HEADER = $(EMBEDDED_RESOURCES_DIR)/ab_embedded_icons.h
EMBEDDED_FONTS_HEADER = $(EMBEDDED_RESOURCES_DIR)/ab_embedded_fonts.h
EMBEDDED_TEXTS_HEADER = $(EMBEDDED_RESOURCES_DIR)/ab_embedded_texts.h

# SOURCE FILES
SRC_DIR = src
INC_DIR = include $(EMBEDDED_RESOURCES_DIR) $(LIBRARY_VULKAN)/include

IMGUI_SRC = $(LIBRARY_IMGUI)/include
LIBSPNG_SRC = $(LIBRARY_LIBSPNG)/src

IMGUI_SRC_FILES = $(shell find "$(IMGUI_SRC)" -type f -name '*.cpp' -or -name '*.c')
IMGUI_OBJ_FILES = $(patsubst $(IMGUI_SRC)/%,$(BUILD_DIR)/%.o,$(IMGUI_SRC_FILES))

LIBSPNG_SRC_FILES = $(shell find "$(LIBSPNG_SRC)" -type f -name '*.cpp' -or -name '*.c')
LIBSPNG_OBJ_FILES = $(patsubst $(LIBSPNG_SRC)/%,$(BUILD_DIR)/%.o,$(LIBSPNG_SRC_FILES))

ALL_SRC_FILES = $(shell find "$(SRC_DIR)" -type f -name '*.cpp' -or -name '*.c')
GUI_FILES = $(shell find "$(SRC_DIR)/gui" -type f -name '*.cpp' -or -name '*.c')
OBJ_FILES = $(patsubst $(SRC_DIR)/%,$(BUILD_DIR)/%.o,$(SRC_FILES))
INC_FILES = $(addprefix -I,$(INC_DIR))
SHADER_FILES = $(shell find "$(SHADER_DIR)" -type f -name '*.comp')
SHADER_BINS = $(patsubst $(SHADER_DIR)/%.comp,$(BUILD_DIR)/%.spv,$(SHADER_FILES))

# CLI-ONLY BUILD
ifeq ($(CLI),1)
    DEFINES += -D_CLI
    SRC_FILES = $(filter-out $(GUI_FILES),$(ALL_SRC_FILES))
else
	LDFLAGS += $(LINKER_EXTRA_LIBS) $(LDFLAGS_GUI)
	INC_DIR += $(LIBRARY_LIBSPNG)/include $(LIBRARY_IMGUI)/include $(LIBRARY_GLEW)/include $(LIBRARY_GLFW)/include $(LIBRARY_FREETYPE)/include $(LIBRARY_NATIVEFILEDIALOGEXTENDED)/src/include
    SRC_FILES = $(ALL_SRC_FILES)
	OBJ_FILES += $(IMGUI_OBJ_FILES) $(LIBSPNG_OBJ_FILES)
endif

.PHONY: build

build: | prepare $(BINARY)

clean:
	rm -rf $(OUT_DIR)
	rm -rf $(EMBEDDED_RESOURCES_DIR)

prepare:
	$(foreach var,$(OBJ_FILES),mkdir -p $(dir $(var));)
	mkdir -p $(EMBEDDED_RESOURCES_DIR)

$(EMBEDDED_SHADERS_HEADER): $(SHADER_BINS)
	$(BINARIZER) -v -o "$(EMBEDDED_SHADERS_HEADER)" "$(BUILD_DIR)/*.spv"

$(EMBEDDED_LANG_HEADER):
	$(BINARIZER) -v -o "$(EMBEDDED_LANG_HEADER)" "$(LANG_DIR)/*.lang"

$(EMBEDDED_ICONS_HEADER):
	$(BINARIZER) -v -o "$(EMBEDDED_ICONS_HEADER)" "$(ICON_DIR)/*.png"

$(EMBEDDED_FONTS_HEADER):
	$(BINARIZER) -v -o "$(EMBEDDED_FONTS_HEADER)" "$(FONT_DIR)/*.ttf"

$(EMBEDDED_TEXTS_HEADER):
	$(BINARIZER) -v -o "$(EMBEDDED_TEXTS_HEADER)" "$(LICENSE_FILE)" "$(ABOUT_FILE)" "$(CHANGELOG_FILE)"

prep_headers: $(EMBEDDED_SHADERS_HEADER) $(EMBEDDED_LANG_HEADER) $(EMBEDDED_ICONS_HEADER) $(EMBEDDED_FONTS_HEADER) $(EMBEDDED_TEXTS_HEADER)

compile_objs: $(OBJ_FILES)

$(BINARY): | compile_objs
	$(LD) -o $(BINARY) $(OBJ_FILES) $(LDFLAGS)

$(BUILD_DIR)/%.cpp.o: $(SRC_DIR)/%.cpp prep_headers
	$(CXX) $(CXXFLAGS) $(DEFINES) -c $< -o $@

$(BUILD_DIR)/%.c.o: $(SRC_DIR)/%.c prep_headers
	$(CC) $(CFLAGS) $(DEFINES) -c $< -o $@

$(BUILD_DIR)/%.spv: $(SHADER_DIR)/%.comp
	$(SHADERC) -V -o $@ $<

$(BUILD_DIR)/%.cpp.o: $(IMGUI_SRC)/%.cpp
	$(CXX) $(CXXFLAGS) -I$(LIBRARY_IMGUI)/include -I$(LIBRARY_FREETYPE)/include -c $< -o $@

$(BUILD_DIR)/%.c.o: $(LIBSPNG_SRC)/%.c
	$(CC) $(CFLAGS) -I$(LIBRARY_LIBSPNG)/include -c $< -o $@