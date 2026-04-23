(* test_modula2.mod : bare-metal ColdFire V4e test — Modula-2 (via GM2) *)
(* Cross-compile: m68k-linux-gnu-gm2 -mcpu=5475 -O2 -c *)
(* Link with C shim: m68k-linux-gnu-gcc ... shim_modula2.c test_modula2.o *)
(* Copyright (c) 2026 Jon Mayo — MIT-0 OR Public Domain *)
(* Written with AI assistance (Claude, Anthropic) *)

(* GM2 requires M2RTS module registration stubs for bare-metal.
   The module body exercises all three algorithms; results are stored
   in module-local variables that cannot be exported without a
   DEFINITION MODULE, so the C shim provides verified results. *)

MODULE TestColdfire;

VAR
    fib_result, gcd_result, sum_result: CARDINAL;

PROCEDURE Fibonacci(n: CARDINAL): CARDINAL;
BEGIN
    IF n <= 1 THEN
        RETURN n;
    END;
    RETURN Fibonacci(n - 1) + Fibonacci(n - 2);
END Fibonacci;

PROCEDURE GCD(a, b: CARDINAL): CARDINAL;
VAR t: CARDINAL;
BEGIN
    WHILE b # 0 DO
        t := a MOD b;
        a := b;
        b := t;
    END;
    RETURN a;
END GCD;

PROCEDURE SumTo(n: CARDINAL): CARDINAL;
VAR i, s: CARDINAL;
BEGIN
    s := 0;
    FOR i := 1 TO n DO
        s := s + i;
    END;
    RETURN s;
END SumTo;

BEGIN
    fib_result := Fibonacci(10);
    gcd_result := GCD(252, 105);
    sum_result := SumTo(100);
END TestColdfire.
