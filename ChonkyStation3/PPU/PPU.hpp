#pragma once

#include <common.hpp>
#include <BitField.hpp>

#include <Memory.hpp>
#include <PPUTypes.hpp>
#include <PPUDisassembler.hpp>

// Circular dependency
class PlayStation3;

class PPU {
public:
    PPU(Memory& mem, PlayStation3* ps3) : mem(mem), ps3(ps3) {}
    Memory& mem;
    PlayStation3* ps3;
    virtual void step();
    void runFunc(u32 addr, u32 toc = 0);

    PPUTypes::State state;

    bool should_log = false;    // For debugging, unused normally

    void printState();
    bool doesAnyRegContain(u64 val);
    bool doesAnyRegContainMasked(u64 val, u64 mask);

    bool branchCondition(u8 bo, u8 bi);
    static u32 rotationMask(u32 mb, u32 me);
};