cargo build --release -p wasmtime-c-api
   c++ ../async.cpp \
       -I ../../artifacts/include \
       -I ../../crates/c-api/include \
       ../../target/release/libwasmtime.a \
       -std=c++11 \
       -lpthread -ldl -lm \
       -o async

