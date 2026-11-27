/* HOST TESTS ONLY: no ARM intrinsics or hardware access are emulated here.
 * Never add this directory to Keil/target firmware include paths.
 */
#if defined(__arm__) || defined(__thumb__)
#error Host test CMSIS shim must not be used for target firmware
#endif
#define __ASM __asm
#define __INLINE inline
#define __STATIC_INLINE static inline
#define __PACKED __attribute__((packed))
#define __ALIGNED(x) __attribute__((aligned(x)))
#define __NOP() ((void)0)
#define __DSB() ((void)0)
#define __ISB() ((void)0)
