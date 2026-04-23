! test_fortran.f90 : bare-metal ColdFire V4e test — Fortran
! Cross-compile: m68k-linux-gnu-gfortran -mcpu=5475 -O2 -c
! Link with C shim: m68k-linux-gnu-gcc ... shim_fortran.c test_fortran.o
! Copyright (c) 2026 Jon Mayo — MIT-0 OR Public Domain
! Written with AI assistance (Claude, Anthropic)

! Subroutine with C binding — callable from a C _start shim.
! Uses ISO_C_BINDING for portable type mapping.
subroutine compute(res_fib, res_gcd, res_sum, res_bits, res_sqrt) &
    bind(C, name="compute")
    use, intrinsic :: iso_c_binding
    implicit none
    integer(c_int32_t), intent(out) :: res_fib, res_gcd, res_sum
    integer(c_int32_t), intent(out) :: res_bits, res_sqrt
    real(c_double) :: x, guess
    integer :: i

    res_fib  = fibonacci(10)
    res_gcd  = gcd_func(252, 105)
    res_sum  = sum_to(100)
    res_bits = bit_test(171)

    ! Newton's method: sqrt(2) * 1000
    x = 2.0d0
    guess = x / 2.0d0
    do i = 1, 20
        guess = (guess + x / guess) / 2.0d0
    end do
    res_sqrt = int(guess * 1000.0d0, c_int32_t)

contains
    recursive integer function fibonacci(n) result(res)
        integer, intent(in) :: n
        if (n <= 1) then
            res = n
        else
            res = fibonacci(n - 1) + fibonacci(n - 2)
        end if
    end function

    integer function gcd_func(a_in, b_in)
        integer, intent(in) :: a_in, b_in
        integer :: a, b, t
        a = a_in
        b = b_in
        do while (b /= 0)
            t = mod(a, b)
            a = b
            b = t
        end do
        gcd_func = a
    end function

    integer function sum_to(n)
        integer, intent(in) :: n
        integer :: i
        sum_to = 0
        do i = 1, n
            sum_to = sum_to + i
        end do
    end function

    integer function bit_test(x)
        integer, intent(in) :: x
        integer :: a, b, c, d, e
        a = ishft(x, 4)
        b = ishft(x, -2)
        c = ieor(a, b)
        d = iand(c, int(Z'FF00'))
        e = ior(d, int(Z'0055'))
        bit_test = e
    end function
end subroutine
