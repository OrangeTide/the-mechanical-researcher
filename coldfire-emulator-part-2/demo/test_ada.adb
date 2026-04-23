-- test_ada.adb : bare-metal ColdFire V4e test — Ada (via GNAT)
-- Cross-compile: m68k-linux-gnu-gcc-10 -mcpu=5475 -O2 -gnatp -c
-- Link with C shim: m68k-linux-gnu-gcc-10 ... shim_ada.c test_ada.o
-- Copyright (c) 2026 Jon Mayo — MIT-0 OR Public Domain
-- Written with AI assistance (Claude, Anthropic)

-- Compiled with -gnatp to suppress runtime checks (overflow, range).
-- Without this, GNAT emits calls to __gnat_rcheck_* which require
-- the GNAT runtime library.

procedure Test_Ada is
   type Unsigned_32 is mod 2 ** 32;

   Result_Fib : Unsigned_32 := 0;
   pragma Export (C, Result_Fib, "result_fib");

   Result_Gcd : Unsigned_32 := 0;
   pragma Export (C, Result_Gcd, "result_gcd");

   Result_Sum : Unsigned_32 := 0;
   pragma Export (C, Result_Sum, "result_sum");

   Result_Bits : Unsigned_32 := 0;
   pragma Export (C, Result_Bits, "result_bits");

   Result_Sqrt_I : Unsigned_32 := 0;
   pragma Export (C, Result_Sqrt_I, "result_sqrt_i");

   function Fibonacci (N : Unsigned_32) return Unsigned_32 is
   begin
      if N <= 1 then
         return N;
      end if;
      return Fibonacci (N - 1) + Fibonacci (N - 2);
   end Fibonacci;

   function Gcd_Func (A_In : Unsigned_32; B_In : Unsigned_32)
      return Unsigned_32
   is
      A : Unsigned_32 := A_In;
      B : Unsigned_32 := B_In;
      T : Unsigned_32;
   begin
      while B /= 0 loop
         T := A mod B;
         A := B;
         B := T;
      end loop;
      return A;
   end Gcd_Func;

   function Sum_To (N : Unsigned_32) return Unsigned_32 is
      S : Unsigned_32 := 0;
   begin
      for I in 1 .. N loop
         S := S + I;
      end loop;
      return S;
   end Sum_To;

   function Bit_Test (X : Unsigned_32) return Unsigned_32 is
      A : Unsigned_32 := X * 16;
      B : Unsigned_32 := X / 4;
      C : Unsigned_32 := A xor B;
      D : Unsigned_32 := C and 16#FF00#;
      E : Unsigned_32 := D or 16#0055#;
   begin
      return E;
   end Bit_Test;

   function Sqrt_Approx return Unsigned_32 is
      X     : Long_Float := 2.0;
      Guess : Long_Float := X / 2.0;
   begin
      for I in 1 .. 20 loop
         Guess := (Guess + X / Guess) / 2.0;
      end loop;
      return Unsigned_32 (Guess * 1000.0);
   end Sqrt_Approx;

begin
   Result_Fib    := Fibonacci (10);
   Result_Gcd    := Gcd_Func (252, 105);
   Result_Sum    := Sum_To (100);
   Result_Bits   := Bit_Test (16#AB#);
   Result_Sqrt_I := Sqrt_Approx;
end Test_Ada;
