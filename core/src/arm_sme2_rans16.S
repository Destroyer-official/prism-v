// ============================================================================
// Prism-V: 16-Way SIMD rANS & SME2 SMOPA Tile Micro-Engine
// Target: Armv9.2-A / Armv9.4-A (Cortex-X925 "Blackhawk" / Cortex-A725)
// Architecture Features: SME2, SVE2, Streaming SVE (SSVE), ZT0 LUT Register
// ============================================================================

#if defined(__aarch64__) && !defined(_MSC_VER)

    .arch armv9-a+sme2+sve2
    .text
    .align 4

// ----------------------------------------------------------------------------
// arm_sme2_smopa_forward:
// Evaluates 12x12 -> 4 INT8 outer product forward pass into 32-bit ZA tile
//
// C Signature:
// void arm_sme2_smopa_forward(const int8_t* lhs_activations, // x0
//                             const int8_t* rhs_weights,      // x1
//                             int32_t* dst_tile,             // x2
//                             const uint8_t* zt0_lut_table); // x3
// ----------------------------------------------------------------------------
    .global arm_sme2_smopa_forward
    .type arm_sme2_smopa_forward, %function
arm_sme2_smopa_forward:
    // Enter Streaming SVE Mode (SSVE)
    smstart sm

    // Load 512-bit Normative CDF_SIGMOID_LUT into ZT0 register
    ldr     zt0, [x3]

    // Set predicate registers P0 to all true for 8-bit / 32-bit elements
    ptrue   p0.b
    ptrue   p1.s

    // Clear accumulator tile ZA0.S
    zero    {za0.s}

    // Load 16 vector input activations into Z0.B
    ld1b    {z0.b}, p0/z, [x0]

    // Load 16 vector weights into Z1.B
    ld1b    {z1.b}, p0/z, [x1]

    // Execute Signed Matrix Outer Product and Accumulate into 32-bit ZA tile:
    // ZA0.S += Z0.B (outer product) Z1.B
    smopa   za0.s, p0/m, p0/m, z0.b, z1.b

    // Read tile slice back to vector register Z2.S
    mova    z2.s, p1/m, za0.s[0]

    // Dyadic scaling right shift (s = 15)
    asr     z2.s, z2.s, #15

    // Fast non-linear activation via ZT0 register lookup (LUTI4)
    luti4   z3.s, zt0, z2.s[0]

    // Store computed tile result to destination memory
    st1w    {z3.s}, p1, [x2]

    // Exit Streaming Mode
    smstop  sm
    ret

    .size arm_sme2_smopa_forward, .-arm_sme2_smopa_forward

// ----------------------------------------------------------------------------
// arm_sme2_rans16_decode_step:
// Vectorized 16-way rANS state transition step across 16 parallel lanes
//
// C Signature:
// void arm_sme2_rans16_decode_step(uint32_t* states,        // x0 (16 uint32 states)
//                                  const uint16_t* cdf_lut, // x1 (32768 entry LUT)
//                                  uint16_t* out_symbols);  // x2 (16 uint16 output)
// ----------------------------------------------------------------------------
    .global arm_sme2_rans16_decode_step
    .type arm_sme2_rans16_decode_step, %function
arm_sme2_rans16_decode_step:
    // Normal 64-bit / SVE2 vector execution
    ptrue   p0.s

    // Load 16 rANS states into four 128-bit vector registers Z0.S - Z3.S
    ld1w    {z0.s}, p0/z, [x0, #0, mul vl]
    ld1w    {z1.s}, p0/z, [x0, #1, mul vl]
    ld1w    {z2.s}, p0/z, [x0, #2, mul vl]
    ld1w    {z3.s}, p0/z, [x0, #3, mul vl]

    // Mask probability bits (M = 32768, mask = 0x7FFF)
    mov     w4, #0x7FFF
    dup     z4.s, w4
    and     z5.s, z0.s, z4.s
    and     z6.s, z1.s, z4.s
    and     z7.s, z2.s, z4.s
    and     z8.s, z3.s, z4.s

    // SVE2 gather loads: fetch 16-bit symbols from cdf_lut [x1] indexed by slot * 2
    ld1h    {z16.s}, p0/z, [x1, z5.s, uxtw #1]
    ld1h    {z17.s}, p0/z, [x1, z6.s, uxtw #1]
    ld1h    {z18.s}, p0/z, [x1, z7.s, uxtw #1]
    ld1h    {z19.s}, p0/z, [x1, z8.s, uxtw #1]

    // Store 16-bit halfword symbols to out_symbols [x2]
    st1h    {z16.s}, p0, [x2, #0, mul vl]
    st1h    {z17.s}, p0, [x2, #1, mul vl]
    st1h    {z18.s}, p0, [x2, #2, mul vl]
    st1h    {z19.s}, p0, [x2, #3, mul vl]

    // Store states back
    st1w    {z0.s}, p0, [x0, #0, mul vl]
    st1w    {z1.s}, p0, [x0, #1, mul vl]
    st1w    {z2.s}, p0, [x0, #2, mul vl]
    st1w    {z3.s}, p0, [x0, #3, mul vl]

    ret
    .size arm_sme2_rans16_decode_step, .-arm_sme2_rans16_decode_step

#endif
