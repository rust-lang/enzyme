# RUN: cd %S && LD_LIBRARY_PATH="%bldpath:$LD_LIBRARY_PATH" PTR="%ptr" BENCH="%bench" BENCHLINK="%blink" LOAD="%loadEnzyme" LOADCLANG="%loadClangEnzyme" ENZYME="%enzyme" make -B results.json -f %s

.PHONY: clean

dir := $(abspath $(lastword $(MAKEFILE_LIST))/../../../..)

clean:
	rm -f *.ll *.o results.txt results.json
	cargo +enzyme clean

$(dir)/benchmarks/ReverseMode/gmm/target/release/libgmmrs.a: src/lib.rs Cargo.toml
	RUSTFLAGS="-Z autodiff=Enable" cargo +enzyme rustc --release --lib --crate-type=staticlib --features=libm

gmm.o: gmm.cpp $(dir)/benchmarks/ReverseMode/gmm/target/release/libgmmrs.a
	/home/manuel/prog/rust-middle/build/x86_64-unknown-linux-gnu/llvm/build/bin/clang++ $(LOADCLANG) $(BENCH) -O3 -fno-math-errno $^ $(BENCHLINK) -lm -o $@

results.json: gmm.o
	numactl -C 1 ./$^
