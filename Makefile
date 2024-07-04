NATIVEFILEDIALOGEXTENDED_DIR=Libraries/nativefiledialog-extended-1.0.1
ASSETBINARIZER_DIR=AssetBinarizer
GPUPERFTESTS_DIR=GPUPerfTests

.PHONY: build

build: libs assetbinarizer
	echo "Building GPUPerfTests"
	make -C "$(GPUPERFTESTS_DIR)"

assetbinarizer:
	echo "Building AssetBinarizer"
	make -C "$(ASSETBINARIZER_DIR)"

libs: nativefiledialogextended
	echo "Building Libraries"

nativefiledialogextended:
	echo "Building NativeFileDialog-Extended"
	cd "$(NATIVEFILEDIALOGEXTENDED_DIR)" && chmod +x build.sh && ./build.sh

clean:
	rm -rf "$(NATIVEFILEDIALOGEXTENDED_DIR)/build"
	make -C "$(ASSETBINARIZER_DIR)" clean
	make -C "$(GPUPERFTESTS_DIR)" clean