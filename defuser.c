#include "defuser.h"

#include <inttypes.h>
#include <stdio.h>

#define MAX_FIELDS 8

/*
 * 把 values 中的 count 个整数写成一行“空格分隔的无符号十进制整数”，存入 out。
 * 成功返回 0；count 为 0（还没有实现）、snprintf 出错或结果被截断时返回非零。
 */
static int write_answer(char *out, size_t cap, const uint32_t *values, size_t count) {
    size_t used = 0;
    for (size_t i = 0; i < count; ++i) {
        int size = snprintf(out + used, cap - used, "%s%" PRIu32, i ? " " : "", values[i]);
        if (size < 0 || (size_t)size >= cap - used) return 1;
        used += (size_t)size;
    }
    return count == 0;
}

static uint32_t rol32(uint32_t value, unsigned shift) {
    return (value << shift) | (value >> (32 - shift));
}

static uint32_t ror32(uint32_t value, unsigned shift) {
    return (value >> shift) | (value << (32 - shift));
}

static uint32_t p1_compute_answer(uint64_t challenge) {
    uint32_t low = (uint32_t)challenge;
    uint32_t high = (uint32_t)(challenge >> 32);
    uint32_t step2 = (UINT32_C(3) * high + low) ^ UINT32_C(0x5886);
    uint32_t step0 = (low + UINT32_C(4) * (uint32_t)(challenge >> 16)) ^ UINT32_C(0xad08);
    return UINT32_C(2) * step0 + (step2 ^ UINT32_C(0x5d8e));
}

static uint32_t p3_dispatch_answer(uint32_t index, uint32_t value) {
    switch (index) {
        case 0: return UINT32_C(3) * value + UINT32_C(0x87ba);
        case 1: return UINT32_C(5) * value + UINT32_C(0x48652);
        case 2: return (value ^ UINT32_C(0x0286)) + (value >> 2);
        case 3: return UINT32_C(5) * value + UINT32_C(0x6b3f);
        case 4: return UINT32_C(3) * value + UINT32_C(0x33dd);
        case 5: return (UINT32_C(2) * value) ^ UINT32_C(0xf8ba);
        case 6: return (UINT32_C(4) * value) ^ UINT32_C(0x60ec);
        case 7: return (value ^ UINT32_C(0x80c9)) + (value >> 2);
        default: return 0;
    }
}

static uint32_t p4_walk_answer(uint32_t target, uint32_t low, uint32_t high,
                               uint32_t mix) {
    uint32_t mid = low + ((high - low) >> 1);
    if (target == mid) return (mid + UINT32_C(0x7c)) ^ mix;

    uint32_t inner;
    uint32_t branch_mask;
    if (target < mid) {
        inner = p4_walk_answer(target, low, mid - 1, mix);
        branch_mask = UINT32_C(0x14);
    } else {
        inner = p4_walk_answer(target, mid + 1, high, mix);
        branch_mask = UINT32_C(0x9);
    }
    return (UINT32_C(3) * inner) ^ (mid + mix) ^ branch_mask;
}

struct p5_node {
    uint32_t accumulator;
    uint32_t value;
    int next;
};

static const struct p5_node p5_nodes[] = {
    {UINT32_C(0xae01), UINT32_C(11), 5},
    {UINT32_C(0x77f1), UINT32_C(0x064e), 9},
    {UINT32_C(0xef97), UINT32_C(0x0854), 10},
    {UINT32_C(0xcb47), UINT32_C(0x0a15), -1},
    {UINT32_C(0x707e), UINT32_C(0x0405), 8},
    {UINT32_C(0xd8ba), UINT32_C(0x035b), 7},
    {UINT32_C(0x16c7), UINT32_C(0x092d), 11},
    {UINT32_C(0x3280), UINT32_C(0x0203), 2},
    {UINT32_C(0x4737), UINT32_C(0x0739), 6},
    {UINT32_C(0xc922), UINT32_C(0x0115), 0},
    {UINT32_C(0x1937), UINT32_C(0x0528), 4},
    {UINT32_C(0x34c8), UINT32_C(0x0b3d), 3}
};

static const unsigned p5_heads[] = {1, 9, 0, 5, 7, 2, 10, 4};

static uint32_t p5_traverse_answer(uint64_t challenge, uint32_t *last_value) {
    unsigned node_index = p5_heads[(unsigned)challenge & 7u];
    unsigned steps = (unsigned)(challenge >> 3) & 3u;
    uint32_t acc = (uint32_t)((challenge >> 6) ^ (challenge >> 32));
    acc ^= UINT32_C(0xdbcf);
    acc += p5_nodes[node_index].accumulator;
    acc = UINT32_C(5) * acc;
    *last_value = p5_nodes[node_index].value;
    acc ^= *last_value;

    for (unsigned i = 0; i < steps; ++i) {
        node_index = (unsigned)p5_nodes[node_index].next;
        acc += p5_nodes[node_index].accumulator;
        acc = UINT32_C(5) * acc;
        *last_value = p5_nodes[node_index].value;
        acc ^= *last_value;
    }
    return acc;
}

static uint32_t p6_op(unsigned op, uint32_t value) {
    switch (op) {
        case 0: return (value ^ UINT32_C(0x5c2a)) + UINT32_C(4) * value;
        case 1: return rol32(value, 5) ^ UINT32_C(0xc69a);
        case 2: return ror32(value, 7);
        case 3: return (UINT32_C(2) * value + UINT32_C(0x9966)) ^ (UINT32_C(5) * value);
        case 4: return (value ^ (value >> 1)) + UINT32_C(0x4831);
        case 5: return (value + UINT32_C(0x221)) ^ (value >> 7);
        default: return 0;
    }
}

static uint32_t p6_dispatch_answer(uint64_t challenge) {
    static const unsigned ops[8] = {2, 5, 1, 4, 1, 3, 0, 5};
    unsigned first = (unsigned)challenge & 7u;
    unsigned second = (unsigned)(challenge >> 3) & 7u;
    uint32_t argument = (uint32_t)((challenge >> 6) ^ (challenge >> 32));
    uint32_t intermediate = p6_op(ops[first], argument) ^ UINT32_C(0xd141);
    return p6_op(ops[second], intermediate) + UINT32_C(0x6658);
}

int solve_phase_1(uint64_t challenge, char *out, size_t cap) {
    uint32_t values[MAX_FIELDS] = {0};
    size_t count = 1;
    values[0] = p1_compute_answer(challenge);
    return write_answer(out, cap, values, count);
}

int solve_phase_2(uint64_t challenge, char *out, size_t cap) {
    uint32_t values[MAX_FIELDS] = {0};
    size_t count = 6;
    values[0] = UINT32_C(3) * ((uint32_t)(challenge >> 34) & UINT32_C(0x3f));
    for (uint32_t i = 1; i < 6; ++i) {
        uint32_t part = ((uint32_t)(challenge >> (2 * i + 3)) & UINT32_C(0x1f)) + i;
        values[i] = UINT32_C(5) * (part ^ values[i - 1]);
    }
    return write_answer(out, cap, values, count);
}

int solve_phase_3(uint64_t challenge, char *out, size_t cap) {
    uint32_t values[MAX_FIELDS] = {0};
    size_t count = 2;
    values[0] = (uint32_t)challenge & 7u;
    uint32_t mixed = (uint32_t)(challenge >> 32) ^ (uint32_t)(challenge >> 3);
    values[1] = p3_dispatch_answer(values[0], mixed);
    return write_answer(out, cap, values, count);
}

int solve_phase_4(uint64_t challenge, char *out, size_t cap) {
    uint32_t values[MAX_FIELDS] = {0};
    size_t count = 1;
    uint32_t target = (uint32_t)challenge & UINT32_C(0x7f);
    uint32_t mix = (uint32_t)(challenge >> 7) ^ (uint32_t)(challenge >> 32);
    values[0] = p4_walk_answer(target, 0, UINT32_C(0x7f), mix);
    return write_answer(out, cap, values, count);
}

int solve_phase_5(uint64_t challenge, char *out, size_t cap) {
    uint32_t values[MAX_FIELDS] = {0};
    size_t count = 2;
    values[0] = p5_traverse_answer(challenge, &values[1]);
    return write_answer(out, cap, values, count);
}

int solve_phase_6(uint64_t challenge, char *out, size_t cap) {
    uint32_t values[MAX_FIELDS] = {0};
    size_t count = 1;
    values[0] = p6_dispatch_answer(challenge);
    return write_answer(out, cap, values, count);
}

int solve_secret(uint64_t challenge, uint64_t context, char *out, size_t cap) {
    uint32_t first = (uint32_t)context & 3u;
    uint32_t context_mix = (uint32_t)context ^ (uint32_t)(context >> 32);
    uint32_t challenge_mix = (uint32_t)challenge ^ (uint32_t)(challenge >> 32);
    uint32_t second = ror32(context_mix ^ challenge_mix, 7) ^ UINT32_C(0xaf45);
    second ^= first * UINT32_C(0x160e) ^ (uint32_t)(context >> 16);
    uint32_t third = second ^
                     (context_mix + first + UINT32_C(0xb94b) + (challenge_mix >> 7));
    int size = snprintf(out, cap,
                        "0123456789abcdef0123456789abcdefOPEN %" PRIu32 " %" PRIu32 " %" PRIu32,
                        first, second, third);
    return size < 0 || (size_t)size >= cap;
}
