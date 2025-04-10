# RUN: cd %S && LD_LIBRARY_PATH="%bldpath:$LD_LIBRARY_PATH" PTR="%ptr" BENCH="%bench" BENCHLINK="%blink" LOAD="%loadEnzyme" LOADCLANG="%loadClangEnzyme" ENZYME="%enzyme" make -B results.json VERBOSE=1 -f %s

.PHONY: clean

dir := $(abspath $(lastword $(MAKEFILE_LIST))/../../../..)

clean:
	rm -f *.ll *.o results.txt results.json
	cargo +enzyme clean

$(dir)/benchmarks/ReverseMode/ode-real/target/release/libode.a: src/lib.rs Cargo.toml
	RUSTFLAGS="-Z autodiff=Enable,LooseTypes" cargo +enzyme rustc --release --lib --crate-type=staticlib

ode.o: ode.cpp $(dir)/benchmarks/ReverseMode/ode-real/target/release/libode.a
	/home/manuel/prog/rust-middle/build/x86_64-unknown-linux-gnu/llvm/build/bin/clang++ $(LOADCLANG) $(BENCH) -O3 -fno-math-errno $^ $(BENCHLINK) -lm -o $@

results.json: ode.o
	numactl -C 1 ./$^ 1000 | tee $@
	numactl -C 1 ./$^ 1000 >> $@
	numactl -C 1 ./$^ 1000 >> $@
	numactl -C 1 ./$^ 1000 >> $@
	numactl -C 1 ./$^ 1000 >> $@
	numactl -C 1 ./$^ 1000 >> $@
	numactl -C 1 ./$^ 1000 >> $@
	numactl -C 1 ./$^ 1000 >> $@
	numactl -C 1 ./$^ 1000 >> $@
	numactl -C 1 ./$^ 1000 >> $@
