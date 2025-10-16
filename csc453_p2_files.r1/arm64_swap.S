#ifdef __APPLE__
#define save_ctx _save_ctx
#define load_ctx _load_ctx
#define swap_ctx _swap_ctx
#define swap_rfiles _swap_rfiles
#else
#endif

#define SS64_X0     0
#define SS64_X2     16
#define SS64_X4     32
#define SS64_X6     48
#define SS64_X8     64
#define SS64_X10    80
#define SS64_X12    96
#define SS64_X14    112
#define SS64_X16    128
#define SS64_X18    144
#define SS64_X20    160
#define SS64_X22    176
#define SS64_X24    192
#define SS64_X26    208
#define SS64_X28    224
#define SS64_LR     240
#define SS64_SP     248
#define NS64_D8     256
#define NS64_D10    272
#define NS64_D12    288
#define NS64_D14    304

.text

.globl save_ctx
.globl load_ctx
.globl swap_ctx

#ifndef __APPLE__
.type swap_rfiles, @function
#endif

.p2align 2
swap_rfiles:
    // void swap_rfiles(rfile *old, rfile *new)
    //  - Old rfile ptr stored in x0
    //  - New rfile ptr stored in x1
    //  - Uses x9

    // --- SAVE --- //
    // Skip saving the registers if `old` is NULL (i.e. 0)
    cmp     x0, xzr
    b.eq    skip_save

    // Save general register x16, x17, and x19 - x30
    stp     x0, x1,   [x0, SS64_X0]
    stp     x2, x3,   [x0, SS64_X2]
    stp     x4, x5,   [x0, SS64_X4]
    stp     x6, x7,   [x0, SS64_X6]
    stp     x8, x9,   [x0, SS64_X8]
    stp     x10, x11, [x0, SS64_X10]
    stp     x12, x13, [x0, SS64_X12]
    stp     x14, x15, [x0, SS64_X14]
    stp     x16, x17, [x0, SS64_X16]
    stp     x18, x19, [x0, SS64_X18]
    stp     x20, x21, [x0, SS64_X20]
    stp     x22, x23, [x0, SS64_X22]
    stp     x24, x25, [x0, SS64_X24]
    stp     x26, x27, [x0, SS64_X26]
    stp     x28, x29, [x0, SS64_X28]
    str     lr,  [x0, SS64_LR]

    // Make a copy of the stack pointer and save it
    mov     x2, sp
    str     x2, [x0, SS64_SP]

    // Save floating point registers d8 - d15 (s8 - s15)
    stp     d8, d9, [x0, NS64_D8]
    stp     d10, d11, [x0, NS64_D10]
    stp     d12, d13, [x0, NS64_D12]
    stp     d14, d15, [x0, NS64_D14]
    // ------------ //

skip_save:

    // --- LOAD --- //
    // Skip loading the registers if `new` is NULL (i.e. 0)
    cmp     x1, xzr
    b.eq    skip_load

    // Load x16, x17, and x19 - x30
    ldp     x2, x3,   [x1, SS64_X2]
    ldp     x4, x5,   [x1, SS64_X4]
    ldp     x6, x7,   [x1, SS64_X6]
    ldp     x8, x9,   [x1, SS64_X8]
    ldp     x10, x11, [x1, SS64_X10]
    ldp     x12, x13, [x1, SS64_X12]
    ldp     x14, x15, [x1, SS64_X14]
    ldp     x16, x17, [x1, SS64_X16]
    ldp     x18, x19, [x1, SS64_X18]
    ldp     x20, x21, [x1, SS64_X20]
    ldp     x22, x23, [x1, SS64_X22]
    ldp     x24, x25, [x1, SS64_X24]
    ldp     x26, x27, [x1, SS64_X26]
    ldp     x28, x29, [x1, SS64_X28]
    ldr     lr, [x1, SS64_LR]

    // Make a copy of the stack pointer and load it
    ldr     x0, [x1, SS64_SP]
    mov     sp, x0

    // Load floating point registers d8 - d15 (s8 - s15)
    ldp     d8, d9, [x1, NS64_D8]
    ldp     d10, d11, [x1, NS64_D10]
    ldp     d12, d13, [x1, NS64_D12]
    ldp     d14, d15, [x1, NS64_D14]
    // ------------ //

    ldp     x0, x1,   [x1, SS64_X0]

skip_load:

    ret

#ifndef __APPLE__
.type save_ctx, @function
#endif

.p2align 2
save_ctx:
    stp    fp, lr, [sp, -16]!
    mov    x1, xzr
    bl     swap_rfiles
    ldp    fp, lr, [sp], 16
    ret

#ifndef __APPLE__
.type load_ctx, @function
#endif

.p2align 2
load_ctx:
    stp    fp, lr, [sp, -16]!
    mov    x1, x0
    mov    x0, xzr
    bl     swap_rfiles
    ldp    fp, lr, [sp], 16
    ret

#ifndef __APPLE__
.type swap_ctx, @function
#endif

.p2align 2
swap_ctx:
    stp    fp, lr, [sp, -16]!
    bl     swap_rfiles
    ldp    fp, lr, [sp], 16
    ret
