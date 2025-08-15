WASI_SDK_PATH=/my_mount/wasi-sdk-25.0-x86_64-linux

WASMCC=${WASI_SDK_PATH}/bin/clang --sysroot=${WASI_SDK_PATH}/share/wasi-sysroot -v
#WASMCFLAGS=${CFLAGS}

# See https://lld.llvm.org/WebAssembly.html
#WASMLDFLAGS=${LDFLAGS} -Wl,--allow-undefined,-z,stack-size=32768,--threads=1
WASMLDFLAGS=-Wl,--no-entry

.PHONY: all
all: \
	fibonacci.wasm \
	fibonacci \

user.wasm: user.c
	@${WASMCC} ${WASMCFLAGS} ${WASMLDFLAGS} $^ -o $@

fibonacci.wasm: fibonacci.c
	@${WASMCC} ${WASMCFLAGS} ${WASMLDFLAGS} $^ -o $@

fibonacci: fibonacci.c
	gcc $^ -o $@

.PHONY: clean
clean:
	@rm -f user.wasm


