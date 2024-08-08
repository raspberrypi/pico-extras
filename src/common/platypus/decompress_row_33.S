#include "hardware/regs/addressmap.h"
.syntax unified
.cpu cortex-m33
.thumb

#ifndef VIDEO_DBI
#error only VIDEO_DBI supported
#endif
#define FIRST_TEST_SHIFT 6
#define SECOND_TEST_SHIFT 11

// For PLATYPUS_GATED_EOL_CHECK, we remove most checks for running off the end of the output buffer when
// the next input is not word aligned.
// This sounds bad, however, we actually inject a new check in the slow/least-compressed 7 byte path
// before reading the data.
// The upshot is that we can avoid the checks as mentioned above, as long as we append 0xa0 bytes
// up to a word boundary beyond the data, to force a read off the end of the compressed data into
// the 7-byte path (note 0xa0 covers both DBI and non-DBI mode for historical reasons)
#if PLATYPUS_GATED_EOL_CHECK
#define GATE_ALIGN_1_CHECK (FIRST_TEST_SHIFT<8)
#define GATE_ALIGN_2_CHECK (FIRST_TEST_SHIFT<16)
#define GATE_ALIGN_3_CHECK (FIRST_TEST_SHIFT<24)
#endif

#define r_output r0
#define r_data r1
#define r_input r2
#define r_top r3
#define r_bottom r4
#define r_tmp3 r5
#define r_tmp2 r6
#define r_tmp1 r7
#define r_output_end r8
#define r_5_tables r9
#define r_222_table r10
#define r_mask r11
// for now
#define r_tmp_hi1  r_tmp3
#define r_row_delta r14

.macro align_fall_thru
.p2align 2
.endm

.macro debuggo val
#if 0
push {r0-r3, ip, lr}
mov r0, r_input
mov r1, \val
mov r2, r_top
mov lr, ip
bl debuggo_print
pop {r0-r3, ip, lr}
#endif
.endm

.macro lo_to_hi_lo dest, src
// src      -- -- AB CD
// dest     AB CD AB CD
    pkhbt \dest, \src, \src, lsl #16
.endm

.macro do_222
    ubfx r_tmp_hi1, r_tmp1, #12, #6
    add  r_tmp_hi1, r_222_table, r_tmp_hi1, lsl #3
    ldrd r_tmp1, r_tmp2, [r_tmp_hi1]
    add r_bottom, r_top, r_tmp2
    add r_top, r_tmp1
.endm

.macro do_555
    ubfx  r_tmp_hi1, r_tmp1, #11, #5 // red idx
    add   r_tmp2, r_5_tables, r_tmp_hi1, LSL #3
    ldrd  r_tmp2, r_tmp_hi1, [r_tmp2, #0] // red values
    add   r_bottom, r_top, r_tmp_hi1
    add   r_top, r_tmp2

    ubfx  r_tmp_hi1, r_tmp1, #16, #5 // green idx
    add   r_tmp2, r_5_tables, r_tmp_hi1, LSL #3
    ldrd  r_tmp2, r_tmp_hi1, [r_tmp2, #256] // green values
    add   r_top, r_tmp2
    add   r_bottom, r_tmp_hi1

    ubfx  r_tmp_hi1, r_tmp1, #21, #5 // blue idx
    add   r_tmp2, r_5_tables, r_tmp_hi1, LSL #3
    ldrd  r_tmp2, r_tmp_hi1, [r_tmp2, #512] // blue values
    add   r_top, r_tmp2
    add   r_bottom, r_tmp_hi1
.endm

// inputs, are top and data
.macro shuffle_7_bytes_to_8 top_from, bottom_from tmp_a tmp_b
//#define SHRINKO 1 // todo still need to figure out why this makes things worse - ok actually don't think it makes things much worse, the problem seems LSL/LSR shifts here take it back to 2 cycles anyway

#if 0
    ldr \tmp_b, =#0xff210821
    lsls \tmp_a, \tmp_b, #16
    ands \tmp_b, \bottom_from
    ands \tmp_a, \top_from
#else
#if SHRINKO
    ands \tmp_b, r_mask, \bottom_from
    ands \tmp_a, \top_from, r_mask, LSL #16
#else
    lsls \tmp_a, r_mask, #16
    ands \tmp_b, r_mask, \bottom_from
    ands \tmp_a, \top_from
#endif
#endif

    eors r_bottom, \bottom_from, \tmp_b
    eors r_top, \top_from, \tmp_a

    // todo can we shave 1 more cycles to make cycles in the DBI code? a challenge to anyone who reads this!
#if SHRINKO
    .p2align 2
    orrs \tmp_b, \tmp_b, \tmp_a, LSR #13
#else
    lsrs \tmp_a, #13
    orrs \tmp_b, \tmp_a
#endif

#if SHRINKO
    eors \tmp_b, \tmp_b, \tmp_b, LSR #10
#else
    lsrs \tmp_a, \tmp_b, #10
    eors \tmp_b, \tmp_a
#endif

    lsrs \tmp_a, \tmp_b, #12
    adcs \tmp_b, \tmp_b

#if SHRINKO
    orrs r_bottom, r_bottom, \tmp_b, LSL #24
#else
    lsls \tmp_b, #24
    orrs r_bottom, \tmp_b
#endif
.endm

.macro write_output
    str  r_bottom, [r_output, r_row_delta]
    stmia r_output!, {r_top}
.endm


#if PLATYPUS_TABLES_MAIN_RAM
.section .data
.global shared_5_table
shared_5_table:
#else
.macro decompressor name data_section_prefix code_section_prefix
.section \data_section_prefix\().\name\().data
.global \name\()_5_table
\name\()_5_table:
#endif

#define Q 0
.word    0x00000000 << Q, 0x00000000 << Q, 0x00000000 << Q, 0x00000001 << Q, 0x00010000 << Q, 0x00010001 << Q, 0x00010000 << Q, 0x00000001 << Q
.word    0x00010001 << Q, 0x00000001 << Q, 0x00010000 << Q, 0x00000000 << Q, 0x00000000 << Q, 0x00010001 << Q, 0x00010000 << Q, 0x00010002 << Q
.word    0x00000000 << Q, 0x00010002 << Q, 0x00020000 << Q, 0x00010001 << Q, 0x00010000 << Q, 0x00020002 << Q, 0x00010000 << Q, 0x00010000 << Q
.word    0x00000001 << Q, 0x00000001 << Q, 0x00010001 << Q, 0x00000002 << Q, 0x00010001 << Q, 0x00000000 << Q, 0x00020001 << Q, 0x00000001 << Q
.word    0x00020000 << Q, 0x00020001 << Q, 0x00000001 << Q, 0x00000002 << Q, 0x00020002 << Q, 0x00000001 << Q, 0x00020001 << Q, 0x00000000 << Q
.word    0x00000000 << Q, 0x00000002 << Q, 0x00000001 << Q, 0x00010002 << Q, 0x00010000 << Q, 0x00020001 << Q, 0x00020000 << Q, 0x00010000 << Q
.word    0x00020000 << Q, 0x00020002 << Q, 0x00000000 << Q, 0x00020002 << Q, 0x00010002 << Q, 0x00000002 << Q, 0x00010000 << Q, 0x00020003 << Q
.word    0x00000000 << Q, 0x00020003 << Q, 0x00010000 << Q, 0x00030003 << Q, 0x00010000 << Q, 0x00000002 << Q, 0x00000001 << Q, 0x00010001 << Q

#undef Q
#define Q 6
.word    0x00000000 << Q, 0x00000000 << Q, 0x00000000 << Q, 0x00000001 << Q, 0x00010000 << Q, 0x00010001 << Q, 0x00010000 << Q, 0x00000001 << Q
.word    0x00010001 << Q, 0x00000001 << Q, 0x00010000 << Q, 0x00000000 << Q, 0x00000000 << Q, 0x00010001 << Q, 0x00010000 << Q, 0x00010002 << Q
.word    0x00000000 << Q, 0x00010002 << Q, 0x00020000 << Q, 0x00010001 << Q, 0x00010000 << Q, 0x00020002 << Q, 0x00010000 << Q, 0x00010000 << Q
.word    0x00000001 << Q, 0x00000001 << Q, 0x00010001 << Q, 0x00000002 << Q, 0x00010001 << Q, 0x00000000 << Q, 0x00020001 << Q, 0x00000001 << Q
.word    0x00020000 << Q, 0x00020001 << Q, 0x00000001 << Q, 0x00000002 << Q, 0x00020002 << Q, 0x00000001 << Q, 0x00020001 << Q, 0x00000000 << Q
.word    0x00000000 << Q, 0x00000002 << Q, 0x00000001 << Q, 0x00010002 << Q, 0x00010000 << Q, 0x00020001 << Q, 0x00020000 << Q, 0x00010000 << Q
.word    0x00020000 << Q, 0x00020002 << Q, 0x00000000 << Q, 0x00020002 << Q, 0x00010002 << Q, 0x00000002 << Q, 0x00010000 << Q, 0x00020003 << Q
.word    0x00000000 << Q, 0x00020003 << Q, 0x00010000 << Q, 0x00030003 << Q, 0x00010000 << Q, 0x00000002 << Q, 0x00000001 << Q, 0x00010001 << Q

#undef Q
#define Q 11
.word    0x00000000 << Q, 0x00000000 << Q, 0x00000000 << Q, 0x00000001 << Q, 0x00010000 << Q, 0x00010001 << Q, 0x00010000 << Q, 0x00000001 << Q
.word    0x00010001 << Q, 0x00000001 << Q, 0x00010000 << Q, 0x00000000 << Q, 0x00000000 << Q, 0x00010001 << Q, 0x00010000 << Q, 0x00010002 << Q
.word    0x00000000 << Q, 0x00010002 << Q, 0x00020000 << Q, 0x00010001 << Q, 0x00010000 << Q, 0x00020002 << Q, 0x00010000 << Q, 0x00010000 << Q
.word    0x00000001 << Q, 0x00000001 << Q, 0x00010001 << Q, 0x00000002 << Q, 0x00010001 << Q, 0x00000000 << Q, 0x00020001 << Q, 0x00000001 << Q
.word    0x00020000 << Q, 0x00020001 << Q, 0x00000001 << Q, 0x00000002 << Q, 0x00020002 << Q, 0x00000001 << Q, 0x00020001 << Q, 0x00000000 << Q
.word    0x00000000 << Q, 0x00000002 << Q, 0x00000001 << Q, 0x00010002 << Q, 0x00010000 << Q, 0x00020001 << Q, 0x00020000 << Q, 0x00010000 << Q
.word    0x00020000 << Q, 0x00020002 << Q, 0x00000000 << Q, 0x00020002 << Q, 0x00010002 << Q, 0x00000002 << Q, 0x00010000 << Q, 0x00020003 << Q
.word    0x00000000 << Q, 0x00020003 << Q, 0x00010000 << Q, 0x00030003 << Q, 0x00010000 << Q, 0x00000002 << Q, 0x00000001 << Q, 0x00010001 << Q

#if PLATYPUS_TABLES_MAIN_RAM
.global shared_222_table
shared_222_table:
#else
.global \name\()_222_table
\name\()_222_table:
#endif
.word    0x00000000, 0x00000000, 0x00000000, 0x00000001, 0x00010000, 0x00010001, 0x00010000, 0x00000001
.word    0x00000000, 0x00000040, 0x00000000, 0x00000041, 0x00010000, 0x00010041, 0x00010000, 0x00000041
.word    0x00400000, 0x00400040, 0x00400000, 0x00400041, 0x00410000, 0x00410041, 0x00410000, 0x00400041
.word    0x00400000, 0x00000040, 0x00400000, 0x00000041, 0x00410000, 0x00010041, 0x00410000, 0x00000041
.word    0x00000000, 0x00000800, 0x00000000, 0x00000801, 0x00010000, 0x00010801, 0x00010000, 0x00000801
.word    0x00000000, 0x00000840, 0x00000000, 0x00000841, 0x00010000, 0x00010841, 0x00010000, 0x00000841
.word    0x00400000, 0x00400840, 0x00400000, 0x00400841, 0x00410000, 0x00410841, 0x00410000, 0x00400841
.word    0x00400000, 0x00000840, 0x00400000, 0x00000841, 0x00410000, 0x00010841, 0x00410000, 0x00000841
.word    0x08000000, 0x08000800, 0x08000000, 0x08000801, 0x08010000, 0x08010801, 0x08010000, 0x08000801
.word    0x08000000, 0x08000840, 0x08000000, 0x08000841, 0x08010000, 0x08010841, 0x08010000, 0x08000841
.word    0x08400000, 0x08400840, 0x08400000, 0x08400841, 0x08410000, 0x08410841, 0x08410000, 0x08400841
.word    0x08400000, 0x08000840, 0x08400000, 0x08000841, 0x08410000, 0x08010841, 0x08410000, 0x08000841
.word    0x08000000, 0x00000800, 0x08000000, 0x00000801, 0x08010000, 0x00010801, 0x08010000, 0x00000801
.word    0x08000000, 0x00000840, 0x08000000, 0x00000841, 0x08010000, 0x00010841, 0x08010000, 0x00000841
.word    0x08400000, 0x00400840, 0x08400000, 0x00400841, 0x08410000, 0x00410841, 0x08410000, 0x00400841
.word    0x08400000, 0x00000840, 0x08400000, 0x00000841, 0x08410000, 0x00010841, 0x08410000, 0x00000841

#if PLATYPUS_TABLES_MAIN_RAM
.macro decompressor name data_section_prefix code_section_prefix
#endif
.section \code_section_prefix\().\name\().code, "ax"

.global \name
.type \name,%function
.thumb_func
//const uint8_t* \name(uint32_t *d0, uint32_t *d1, const uint8_t *s, uint32_t w);
\name:
    push {r4, r5, r6, r7, r8, r9, r10, r11, lr}
#if PLATYPUS_TABLES_MAIN_RAM
    ldr r_5_tables, =shared_5_table
    ldr r_222_table, =shared_222_table
#else
    ldr r_5_tables, =\name\()_5_table
    ldr r_222_table, =\name\()_222_table
#endif
    ldr r_mask, =#0xff210821
    sub r_row_delta, r1, r_output
    add r_output_end, r0, r3, LSL #1

\name\()_rem_0:
    // r_data: X X X X
    ldmia r_input!, {r_data}
align_fall_thru
\name\()_rem_4:
    // r_data: D C B A
    lsrs  r_tmp1, r_data, #FIRST_TEST_SHIFT
    bcs  2f
    debuggo 4
    lo_to_hi_lo r_top, r_data
    lsrs  r_tmp2, r_tmp1, #SECOND_TEST_SHIFT
    bcc  1f
    do_555
    write_output
    cmp   r_output, r_output_end
    blt  \name\()_rem_0
    b    \name\()_done
.p2align 2
1:
    do_222
    write_output
#if GATE_ALIGN_1_CHECK
    b \name\()_rem_1
#else
    cmp   r_output, r_output_end
    blt  \name\()_rem_1
    b    \name\()_done
#endif

.p2align 2
2:
    // r_data: D C B A
    debuggo 4
    mov   r_top, r_data
    ldmia r_input!, {r_data}
    // r_top:  D C B A
    // r_data: H G F E
    shuffle_7_bytes_to_8 r_top, r_data, r_tmp1, r_tmp2
    write_output
#if !GATE_ALIGN_1_CHECK
    cmp   r_output, r_output_end
    bge   \name\()_done
#endif

    // fall thru
align_fall_thru
\name\()_rem_1:
    // r_data : A X X X
    debuggo 1
    lsrs r_tmp1, r_data, #24
    // r_tmp1 : 0 0 0 A
    ldmia r_input!, {r_data}
    // r_data : E D C B
    lsls r_tmp2, r_data, #8
    orrs r_tmp2, r_tmp1
    // r_tmp2 : D C B A
    lsrs  r_tmp1, r_tmp2, #FIRST_TEST_SHIFT
    bcs  2f
    lo_to_hi_lo r_top, r_tmp2
    lsrs  r_tmp2, r_tmp1, #SECOND_TEST_SHIFT
    bcc  1f
    do_555
    write_output
#if GATE_ALIGN_1_CHECK
    b \name\()_rem_1
#else
    cmp   r_output, r_output_end
    blt  \name\()_rem_1
    b    \name\()_done
#endif
.p2align 2
1:
    do_222
    write_output
#if GATE_ALIGN_2_CHECK
    b \name\()_rem_2
#else
    cmp   r_output, r_output_end
    blt \name\()_rem_2
    b \name\()_done
#endif
.p2align 2
2:
#if GATE_ALIGN_1_CHECK
    cmp   r_output, r_output_end
    bge \name\()_done
#endif

    // r_data : E D C B
    // r_tmp2 : D C B A
    lsrs r_bottom, r_data, #24
    // r_bottom : 0 0 0 E
    ldmia r_input!, {r_data}
    // r_data : I H G F
    lsls r_tmp1, r_data, #8
    orrs r_bottom, r_tmp1
    // r_bottom : H G F E

    shuffle_7_bytes_to_8 r_tmp2, r_bottom, r_tmp1, r_tmp3
    write_output

#if !GATE_ALIGN_2_CHECK
    cmp   r_output, r_output_end
    bge \name\()_done
#endif

    // fall thru
align_fall_thru
\name\()_rem_2:
    // r_data : B A 0 0
    debuggo 2
    lsrs r_tmp1, r_data, #16
    ldmia r_input!, {r_data}
    // r_data : F E D C
    lsls r_tmp2, r_data, #16
    orrs r_tmp2, r_tmp1
    // r_tmp2 : D C B A

    lsrs  r_tmp1, r_tmp2, #FIRST_TEST_SHIFT
    bcs  2f
    lo_to_hi_lo r_top, r_tmp2
    lsrs  r_tmp2, r_tmp1, #SECOND_TEST_SHIFT
    bcc  1f
    do_555
    write_output
#if GATE_ALIGN_2_CHECK
    b \name\()_rem_2
#else
    cmp   r_output, r_output_end
    blt \name\()_rem_2
#endif
\name\()_done:
\name\()_done2:
    pop {r4, r5, r6, r7, r8, r9, r10, r11, pc}
.p2align 2
1:
    do_222
    write_output
#if GATE_ALIGN_3_CHECK
    b \name\()_rem_3
#else
    cmp   r_output, r_output_end
    blt  \name\()_rem_3
    b    \name\()_done
#endif
.p2align 2
2:
#if GATE_ALIGN_2_CHECK
    cmp   r_output, r_output_end
    bge \name\()_done
#endif

    // r_data : F E D C
    // r_tmp2 : D C B A

    lsrs r_bottom, r_data, #16
    // r_bottom: 0 0 F E
    ldmia r_input!, {r_data}
    // r_data : J I H G
    lsls r_tmp1, r_data, #16
    orrs r_bottom, r_tmp1
    // r_bottom : H G F E

    shuffle_7_bytes_to_8 r_tmp2, r_bottom, r_tmp1, r_tmp3
    write_output
#if !GATE_ALIGN_3_CHECK
    cmp   r_output, r_output_end
    bge \name\()_done
#endif

// fall thru
align_fall_thru
\name\()_rem_3: // r_remaining_bits has 3 bytes remaining (in the MSB)
    // r_data : C B A 0
    debuggo 3
    lsrs r_tmp1, r_data, #8
    ldmia r_input!, {r_data}
    // r_data : G F E D
    lsls r_tmp2, r_data, #24
    orrs r_tmp2, r_tmp1
    // r_tmp2 : D C B A

    lsrs  r_tmp1, r_tmp2, #FIRST_TEST_SHIFT
    bcs  2f

    lo_to_hi_lo r_top, r_tmp2
    lsrs  r_tmp2, r_tmp1, #SECOND_TEST_SHIFT
    bcc  1f
    do_555
    write_output
#if GATE_ALIGN_3_CHECK
    b \name\()_rem_3
#else
    cmp   r_output, r_output_end
    blt  \name\()_rem_3
    b    \name\()_done
#endif
.p2align 2
1:
    do_222
    write_output
    cmp   r_output, r_output_end
    blt  \name\()_rem_4
    b    \name\()_done2
.p2align 2
2:
#if GATE_ALIGN_3_CHECK
    cmp   r_output, r_output_end
    bge \name\()_done
#endif

    // r_data : G F E D
    // r_tmp2 : D C B A

    lsrs r_bottom, r_data, #8
    // r_bottom : 0 G F E
    ldmia r_input!, {r_data}
    // r_data : K J I H
    lsls r_tmp1, r_data, #24
    orrs r_bottom, r_tmp1
    // r_bottom : H G F E

    shuffle_7_bytes_to_8 r_tmp2, r_bottom, r_tmp1, r_tmp3
    write_output

    cmp   r_output, r_output_end
    blt  \name\()_rem_4
    b    \name\()_done2
.endm

// put one decompressor in each scratch bank for use by each core
decompressor platypus_decompress_row_asm_a .scratch_x, .scratch_x
decompressor platypus_decompress_row_asm_b .scratch_y, .scratch_y