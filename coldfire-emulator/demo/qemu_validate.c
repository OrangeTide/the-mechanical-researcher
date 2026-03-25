/* qemu_validate.c : QEMU validation for ColdFire V4e emulator results */
/* Runs the same test functions under qemu-m68k for comparison.
 * Build:  m68k-linux-gnu-gcc -mcpu=5475 -O2 -nostdlib -static -o qemu_validate qemu_validate.c
 * Run:    qemu-m68k -cpu cfv4e ./qemu_validate */
/* Copyright (c) 2026 Jon Mayo — MIT-0 OR Public Domain */
/* Written with AI assistance (Claude, Anthropic) */

/* No libc — raw Linux syscalls to avoid glibc alignment issues on ColdFire */

typedef unsigned int uint32_t;
typedef int int32_t;

/****************************************************************
 * Syscall wrappers (m68k Linux ABI: trap #0, syscall# in d0)
 ****************************************************************/

static int
sys_write(int fd, const char *buf, int len)
{
    register int d0 __asm__("d0") = 4;
    register int d1 __asm__("d1") = fd;
    register const char *d2 __asm__("d2") = buf;
    register int d3 __asm__("d3") = len;
    __asm__ volatile(
        "trap #0"
        : "+d"(d0)
        : "d"(d1), "d"(d2), "d"(d3)
        : "memory"
    );
    return d0;
}

static void __attribute__((noreturn))
sys_exit(int code)
{
    register int d0 __asm__("d0") = 1;
    register int d1 __asm__("d1") = code;
    __asm__ volatile(
        "trap #0"
        : : "d"(d0), "d"(d1)
    );
    __builtin_unreachable();
}

/****************************************************************
 * Minimal output helpers (no libc)
 ****************************************************************/

static int
my_strlen(const char *s)
{
    int n = 0;
    while (*s++)
        n++;
    return n;
}

static void
puts_fd(int fd, const char *s)
{
    sys_write(fd, s, my_strlen(s));
}

static void
put_uint(int fd, uint32_t v)
{
    char buf[12];
    int i = 0;

    if (v == 0) {
        sys_write(fd, "0", 1);
        return;
    }
    while (v > 0) {
        buf[i++] = '0' + (v % 10);
        v /= 10;
    }
    /* reverse */
    {
        int a = 0, b = i - 1;
        while (a < b) {
            char t = buf[a];
            buf[a] = buf[b];
            buf[b] = t;
            a++;
            b--;
        }
    }
    sys_write(fd, buf, i);
}

static void
put_hex(int fd, uint32_t v)
{
    static const char hex[] = "0123456789abcdef";
    char buf[10];
    int i;

    buf[0] = '0';
    buf[1] = 'x';
    for (i = 0; i < 8; i++)
        buf[2 + i] = hex[(v >> (28 - i * 4)) & 0xF];
    sys_write(fd, buf, 10);
}

/****************************************************************
 * Same test functions as test_program.c
 ****************************************************************/

static uint32_t
fibonacci(uint32_t n)
{
    if (n <= 1)
        return n;
    return fibonacci(n - 1) + fibonacci(n - 2);
}

static uint32_t
gcd(uint32_t a, uint32_t b)
{
    while (b != 0) {
        uint32_t t = a % b;
        a = b;
        b = t;
    }
    return a;
}

static uint32_t
sum_to(uint32_t n)
{
    uint32_t s = 0;
    uint32_t i;

    for (i = 1; i <= n; i++)
        s += i;
    return s;
}

static uint32_t
bit_test(uint32_t x)
{
    uint32_t a = x << 4;
    uint32_t b = x >> 2;
    uint32_t c = a ^ b;
    uint32_t d = c & 0xFF00;
    uint32_t e = d | 0x0055;
    return e;
}

static uint32_t
sqrt_approx(double x)
{
    double guess = x / 2.0;
    int i;

    for (i = 0; i < 20; i++)
        guess = (guess + x / guess) / 2.0;

    return (uint32_t)(guess * 1000.0);
}

/****************************************************************
 * Check and report
 ****************************************************************/

static int passed, failed;

static void
check(const char *name, uint32_t got, uint32_t expected)
{
    puts_fd(1, "  ");
    puts_fd(1, name);
    puts_fd(1, "  got ");
    put_uint(1, got);
    puts_fd(1, "  expected ");
    put_uint(1, expected);
    if (got == expected) {
        puts_fd(1, "  PASS\n");
        passed++;
    } else {
        puts_fd(1, "  FAIL\n");
        failed++;
    }
}

/****************************************************************
 * Entry point (no libc, no crt)
 ****************************************************************/

void _start(void) __attribute__((section(".text.startup")));

void
_start(void)
{
    uint32_t r_fib, r_gcd, r_sum, r_bits, r_sqrt;

    passed = 0;
    failed = 0;

    r_fib  = fibonacci(10);
    r_gcd  = gcd(252, 105);
    r_sum  = sum_to(100);
    r_bits = bit_test(0xAB);
    r_sqrt = sqrt_approx(2.0);

    puts_fd(1, "QEMU ColdFire V4e validation\n");
    puts_fd(1, "----------------------------\n");

    check("fibonacci(10)", r_fib, 55);
    check("gcd(252, 105)", r_gcd, 21);
    check("sum_to(100)",   r_sum, 5050);
    check("bit_test(0xAB)",r_bits, 0x0A55);
    check("sqrt(2)*1000",  r_sqrt, 1414);

    puts_fd(1, "\n");
    put_uint(1, passed);
    puts_fd(1, "/");
    put_uint(1, passed + failed);
    puts_fd(1, " passed\n");

    sys_exit(failed > 0 ? 1 : 0);
}
