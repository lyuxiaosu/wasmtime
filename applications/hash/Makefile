WASI_SDK_PATH=/my_mount/wasi-sdk-25.0-x86_64-linux

WASMCC=${WASI_SDK_PATH}/bin/clang --sysroot=${WASI_SDK_PATH}/share/wasi-sysroot -v
#WASMCFLAGS=${CFLAGS}

# See https://lld.llvm.org/WebAssembly.html
#WASMLDFLAGS=${LDFLAGS} -Wl,--allow-undefined,-z,stack-size=32768,--threads=1
WASMLDFLAGS=-Wl,--no-entry

.PHONY: all
all: \
	hash.out \
	hash.wasm 

hash.wasm: main.c
	@${WASMCC} -O3 ${WASMCFLAGS} ${WASMLDFLAGS} $^ -o $@

hash.out: main.c
	${CC} -O3 $^ -o $@

# hash.wasm.out: hash.wasm.c
# 	${CC} ${CFLAGS} ${LDFLAGS} -Wl,-e,w2c_main -I. $^ -o $@

.PHONY: clean
clean:
	@rm -f hash.wasm
	@rm -f hash.cwasm
	@rm -f hash.wasm.out
	@rm -f hash.wasm.c
	@rm -f hash.out
	@rm -f hash.aot
	@rm -f bench.csv

# Serverless style app that uses stdin and stdout
# echo "10" | make run
.PHONY: run_wasmtime_jit
run_wasmtime_jit: hash.wasm
	@echo "40" | wasmtime hash.wasm

.PHONY: run_wasmtime_aot
run_wasmtime_aot: hash.cwasm
	@echo "40" | wasmtime run --allow-precompiled hash.cwasm

.PHONY: run_wasm3
run_wasm3: hash.wasm
	@echo "40" | wasm3 hash.wasm

.PHONY: run_wamr_int
run_wamr_int: hash.wasm
	@echo "40" | iwasm hash.wasm

.PHONY: run_wamr_aot
run_wamr_aot: hash.aot
	@echo "40" | iwasm hash.aot

.PHONY: run_native
run_native: hash.out
	@./hash.out

bench.csv: hash.wasm hash.cwasm hash.out hash.aot
	hyperfine -w 10 --export-csv bench.csv \
		-n hash_native       'echo "40" | ./hash.out' \
		-n hash_wasmtime_jit 'echo "40" | wasmtime run hash.wasm' \
		-n hash_wasmtime_aot 'echo "40" | wasmtime run --allow-precompiled hash.cwasm' \
		-n hash_wamr_int     'echo "40" | iwasm hash.wasm' \
		-n hash_wamr_aot     'echo "40" | iwasm hash.aot' \
		-n hash_wasm3        'echo "40" | wasm3 hash.wasm'
