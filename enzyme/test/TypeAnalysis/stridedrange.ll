; RUN: if [ %llvmver -lt 16 ]; then %opt < %s %loadEnzyme -print-type-analysis -type-analysis-func=caller -o /dev/null | FileCheck %s; fi
; RUN: %opt < %s %newLoadEnzyme -passes="print-type-analysis" -type-analysis-func=caller -S | FileCheck %s

; Verify compact strided TypeTree ranges parse and remain queryable past
; MaxTypeOffset (default 500). The attribute encodes floats at
; 8 + 4*k for k in 0..1000 (last element byte 4004) beside an i64 header.

target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

define float @caller(ptr "enzyme_type"="{[-1]:Pointer, [-1,0]:Integer, [-1,8+4*1000]:Float@float}" %c) {
entry:
  %h = load i64, ptr %c, align 8
  %data = getelementptr inbounds i8, ptr %c, i64 8
  %first = load float, ptr %data, align 4
  %lastp = getelementptr inbounds i8, ptr %c, i64 4004
  %last = load float, ptr %lastp, align 4
  %sum = fadd float %first, %last
  ret float %sum
}

; CHECK: caller - {} |{[-1]:Pointer, [-1,0]:Integer, [-1,8+4*1000]:Float@float}:{}
; CHECK: ptr %c: {[-1]:Pointer, [-1,0]:Integer, [-1,8+4*1000]:Float@float}
; CHECK:   %last = load float, ptr %lastp, align 4: {[-1]:Float@float}
