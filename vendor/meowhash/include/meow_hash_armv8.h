//
// PLEASE SEE LICENSE.martins for the license for this file
// See also copyright and license in meow_hash_x86_aesni.h
//
#pragma once

#define MEOW_HASH_VERSION 5
#define MEOW_HASH_VERSION_NAME "0.5/calico"

// Meow hash v0.5 with ARMv8 Crypto Extension instructions
// Ported from https://github.com/cmuratori/meow_hash

// Performance on Pine A64 (Cortex-A53, 1.2GHz)
// (compiled with clang v10.0 with -O3 -mcpu=cortex-a53)
// C code    = ~0.34 bytes/cycle
// this code = ~1.75 bytes/cycle

// Performance on Jetson Nano (Cortex-A57, 1.43GHz)
// (compiled with clang v10.0 with -O3 -mcpu=cortex-a57)
// C code    = ~0.5 bytes/cycle
// this code = ~4.0 bytes/cycle (for ~1MB or less data)
//             ~2.5 bytes/cycle (for >1MB)

#include <stddef.h>
#include <stdint.h>
#include <arm_neon.h>

#if !defined MEOW_PAGESIZE
#define MEOW_PAGESIZE 4096
#endif

#if !defined(meow_u8)

#define meow_u8   uint8_t
#define meow_u32  uint32_t
#define meow_u64  uint64_t
#define meow_umm  size_t
#define meow_u128 uint8x16_t

#endif

#define MeowU32From(A, I) vgetq_lane_u32(vreinterpretq_u32_u8(A), (I))
#define MeowU64From(A, I) vgetq_lane_u64(vreinterpretq_u64_u8(A), (I))

#define MeowHashesAreEqual(A, B) (MeowU64From(A, 0) == MeowU64From(B, 0) && MeowU64From(A, 1) == MeowU64From(B, 1))

#define movdqu(A, B)     A = vld1q_u8(B)
#define movdqu_mem(A, B) vst1q_u8((A), (B))

#define movq(A, B)  A = vreinterpretq_u8_u64((uint64x2_t){ (B), 0 })
#define paddq(A, B) A = vreinterpretq_u8_u64(vaddq_u64(vreinterpretq_u64_u8(A), vreinterpretq_u64_u8(B)))

#define pxor_clear(A) A = (uint8x16_t){}
#define pxor(A, B)    A = veorq_u8(A, B)
#define pand(A, B)    A = vandq_u8(A, B)

#define palignr1(A, B) A =                                                                                     \
  vorrq_u8(                                                                                                    \
    vqtbl1q_u8(A, (uint8x16_t){0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0}), \
    vqtbl1q_u8(B, (uint8x16_t){1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,0xff})                                      \
  )

#define palignr15(A, B) A =                                                                                    \
  vorrq_u8(                                                                                                    \
    vqtbl1q_u8(A, (uint8x16_t){0xff,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14}),                                      \
    vqtbl1q_u8(B, (uint8x16_t){15,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff}) \
  )

#define pshufb(A, B) A = vqtbl1q_u8(A, B)

#define aesdec(A, B)        A = veorq_u8(vaesimcq_u8(vaesdq_u8((A), (uint8x16_t){})), (B))
#define aesdec_xor(A, X, B) A = veorq_u8(vaesimcq_u8(vaesdq_u8((A), (X))), (B))

#define MEOW_MIX_REG(r1, r2, r3, r4, r5, i1, i2, i3, i4) do { \
    aesdec(r1, r2);                                           \
    paddq(r3, i1);                                            \
    aesdec_xor(r2, i2, r4);                                   \
    paddq(r5, i3);                                            \
    pxor(r4, i4);                                             \
} while (0)

#define MEOW_MIX(r1, r2, r3, r4, r5, ptr) do {       \
   meow_u128 i1 = vld1q_u8(ptr + 15);                \
   meow_u128 i2 = vld1q_u8(ptr + 0);                 \
   meow_u128 i3 = vld1q_u8(ptr + 1);                 \
   meow_u128 i4 = vld1q_u8(ptr + 16);                \
   MEOW_MIX_REG(r1, r2, r3, r4, r5, i1, i2, i3, i4); \
} while (0)

#define MEOW_SHUFFLE(r1, r2, r3, r4, r5, r6) do { \
    aesdec(r1, r4);                               \
    paddq(r2, r5);                                \
    aesdec_xor(r4, r6, r2);                       \
    paddq(r5, r6);                                \
    pxor(r2, r3);                                 \
} while (0)

static meow_u8 MeowShiftAdjust[32] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};
static meow_u8 MeowMaskLen[32] = {255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0};

// NOTE(casey): The default seed is now a "nothing-up-our-sleeves" number for good measure.  You may verify that it is just an encoding of Pi.
static meow_u8 MeowDefaultSeed[128] =
{
    0x32, 0x43, 0xF6, 0xA8, 0x88, 0x5A, 0x30, 0x8D,
    0x31, 0x31, 0x98, 0xA2, 0xE0, 0x37, 0x07, 0x34,
    0x4A, 0x40, 0x93, 0x82, 0x22, 0x99, 0xF3, 0x1D,
    0x00, 0x82, 0xEF, 0xA9, 0x8E, 0xC4, 0xE6, 0xC8,
    0x94, 0x52, 0x82, 0x1E, 0x63, 0x8D, 0x01, 0x37,
    0x7B, 0xE5, 0x46, 0x6C, 0xF3, 0x4E, 0x90, 0xC6,
    0xCC, 0x0A, 0xC2, 0x9B, 0x7C, 0x97, 0xC5, 0x0D,
    0xD3, 0xF8, 0x4D, 0x5B, 0x5B, 0x54, 0x70, 0x91,
    0x79, 0x21, 0x6D, 0x5D, 0x98, 0x97, 0x9F, 0xB1,
    0xBD, 0x13, 0x10, 0xBA, 0x69, 0x8D, 0xFB, 0x5A,
    0xC2, 0xFF, 0xD7, 0x2D, 0xBD, 0x01, 0xAD, 0xFB,
    0x7B, 0x8E, 0x1A, 0xFE, 0xD6, 0xA2, 0x67, 0xE9,
    0x6B, 0xA7, 0xC9, 0x04, 0x5F, 0x12, 0xC7, 0xF9,
    0x92, 0x4A, 0x19, 0x94, 0x7B, 0x39, 0x16, 0xCF,
    0x70, 0x80, 0x1F, 0x2E, 0x28, 0x58, 0xEF, 0xC1,
    0x66, 0x36, 0x92, 0x0D, 0x87, 0x15, 0x74, 0xE6,
};

//
// NOTE(casey): Single block version
//

static meow_u128
MeowHash(void* Seed128Init, meow_umm Len, void* SourceInit)
{
    meow_u128 xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7; // NOTE(casey): xmm0-xmm7 are the hash accumulation lanes
    meow_u128 xmm8, xmm9, xmm10, xmm11, xmm12, xmm13, xmm14, xmm15; // NOTE(casey): xmm8-xmm15 hold values to be appended (residual, length)
    
    meow_u8* rax = (meow_u8*)SourceInit;
    meow_u8* rcx = (meow_u8*)Seed128Init;
    
    //
    // NOTE(casey): Seed the eight hash registers
    //
    
    movdqu(xmm0, rcx + 0x00);
    movdqu(xmm1, rcx + 0x10);
    movdqu(xmm2, rcx + 0x20);
    movdqu(xmm3, rcx + 0x30);
    
    movdqu(xmm4, rcx + 0x40);
    movdqu(xmm5, rcx + 0x50);
    movdqu(xmm6, rcx + 0x60);
    movdqu(xmm7, rcx + 0x70);
    
    //
    // NOTE(casey): Hash all full 256-byte blocks
    //
    
    meow_umm BlockCount = (Len >> 8);
    while (BlockCount--)
    {
        MEOW_MIX(xmm0,xmm4,xmm6,xmm1,xmm2, rax + 0x00);
        MEOW_MIX(xmm1,xmm5,xmm7,xmm2,xmm3, rax + 0x20);
        MEOW_MIX(xmm2,xmm6,xmm0,xmm3,xmm4, rax + 0x40);
        MEOW_MIX(xmm3,xmm7,xmm1,xmm4,xmm5, rax + 0x60);
        MEOW_MIX(xmm4,xmm0,xmm2,xmm5,xmm6, rax + 0x80);
        MEOW_MIX(xmm5,xmm1,xmm3,xmm6,xmm7, rax + 0xa0);
        MEOW_MIX(xmm6,xmm2,xmm4,xmm7,xmm0, rax + 0xc0);
        MEOW_MIX(xmm7,xmm3,xmm5,xmm0,xmm1, rax + 0xe0);
        
        rax += 0x100;
    }
    
    //
    // NOTE(casey): Load any less-than-32-byte residual
    //
    
    pxor_clear(xmm9);
    pxor_clear(xmm11);
    
    //
    // TODO(casey): I need to put more thought into how the end-of-buffer stuff is actually working out here,
    // because I _think_ it may be possible to remove the first branch (on Len8) and let the mask zero out the
    // result, but it would take a little thought to make sure it couldn't read off the end of the buffer due
    // to the & 0xf on the align computation.
    //
    
    // NOTE(casey): First, we have to load the part that is _not_ 16-byte aligned
    meow_u8 *Last = (meow_u8*)SourceInit + (Len & ~0xf);
    meow_u32 Len8 = (Len & 0xf);
    if (Len8)
    {
        // NOTE(casey): Load the mask early
        movdqu(xmm8, &MeowMaskLen[0x10 - Len8]);
        
        meow_u8 *LastOk = (meow_u8*)((((meow_umm)(((meow_u8 *)SourceInit)+Len - 1)) | (MEOW_PAGESIZE - 1)) - 16);
        int Align = (Last > LastOk) ? ((int)(meow_umm)Last) & 0xf : 0;
        movdqu(xmm10, &MeowShiftAdjust[Align]);
        movdqu(xmm9, Last - Align);
        pshufb(xmm9, xmm10);
        
        // NOTE(jeffr): and off the extra bytes
        pand(xmm9, xmm8);
    }
    
    // NOTE(casey): Next, we have to load the part that _is_ 16-byte aligned
    if (Len & 0x10)
    {
        xmm11 = xmm9;
        movdqu(xmm9, Last - 0x10);
    }
    
    //
    // NOTE(casey): Construct the residual and length injests
    //
    
    xmm8 = xmm9;
    xmm10 = xmm9;
    palignr15(xmm8, xmm11);
    palignr1(xmm10, xmm11);
    
    // NOTE(casey): We have room for a 128-bit nonce and a 64-bit none here, but
    // the decision was made to leave them zero'd so as not to confuse people
    // about hwo to use them or what security implications they had.
    pxor_clear(xmm12);
    pxor_clear(xmm13);
    pxor_clear(xmm14);
    movq(xmm15, Len);
    palignr15(xmm12, xmm15);
    palignr1(xmm14, xmm15);
    
    // NOTE(casey): To maintain the mix-down pattern, we always Meow Mix the less-than-32-byte residual, even if it was empty
    MEOW_MIX_REG(xmm0, xmm4, xmm6, xmm1, xmm2,  xmm8, xmm9, xmm10, xmm11);
    
    // NOTE(casey): Append the length, to avoid problems with our 32-byte padding
    MEOW_MIX_REG(xmm1, xmm5, xmm7, xmm2, xmm3,  xmm12, xmm13, xmm14, xmm15);
    
    //
    // NOTE(casey): Hash all full 32-byte blocks
    //
    meow_u32 LaneCount = (Len >> 5) & 0x7;
    if (LaneCount == 0) goto MixDown; MEOW_MIX(xmm2,xmm6,xmm0,xmm3,xmm4, rax + 0x00); --LaneCount;
    if (LaneCount == 0) goto MixDown; MEOW_MIX(xmm3,xmm7,xmm1,xmm4,xmm5, rax + 0x20); --LaneCount;
    if (LaneCount == 0) goto MixDown; MEOW_MIX(xmm4,xmm0,xmm2,xmm5,xmm6, rax + 0x40); --LaneCount;
    if (LaneCount == 0) goto MixDown; MEOW_MIX(xmm5,xmm1,xmm3,xmm6,xmm7, rax + 0x60); --LaneCount;
    if (LaneCount == 0) goto MixDown; MEOW_MIX(xmm6,xmm2,xmm4,xmm7,xmm0, rax + 0x80); --LaneCount;
    if (LaneCount == 0) goto MixDown; MEOW_MIX(xmm7,xmm3,xmm5,xmm0,xmm1, rax + 0xa0); --LaneCount;
    if (LaneCount == 0) goto MixDown; MEOW_MIX(xmm0,xmm4,xmm6,xmm1,xmm2, rax + 0xc0); --LaneCount;
    
    //
    // NOTE(casey): Mix the eight lanes down to one 128-bit hash
    //
    
    MixDown:
    
    MEOW_SHUFFLE(xmm0, xmm1, xmm2, xmm4, xmm5, xmm6);
    MEOW_SHUFFLE(xmm1, xmm2, xmm3, xmm5, xmm6, xmm7);
    MEOW_SHUFFLE(xmm2, xmm3, xmm4, xmm6, xmm7, xmm0);
    MEOW_SHUFFLE(xmm3, xmm4, xmm5, xmm7, xmm0, xmm1);
    MEOW_SHUFFLE(xmm4, xmm5, xmm6, xmm0, xmm1, xmm2);
    MEOW_SHUFFLE(xmm5, xmm6, xmm7, xmm1, xmm2, xmm3);
    MEOW_SHUFFLE(xmm6, xmm7, xmm0, xmm2, xmm3, xmm4);
    MEOW_SHUFFLE(xmm7, xmm0, xmm1, xmm3, xmm4, xmm5);
    MEOW_SHUFFLE(xmm0, xmm1, xmm2, xmm4, xmm5, xmm6);
    MEOW_SHUFFLE(xmm1, xmm2, xmm3, xmm5, xmm6, xmm7);
    MEOW_SHUFFLE(xmm2, xmm3, xmm4, xmm6, xmm7, xmm0);
    MEOW_SHUFFLE(xmm3, xmm4, xmm5, xmm7, xmm0, xmm1);
    
    paddq(xmm0, xmm2);
    paddq(xmm1, xmm3);
    paddq(xmm4, xmm6);
    paddq(xmm5, xmm7);
    pxor(xmm0, xmm1);
    pxor(xmm4, xmm5);
    paddq(xmm0, xmm4);
    
    return xmm0;
}

//
// NOTE(casey): Streaming construction
//

typedef struct
{
    meow_u128 xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7;
    meow_u64 TotalLengthInBytes;
    
    meow_u32 BufferLen;
    
    meow_u8 Buffer[256];
    meow_u128 Pad[2]; // NOTE(casey): So we know we can over-read Buffer as necessary
} meow_state;

static void
MeowBegin(meow_state* State, void* Seed128)
{
    meow_u8* rcx = (meow_u8*)Seed128;
    
    movdqu(State->xmm0, rcx + 0x00);
    movdqu(State->xmm1, rcx + 0x10);
    movdqu(State->xmm2, rcx + 0x20);
    movdqu(State->xmm3, rcx + 0x30);
    movdqu(State->xmm4, rcx + 0x40);
    movdqu(State->xmm5, rcx + 0x50);
    movdqu(State->xmm6, rcx + 0x60);
    movdqu(State->xmm7, rcx + 0x70);
    
    State->BufferLen = 0;
    State->TotalLengthInBytes = 0;
}

static void
MeowAbsorbBlocks(meow_state* State, meow_umm BlockCount, meow_u8* rax)
{
    meow_u128 xmm0 = State->xmm0;
    meow_u128 xmm1 = State->xmm1;
    meow_u128 xmm2 = State->xmm2;
    meow_u128 xmm3 = State->xmm3;
    meow_u128 xmm4 = State->xmm4;
    meow_u128 xmm5 = State->xmm5;
    meow_u128 xmm6 = State->xmm6;
    meow_u128 xmm7 = State->xmm7;
    
    while (BlockCount--)
    {
        MEOW_MIX(xmm0,xmm4,xmm6,xmm1,xmm2, rax + 0x00);
        MEOW_MIX(xmm1,xmm5,xmm7,xmm2,xmm3, rax + 0x20);
        MEOW_MIX(xmm2,xmm6,xmm0,xmm3,xmm4, rax + 0x40);
        MEOW_MIX(xmm3,xmm7,xmm1,xmm4,xmm5, rax + 0x60);
        MEOW_MIX(xmm4,xmm0,xmm2,xmm5,xmm6, rax + 0x80);
        MEOW_MIX(xmm5,xmm1,xmm3,xmm6,xmm7, rax + 0xa0);
        MEOW_MIX(xmm6,xmm2,xmm4,xmm7,xmm0, rax + 0xc0);
        MEOW_MIX(xmm7,xmm3,xmm5,xmm0,xmm1, rax + 0xe0);
        
        rax += 0x100;
    }
    
    State->xmm0 = xmm0;
    State->xmm1 = xmm1;
    State->xmm2 = xmm2;
    State->xmm3 = xmm3;
    State->xmm4 = xmm4;
    State->xmm5 = xmm5;
    State->xmm6 = xmm6;
    State->xmm7 = xmm7;
}

static void
MeowAbsorb(meow_state* State, meow_umm Len, void* SourceInit)
{
    State->TotalLengthInBytes += Len;
    meow_u8* Source = (meow_u8*)SourceInit;
    
    // NOTE(casey): Handle any buffered residual
    if (State->BufferLen)
    {
        meow_u32 Fill = (sizeof(State->Buffer) - State->BufferLen);
        if (Fill > Len)
        {
            Fill = (meow_u32)Len;
        }
        
        Len -= Fill;
        while (Fill--)
        {
            State->Buffer[State->BufferLen++] = *Source++;
        }
        
        if (State->BufferLen == sizeof(State->Buffer))
        {
            MeowAbsorbBlocks(State, 1, State->Buffer);
            State->BufferLen = 0;
        }
    }
    
    // NOTE(casey): Handle any full blocks
    meow_u64 BlockCount = (Len >> 8);
    meow_u64 Advance = (BlockCount << 8);
    MeowAbsorbBlocks(State, BlockCount, Source);
    
    Len -= Advance;
    Source += Advance;
    
    // NOTE(casey): Store residual
    while (Len--)
    {
        State->Buffer[State->BufferLen++] = *Source++;
    }
}

static meow_u128
MeowEnd(meow_state* State, meow_u8* Store128)
{
    meow_u64 Len = State->TotalLengthInBytes;
    
    meow_u128 xmm0 = State->xmm0;
    meow_u128 xmm1 = State->xmm1;
    meow_u128 xmm2 = State->xmm2;
    meow_u128 xmm3 = State->xmm3;
    meow_u128 xmm4 = State->xmm4;
    meow_u128 xmm5 = State->xmm5;
    meow_u128 xmm6 = State->xmm6;
    meow_u128 xmm7 = State->xmm7;
    
    meow_u128 xmm8, xmm9, xmm10, xmm11, xmm12, xmm13, xmm14, xmm15;
    
    meow_u8* rax = State->Buffer;
    
    pxor_clear(xmm9);
    pxor_clear(xmm11);
    
    meow_u8* Last = (uint8_t*)rax + (Len & 0xf0);
    meow_u32 Len8 = (Len & 0xf);
    if (Len8)
    {
        movdqu(xmm8, &MeowMaskLen[0x10 - Len8]);
        movdqu(xmm9, Last);
        pand(xmm9, xmm8);
    }
    
    if (Len & 0x10)
    {
        xmm11 = xmm9;
        movdqu(xmm9, Last - 0x10);
    }
    
    xmm8 = xmm9;
    xmm10 = xmm9;
    palignr15(xmm8, xmm11);
    palignr1(xmm10, xmm11);
    
    pxor_clear(xmm12);
    pxor_clear(xmm13);
    pxor_clear(xmm14);
    movq(xmm15, Len);
    palignr15(xmm12, xmm15);
    palignr1(xmm14, xmm15);
    
    // NOTE(casey): To maintain the mix-down pattern, we always Meow Mix the less-than-32-byte residual, even if it was empty
    MEOW_MIX_REG(xmm0, xmm4, xmm6, xmm1, xmm2,  xmm8, xmm9, xmm10, xmm11);
    
    // NOTE(casey): Append the length, to avoid problems with our 32-byte padding
    MEOW_MIX_REG(xmm1, xmm5, xmm7, xmm2, xmm3,  xmm12, xmm13, xmm14, xmm15);

    //
    // NOTE(casey): Hash all full 32-byte blocks
    //
    meow_u32 LaneCount = (Len >> 5) & 0x7;
    if( LaneCount == 0) goto MixDown; MEOW_MIX(xmm2,xmm6,xmm0,xmm3,xmm4, rax + 0x00); --LaneCount;
    if( LaneCount == 0) goto MixDown; MEOW_MIX(xmm3,xmm7,xmm1,xmm4,xmm5, rax + 0x20); --LaneCount;
    if( LaneCount == 0) goto MixDown; MEOW_MIX(xmm4,xmm0,xmm2,xmm5,xmm6, rax + 0x40); --LaneCount;
    if( LaneCount == 0) goto MixDown; MEOW_MIX(xmm5,xmm1,xmm3,xmm6,xmm7, rax + 0x60); --LaneCount;
    if( LaneCount == 0) goto MixDown; MEOW_MIX(xmm6,xmm2,xmm4,xmm7,xmm0, rax + 0x80); --LaneCount;
    if( LaneCount == 0) goto MixDown; MEOW_MIX(xmm7,xmm3,xmm5,xmm0,xmm1, rax + 0xa0); --LaneCount;
    if( LaneCount == 0) goto MixDown; MEOW_MIX(xmm0,xmm4,xmm6,xmm1,xmm2, rax + 0xc0); --LaneCount;
    
    //
    // NOTE(casey): Mix the eight lanes down to one 128-bit hash
    //
    
    MixDown:
    
    MEOW_SHUFFLE(xmm0, xmm1, xmm2, xmm4, xmm5, xmm6);
    MEOW_SHUFFLE(xmm1, xmm2, xmm3, xmm5, xmm6, xmm7);
    MEOW_SHUFFLE(xmm2, xmm3, xmm4, xmm6, xmm7, xmm0);
    MEOW_SHUFFLE(xmm3, xmm4, xmm5, xmm7, xmm0, xmm1);
    MEOW_SHUFFLE(xmm4, xmm5, xmm6, xmm0, xmm1, xmm2);
    MEOW_SHUFFLE(xmm5, xmm6, xmm7, xmm1, xmm2, xmm3);
    MEOW_SHUFFLE(xmm6, xmm7, xmm0, xmm2, xmm3, xmm4);
    MEOW_SHUFFLE(xmm7, xmm0, xmm1, xmm3, xmm4, xmm5);
    MEOW_SHUFFLE(xmm0, xmm1, xmm2, xmm4, xmm5, xmm6);
    MEOW_SHUFFLE(xmm1, xmm2, xmm3, xmm5, xmm6, xmm7);
    MEOW_SHUFFLE(xmm2, xmm3, xmm4, xmm6, xmm7, xmm0);
    MEOW_SHUFFLE(xmm3, xmm4, xmm5, xmm7, xmm0, xmm1);
    
    if (Store128)
    {
        movdqu_mem(Store128 + 0x00, xmm0);
        movdqu_mem(Store128 + 0x10, xmm1);
        movdqu_mem(Store128 + 0x20, xmm2);
        movdqu_mem(Store128 + 0x30, xmm3);
        movdqu_mem(Store128 + 0x40, xmm4);
        movdqu_mem(Store128 + 0x50, xmm5);
        movdqu_mem(Store128 + 0x60, xmm6);
        movdqu_mem(Store128 + 0x70, xmm7);
    }
    
    paddq(xmm0, xmm2);
    paddq(xmm1, xmm3);
    paddq(xmm4, xmm6);
    paddq(xmm5, xmm7);
    pxor(xmm0, xmm1);
    pxor(xmm4, xmm5);
    paddq(xmm0, xmm4);
    
    return(xmm0);
}

#undef movdqu
#undef movdqu_mem
#undef movq
#undef aesdec
#undef pxor
#undef paddq
#undef pand
#undef palignr1
#undef palignr15
#undef pxor_clear
#undef MEOW_MIX
#undef MEOW_MIX_REG
#undef MEOW_SHUFFLE

//
// NOTE(casey): If you need to create your own seed from non-random data, you can use MeowExpandSeed
// to create a seed which you then store for repeated use.  It is _expensive_ to generate the seed,
// so you do not want to do this every time you hash.  You _only_ want to do it when you actually
// need to create a new seed.
//

static void
MeowExpandSeed(meow_umm InputLen, void* Input, meow_u8* SeedResult)
{
    meow_state State;
    meow_u64 LengthTab = (meow_u64)InputLen; // NOTE(casey): We need to always injest 8-byte lengths exactly, even on 32-bit builds, to ensure identical results
    meow_umm InjestCount = (256 / InputLen) + 2;
    
    MeowBegin(&State, MeowDefaultSeed);
    MeowAbsorb(&State, sizeof(LengthTab), &LengthTab);
    while (InjestCount--)
    {
        MeowAbsorb(&State, InputLen, Input);
    }
    MeowEnd(&State, SeedResult);
}
