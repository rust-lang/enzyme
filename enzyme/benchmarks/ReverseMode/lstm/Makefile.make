# RUN: cd %S && LD_LIBRARY_PATH="%bldpath:$LD_LIBRARY_PATH" PTR="%ptr" BENCH="%bench" BENCHLINK="%blink" LOAD="%loadEnzyme" LOADCLANG="%loadClangEnzyme" ENZYME="%enzyme" make -B lstm-raw.ll results.json -f %s

.PHONY: clean

dir := $(abspath $(lastword $(MAKEFILE_LIST))/../../../..)

clean:
	rm -f *.ll *.o results.txt results.json
	cargo +enzyme clean

$(dir)/benchmarks/ReverseMode/lstm/target/release/liblstm.a: src/lib.rs Cargo.toml
	RUSTFLAGS="-Z autodiff=Enable,LooseTypes" cargo +enzyme rustc --release --lib --crate-type=staticlib

%-unopt.ll: %.cpp
	clang++ $(BENCH) $(PTR) $^ -pthread -O2 -fno-vectorize -fno-slp-vectorize -ffast-math -fno-unroll-loops -o $@ -S -emit-llvm

%-raw.ll: %-unopt.ll
	opt $^ $(LOAD) $(ENZYME) -o $@ -S

%-opt.ll: %-raw.ll
	opt $^ -o $@ -S

lstm.o: lstm-opt.ll $(dir)/benchmarks/ReverseMode/lstm/target/release/liblstm.a
	clang++ -pthread -O2 $^ -o $@ $(BENCHLINK) -lm

results.json: lstm.o
	numactl -C 1 ./$^
