/*
Example of instantiating of the WebAssembly module and invoking its exported
function.

You can build using cmake:

mkdir build && cd build && cmake .. && \
  cmake --build . --target wasmtime-interrupt
*/

#include <array>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <future>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <streambuf>
#include <string>
#include <thread>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <wasm.h>
#include <wasmtime.h>

#ifdef _WIN32
static void spawn_interrupt(wasm_engine_t *engine) {
  wasmtime_engine_increment_epoch(engine);
}
#else
#include <pthread.h>
#include <time.h>
template <typename T, void (*fn)(T *)> struct deleter {
  void operator()(T *ptr) { fn(ptr); }
};
template <typename T, void (*fn)(T *)>
using handle = std::unique_ptr<T, deleter<T, fn>>;

static void *helper(void *_engine) {
  wasm_engine_t *engine = (wasm_engine_t*)_engine;
  while(true) {
    struct timespec sleep_dur;
    sleep_dur.tv_sec = 10;
    sleep_dur.tv_nsec = 0;
    printf("sleep for 10 seconds\n");
    nanosleep(&sleep_dur, NULL);
    printf("Sending an interrupt\n");
    wasmtime_engine_increment_epoch(engine);
  }
  return 0;
}

static void spawn_interrupt(wasm_engine_t *engine) {
  pthread_t child;
  int rc = pthread_create(&child, NULL, helper, engine);
  assert(rc == 0);
}
#endif

static void exit_with_error(const char *message, wasmtime_error_t *error,
                            wasm_trap_t *trap);

handle<wasm_engine_t, wasm_engine_delete> create_engine(wasm_config_t *config) {
  assert(config != nullptr);
  wasmtime_config_async_support_set(config, true);
  wasmtime_config_epoch_interruption_set(config, true);
  //wasmtime_config_consume_fuel_set(config, true);
  handle<wasm_engine_t, wasm_engine_delete> engine;
  // this takes ownership of config
  engine.reset(wasm_engine_new_with_config(config));
  assert(engine);
  return engine;
}

handle<wasmtime_store_t, wasmtime_store_delete>
create_store(wasm_engine_t *engine) {
  handle<wasmtime_store_t, wasmtime_store_delete> store;
  store.reset(wasmtime_store_new(engine, nullptr, nullptr));
  assert(store);
  return store;
}

handle<wasmtime_linker_t, wasmtime_linker_delete>
create_linker(wasm_engine_t *engine) {
  handle<wasmtime_linker_t, wasmtime_linker_delete> linker;
  linker.reset(wasmtime_linker_new(engine));
  assert(linker);
  return linker;
}

handle<wasmtime_module_t, wasmtime_module_delete>
compile_wat_module_from_file(wasm_engine_t *engine,
                             const std::string &filename) {
  std::ifstream t(filename);
  std::stringstream buffer;
  buffer << t.rdbuf();
  if (t.bad()) {
    std::cerr << "error reading file: " << filename << std::endl;
    std::exit(1);
  }
  const std::string &content = buffer.str();
  wasm_byte_vec_t wasm_bytes;
  handle<wasmtime_error_t, wasmtime_error_delete> error{
      wasmtime_wat2wasm(content.data(), content.size(), &wasm_bytes)};
  if (error) {
    exit_with_error("failed to parse wat", error.get(), nullptr);
  }
  wasmtime_module_t *mod_ptr = nullptr;
  error.reset(wasmtime_module_new(engine,
                                  reinterpret_cast<uint8_t *>(wasm_bytes.data),
                                  wasm_bytes.size, &mod_ptr));
  wasm_byte_vec_delete(&wasm_bytes);
  handle<wasmtime_module_t, wasmtime_module_delete> mod{mod_ptr};
  if (!mod) {
    exit_with_error("failed to compile module", error.get(), nullptr);
  }
  return mod;
}

int main() {
  // Create a `wasm_store_t` with interrupts enabled
  wasm_config_t *config = wasm_config_new();
  assert(config != NULL);
  handle<wasmtime_error_t, wasmtime_error_delete> error;

  auto engine = create_engine(config);
  assert(engine != NULL);
  //wasmtime_store_t *store = wasmtime_store_new(engine, NULL, NULL);
  auto store = create_store(engine.get());
  assert(store != NULL);
  wasmtime_context_t *context = wasmtime_store_context(store.get());

  // Configure the epoch deadline after which WebAssembly code will yield.
  wasmtime_context_epoch_deadline_async_yield_and_update(context, 1);

  // Read our input file, which in this case is a wasm text file.
  auto compiled_module =
      compile_wat_module_from_file(engine.get(), "examples/interrupt.wat");
  auto linker = create_linker(engine.get());

  // Now instantiate our module using the linker.
  handle<wasmtime_call_future_t, wasmtime_call_future_delete> call_future;
  wasm_trap_t *trap_ptr = nullptr;
  wasmtime_error_t *error_ptr = nullptr;
  wasmtime_instance_t instance;
  call_future.reset(wasmtime_linker_instantiate_async(
      linker.get(), context, compiled_module.get(), &instance, &trap_ptr,
      &error_ptr));
  while (!wasmtime_call_future_poll(call_future.get())) {
    std::cout << "yielding instantiation!" << std::endl;
  }
  error.reset(error_ptr);
  handle<wasm_trap_t, wasm_trap_delete> trap{trap_ptr};
  if (error || trap) {
    exit_with_error("failed to instantiate module", error.get(), trap.get());
  }

  call_future = nullptr;
  linker = nullptr;
  // Lookup our `run` export function
  wasmtime_extern_t run;
  bool ok = wasmtime_instance_export_get(context, &instance, "run", 3, &run);
  assert(ok);
  assert(run.kind == WASMTIME_EXTERN_FUNC);

  //get future
  std::array<wasmtime_val_t, 0> results;
  call_future.reset(wasmtime_func_call_async(
      context, &run.of.func, NULL, 0,
      results.data(), results.size(), &trap_ptr, &error_ptr));
 
  if (error_ptr) {
    error.reset(error_ptr);
    exit_with_error("error during async call", error.get(), nullptr);
  }

  if (trap_ptr) {
    handle<wasm_trap_t, wasm_trap_delete> trap{trap_ptr};
    exit_with_error("trap during async call", nullptr, trap.get());
  }
  // Spawn a thread to send us an interrupt after a period of time.
  spawn_interrupt(engine.get());

  // And call it!
  printf("Entering infinite loop...\n");
  while (!wasmtime_call_future_poll(call_future.get())) {
    std::cout << "Pending..." << std::endl;
    // 这里可以睡眠一段时间，或做其它工作
  }
  printf("Excecution finished\n");
  return 0;
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
