/*
Example of compiling, instantiating, and linking two WebAssembly modules
together.

You can build using cmake:

mkdir build && cd build && cmake .. && cmake --build . --target wasmtime-linking
*/

#define _POSIX_C_SOURCE 200809L
#include <time.h>
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <wasi.h>
#include <wasm.h>
#include <wasmtime.h>


#define MIN(a, b) ((a) < (b) ? (a) : (b))

static void exit_with_error(const char *message, wasmtime_error_t *error,
                            wasm_trap_t *trap);
static void read_wat_file(wasm_engine_t *engine, wasm_byte_vec_t *bytes,
                          const char *file);

typedef struct {
    uint8_t* data;
    size_t size;
} WasmFile;

WasmFile read_wasm_file(const char* filename) {
    WasmFile result = { NULL, 0 };
    FILE* file = fopen(filename, "rb");
    if (!file) {
        fprintf(stderr, "Failed to open file: %s\n", filename);
        return result;
    }

    // get file size 
    fseek(file, 0, SEEK_END);
    result.size = ftell(file);
    rewind(file);

    // allocate memory  
    result.data = (uint8_t*)malloc(result.size);
    if (!result.data) {
        fprintf(stderr, "Failed to allocate memory\n");
        fclose(file);
        return result;
    }

    if (fread(result.data, 1, result.size, file) != result.size) {
        fprintf(stderr, "Failed to read file\n");
        free(result.data);
        result.data = NULL;
        result.size = 0;
    }

    fclose(file);
    return result;
}

static inline uint64_t now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

int main() {
  //read wasm from file
  //WasmFile wf = read_wasm_file("examples/fibonacci.wasm");
  //WasmFile wf = read_wasm_file("examples/hash.wasm");
  WasmFile wf = read_wasm_file("examples/pb_datamining_correlation.wasm");
  // Set up our context
  wasm_engine_t *engine = wasm_engine_new();
  assert(engine != NULL);
  wasmtime_store_t *store = wasmtime_store_new(engine, NULL, NULL);
  assert(store != NULL);
  wasmtime_context_t *context = wasmtime_store_context(store);

  wasm_byte_vec_t linking1_wasm;
  linking1_wasm.data = reinterpret_cast<wasm_byte_t*>(wf.data);
  linking1_wasm.size = wf.size;

  // Compile our two modules
  wasmtime_error_t *error;
  wasmtime_module_t *linking1_module = NULL;
  error = wasmtime_module_new(engine, (uint8_t *)linking1_wasm.data,
                              linking1_wasm.size, &linking1_module);
  if (error != NULL)
    exit_with_error("failed to compile linking1", error, NULL);
  wasm_byte_vec_delete(&linking1_wasm);

  // Configure WASI and store it within our `wasmtime_store_t`
  wasi_config_t *wasi_config = wasi_config_new();
  assert(wasi_config);
  wasi_config_inherit_argv(wasi_config);
  wasi_config_inherit_env(wasi_config);
  //wasi_config_inherit_stdin(wasi_config);
  //wasi_config_inherit_stdout(wasi_config);
  //wasi_config_inherit_stderr(wasi_config);

  /*char *parameter = "12";
  wasm_byte_vec_t* input = (wasm_byte_vec_t*) malloc(sizeof(wasm_byte_vec_t));
  input->size = strlen(parameter);
  input->data = (wasm_byte_t*) malloc(input->size);
  memcpy(input->data, parameter, input->size);
  wasi_config_set_stdin_bytes(wasi_config, input);*/

  wasm_trap_t *trap = NULL;
  error = wasmtime_context_set_wasi(context, wasi_config);
  if (error != NULL)
    exit_with_error("failed to instantiate wasi", NULL, trap);

  // Create our linker which will be linking our modules together, and then add
  // our WASI instance to it.
  wasmtime_linker_t *linker = wasmtime_linker_new(engine);
  error = wasmtime_linker_define_wasi(linker);
  if (error != NULL)
    exit_with_error("failed to link wasi", error, NULL);


  // Instantiate `linking1` with the linker now that `linking2` is defined
  wasmtime_instance_t linking1;
  error = wasmtime_linker_instantiate(linker, context, linking1_module,
                                      &linking1, &trap);
  if (error != NULL || trap != NULL)
    exit_with_error("failed to instantiate linking1", error, trap);

  // Lookup our `run` export function
  wasmtime_extern_t run;
  bool ok = wasmtime_instance_export_get(context, &linking1, "_start", 6, &run);
  assert(ok);
  assert(run.kind == WASMTIME_EXTERN_FUNC);
  uint64_t start = now_ns();
  error = wasmtime_func_call(context, &run.of.func, NULL, 0, NULL, 0, &trap);
  uint64_t end = now_ns();

  uint64_t diff_ns = end - start;
  printf("Execution time: %llu ns (%.3f us)\n",
           (unsigned long long)diff_ns,
           diff_ns / 1000.0);

  if (error != NULL || trap != NULL)
    exit_with_error("failed to call run", error, trap);

  // Clean up after ourselves at this point
  wasmtime_linker_delete(linker);
  wasmtime_module_delete(linking1_module);
  wasmtime_store_delete(store);
  wasm_engine_delete(engine);
  return 0;
}

static void read_wat_file(wasm_engine_t *engine, wasm_byte_vec_t *bytes,
                          const char *filename) {
  wasm_byte_vec_t wat;
  // Load our input file to parse it next
  FILE *file = fopen(filename, "r");
  if (!file) {
    printf("> Error loading file!\n");
    exit(1);
  }
  fseek(file, 0L, SEEK_END);
  size_t file_size = ftell(file);
  wasm_byte_vec_new_uninitialized(&wat, file_size);
  fseek(file, 0L, SEEK_SET);
  if (fread(wat.data, file_size, 1, file) != 1) {
    printf("> Error loading module!\n");
    exit(1);
  }
  fclose(file);

  // Parse the wat into the binary wasm format
  wasmtime_error_t *error = wasmtime_wat2wasm(wat.data, wat.size, bytes);
  if (error != NULL)
    exit_with_error("failed to parse wat", error, NULL);
  wasm_byte_vec_delete(&wat);
}

static void exit_with_error(const char *message, wasmtime_error_t *error,
                            wasm_trap_t *trap) {
  fprintf(stderr, "error: %s\n", message);
  wasm_byte_vec_t error_message;
  if (error != NULL) {
    wasmtime_error_message(error, &error_message);
    wasmtime_error_delete(error);
  } else {
    wasm_trap_message(trap, &error_message);
    wasm_trap_delete(trap);
  }
  fprintf(stderr, "%.*s\n", (int)error_message.size, error_message.data);
  wasm_byte_vec_delete(&error_message);
  exit(1);
}
