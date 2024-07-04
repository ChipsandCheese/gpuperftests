BUILD_DIR=x64/build
OUT_DIR=x64
SRC_DIR=src
BINARY=$(OUT_DIR)/AssetBinarizer

SRC_FILES=$(shell find "$(SRC_DIR)" -type f -name '*.cpp' -or -name '*.c')
TEMP_OBJ_FILES=$(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(SRC_FILES))
OBJ_FILES=$(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(TEMP_OBJ_FILES))

CC=gcc
CPPC=g++
LD=g++
CFLAGS=
CPPFLAGS=$(CFLAGS)
LDFLAGS=
DEFINES=

.PHONY: build

build: | prepare $(BINARY)

clean:
	rm -rf $(OUT_DIR)

prepare:
	$(foreach var,$(OBJ_FILES),mkdir -p $(dir $(var));)


$(BINARY): $(OBJ_FILES)
	$(LD) -o $(BINARY) $(OBJ_FILES) $(LDFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	$(CPPC) $(CPPFLAGS) $(DEFINES) -c $< -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) $(DEFINES) -c $< -o $@