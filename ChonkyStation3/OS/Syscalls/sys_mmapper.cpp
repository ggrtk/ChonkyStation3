#include <Syscall.hpp>
#include "PlayStation3.hpp"


MAKE_LOG_FUNCTION(log, sys_mmapper);

static u32 next_address_alloc = 0x30000000; // TODO: store this addr somewhere instead of hardcoding it

u64 Syscall::sys_mmapper_allocate_address() {
    const u64 size = ARG0;
    const u64 flags = ARG1;
    const u64 alignment = ARG2;
    const u32 addr_ptr = ARG3;
    log("sys_mmapper_allocate_address(size: 0x%016llx, flags: 0x%016llx, alignment: 0x%016llx, addr_ptr: 0x%08x)\n", size, flags, alignment, addr_ptr);

    while (ps3->mem.isMapped(next_address_alloc).first)
        next_address_alloc += 256_MB;
    ps3->mem.write<u32>(addr_ptr, next_address_alloc);
    next_address_alloc += 256_MB;

    return Result::CELL_OK;
}

u64 Syscall::sys_mmapper_search_and_map() {
    const u32 start_addr = ARG0;
    const u32 handle = ARG1;
    const u64 flags = ARG2;
    const u32 addr_ptr = ARG3;
    log("sys_mmapper_search_and_map(start_addr: 0x%08x, handle: 0x%08x, flags: 0x%016llx, addr_ptr: 0x%08x)\n", start_addr, handle, flags, addr_ptr);
    auto block = ps3->mem.ram.findBlockWithHandle(handle);
    Helpers::debugAssert(block.first, "sys_mmapper_search_and_map: unknown handle\n");

    // Find area to map block to
    u64 vaddr = ps3->mem.findNextAllocatableVaddr(block.second->size, start_addr);
    auto entry = ps3->mem.mmap(vaddr, block.second->start, block.second->size);

    ps3->mem.write<u32>(addr_ptr, entry->vaddr);

    return Result::CELL_OK;
}