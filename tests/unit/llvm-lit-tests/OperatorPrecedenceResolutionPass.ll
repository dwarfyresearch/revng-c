;
; Copyright rev.ng Labs Srl. See LICENSE.md for details.
;

; RUN: %revngopt %s -operatorprecedence-resolution -language=c -S -o - | FileCheck %s --check-prefix=C-LANG
; RUN: %revngopt %s -operatorprecedence-resolution -language=nop -S -o - | FileCheck %s --check-prefix=NOP-LANG
;
; Ensures that OperatorPrecedenceResolutionPass has correctly parenthesized the expression
; according to the operator precedence priority.

; int parenthesize_exp0(int *p) {
;   return *(uint32_t*)((uintptr_t)(p) + 1);
; }
define i32 @parenthesize_exp0(i32* %0) !revng.tags !0 {
  %2 = ptrtoint i32* %0 to i64
  %3 = add i64 %2, 1
  ; CHECK: %4 = call i64 @parentheses(i64 %3)
  ; CHECK-NEXT: %5 = inttoptr i64 %4 to i32*
  ; CHECK-NEXT: %6 = load i32, i32* %5
  ; CHECK-NEXT: ret i32 %6
  %4 = inttoptr i64 %3 to i32*
  %5 = load i32, i32* %4, align 4
  ret i32 %5
}

; int parenthesize_exp1(int a, int b, int c) {
;   return a / b / c;
; }
define i32 @parenthesize_exp1(i32 %0, i32 %1, i32 %2) !revng.tags !0 {
  %4 = sdiv i32 %0, %1
  ; CHECK-NOT: %5 = call i32 @parentheses.1(i32 %4)
  %5 = sdiv i32 %4, %2
  ret i32 %5
}

; int parenthesize_exp2(int a, int b, int c) {
;   return a / (b / c);
; }
define i32 @parenthesize_exp2(i32 %0, i32 %1, i32 %2) !revng.tags !0 {
  %4 = sdiv i32 %1, %2
  ; CHECK: %5 = call i32 @parentheses.1(i32 %4)
  ; CHECK-NEXT: %6 = sdiv i32 %0, %5
  %5 = sdiv i32 %0, %4
  ret i32 %5
}

; int parenthesize_exp3(int a, int b, int c, int d, int e) {
;   return 5 + a * (b + c) + d / (2 + e);
; }
; Only associativity is taken into account in the NOP language, since all the operators
; have the same precedence. This implies that the reconstructed expression for NOP is the following:
; (5 + a * b + c) + (d / (2 + e))
; Note that here the parentheses are needed to specify that the division comes before the addition.
define i32 @parenthesize_exp3(i32 %0, i32 %1, i32 %2, i32 %3, i32 %4) !revng.tags !0 {
  %6 = add i32 %2, %1
  ; C-LANG: %7 = call i32 @parentheses.1(i32 %6)
  ; C-LANG-NEXT: %8 = mul i32 %7, %0
  %7 = mul i32 %6, %0
  %8 = add i32 %7, 5
  %9 = add i32 %4, 2
  ; C-LANG: %11 = call i32 @parentheses.1(i32 %10)
  ; C-LANG-NEXT: %12 = sdiv i32 %3, %11
  ; NOP-LANG: %10 = call i32 @parentheses(i32 %9)
  ; NOP-LANG-NEXT: %11 = sdiv i32 %3, %10
  %10 = sdiv i32 %3, %9
  ; NOP-LANG: %12 = call i32 @parentheses(i32 %11)
  ; NOP-LANG-NEXT: %13 = add i32 %8, %12
  %11 = add i32 %8, %10
  ret i32 %11
}

!0 = !{!"Isolated"}
