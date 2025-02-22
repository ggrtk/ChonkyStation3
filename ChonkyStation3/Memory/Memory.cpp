#include "Memory.hpp"


// Allocates size bytes of physical memory. Returns physical address of the allocated memory.
MemoryRegion::Block* MemoryRegion::allocPhys(size_t size) {
    // Page alignment
    size_t aligned_size = pageAlign(size);
    // Find the next free block of memory big enough for the given size
    u64 addr = 0;
    while (true) {
        auto next_block = findNextBlock(addr);
        if (next_block.first) {
            if (next_block.second->start - addr >= aligned_size) {
                // addr OK
                break;
            }
            else {
                // Keep searching
                addr = next_block.second->start + next_block.second->size;
                if (addr >= RAM_SIZE)
                    Helpers::panic("Out of memory\n");
            }
        }
        else {
            // addr OK
            break;
        }
    }

    // Did we run out of memory?
    if (addr + aligned_size >= RAM_SIZE)
        Helpers::panic("Out of memory\n");

    // Allocate block
    blocks.push_back({ addr, aligned_size });

    return &blocks.back();
}

// Allocates and maps size bytes of memory. Returns virtual address of allocated memory. Marks allocated area as fastmem. Optionally specify the lowest possible virtual address to allocate
MemoryRegion::MapEntry* MemoryRegion::alloc(size_t size, u64 start_addr) {
    // Page alignment
    size_t aligned_size = pageAlign(size);
    // Allocate block of memory
    u64 paddr = allocPhys(aligned_size)->start;

    // Map area
    u64 vaddr = findNextAllocatableVaddr(size, start_addr);
    MapEntry* entry = mmap(vaddr, paddr, aligned_size);
    
    // Fastmem
    for (u64 page_addr = entry->vaddr; page_addr < entry->vaddr + entry->size; page_addr += PAGE_SIZE) {
        const u64 page = page_addr >> PAGE_SHIFT;
        u8* ptr = getPtrPhys(paddr);
        mem_manager.markAsFastMem(page, ptr, true, true);
        paddr += PAGE_SIZE;
    }

    log("Allocated 0x%08llx bytes at 0x%016llx\n", aligned_size, vaddr);
    return entry;
}

void MemoryRegion::free(MemoryRegion::MapEntry* entry) {
    // Get the block this entry is mapped to
    auto block = findBlockFromAddr(entry->paddr);
    // Remove block
    for (int i = 0; i < blocks.size(); i++) {
        if (blocks[i].start == block.second->start) {
            blocks.erase(blocks.begin() + i);
            break;
        }
    }
    // Remove map entry
    for (int i = 0; i < map.size(); i++) {
        if (map[i].vaddr == entry->vaddr) {
            map.erase(map.begin() + i);
            break;
        }
    }
}

// Returns whether the given physical address is part of an allocated memory block and, in case it is, returns the block info.
std::pair<bool, MemoryRegion::Block*> MemoryRegion::findBlockFromAddr(u64 paddr) {
    for (auto& i : blocks) {
        if (Helpers::inRange<u32>(paddr, i.start, i.start + i.size - 1)) return { true, &i };
    }

    return { false, nullptr };
}

// Returns whether there is an allocated memory block after the given physical address and, in case there is, returns the block info.
std::pair<bool, MemoryRegion::Block*> MemoryRegion::findNextBlock(u64 start_addr) {
    Block* block = nullptr;
    for (auto& i : blocks) {
        if (i.start >= start_addr) {
            if (block == nullptr)
                block = &i;
            else if (i.start < block->start)
                block = &i;
        }
    }

    return { block != nullptr, block };
}

// Returns whether there is an allocated block in the physical address space with the given handle and, in case there is, returns the block info.
std::pair<bool, MemoryRegion::Block*> MemoryRegion::findBlockWithHandle(u64 handle) {
    for (auto& i : blocks) {
        if (i.handle == handle)
            return { true, &i };
    }
    return { false, nullptr };
}

// Frees a physical block with the given handle, or does nothing if there is no block with the given handle.
void MemoryRegion::freeBlockWithHandle(u64 handle) {
    // Get the block
    auto block = findBlockWithHandle(handle);
    // Remove the block
    for (int i = 0; i < blocks.size(); i++) {
        if (blocks[i].start == block.second->start) {
            blocks.erase(blocks.begin() + i);
            break;
        }
    }
}

// Returns whether there is a mapped area in the virtual address space after the given virtual address and, in case there is, returns the map info.
std::pair<bool, MemoryRegion::MapEntry*> MemoryRegion::findNextMappedArea(u64 start_addr) {
    MapEntry* map_entry = nullptr;
    for (auto& i : map) {
        if (i.vaddr >= start_addr) {
            if (map_entry == nullptr)
                map_entry = &i;
            else if (i.vaddr < map_entry->vaddr)
                map_entry = &i;
        }
    }

    return { map_entry != nullptr, map_entry };
}

// Returns the first available unmapped region in the virtual address space big enough to fit size bytes, or 0 if there is none.
u64 MemoryRegion::findNextAllocatableVaddr(size_t size, u64 start_addr) {
    u64 vaddr = (start_addr == 0) ? virtual_base : start_addr;
    u64 aligned_size = pageAlign(size);

    // Find the next free area in the address map
    //log("Searching allocateable area...\n");
    while (true) {
        auto next_area = findNextMappedArea(vaddr);
        //log("0x%016llx (next area at 0x%016llx, spacing: 0x%016llx)...", vaddr, next_area.second->vaddr, next_area.second->vaddr - vaddr);
        if (next_area.first) {
            if (next_area.second->vaddr - vaddr >= aligned_size) {
                // addr OK
                break;
            }
            else {
                // Keep searching
                vaddr = next_area.second->vaddr + next_area.second->size;
                //log(" not ok\n");
            }
        }
        else {
            // addr OK
            break;
        }
    }
    //log(" ok\n");
    return vaddr;
}

// Returns whether there is a mapped area in the virtual address space with the given handle and, in case there is, returns the map info.
std::pair<bool, MemoryRegion::MapEntry*> MemoryRegion::findMapEntryWithHandle(u64 handle) {
    for (auto& i : map) {
        if (i.handle == handle)
            return { true, &i };
    }
    return { false, nullptr };
}

// Checks if the given virtual address is mapped to a physical address.
// If it is, also return the map entry.
std::pair<bool, MemoryRegion::MapEntry*> MemoryRegion::isMapped(u64 vaddr) {
    for (auto& i : map) {
        if (Helpers::inRange<u32>(vaddr, i.vaddr, i.vaddr + i.size - 1)) return { true, &i };
    }

    return { false, nullptr };
}

// Maps a virtual address to a physical one.
MemoryRegion::MapEntry* MemoryRegion::mmap(u64 vaddr, u64 paddr, size_t size) {
    if (isMapped(vaddr).first) {
        printAddressMap();
        Helpers::panic("Tried to map an already mapped virtual address at 0x%016llx\n", vaddr);
    }

    // Page alignment
    size_t aligned_size = pageAlign(size);

    map.push_back({ vaddr, paddr, aligned_size });
    return &map.back();
}

// Unmaps the region starting at the given virtual address
void MemoryRegion::unmap(u64 vaddr) {
    for (int i = 0; i < map.size(); i++) {
        if (map[i].vaddr == vaddr) {
            map.erase(map.begin() + i);
        }
    }
}

// Translates a virtual address
u64 MemoryRegion::translateAddr(u64 vaddr) {
    auto [mapped, map] = isMapped(vaddr);
    if (!mapped) Helpers::panic("Tried to access unmapped vaddr 0x%016x\n", vaddr);

    return map->paddr + (vaddr - map->vaddr);
}

// Returns a pointer to the data at the specified physical address
u8* MemoryRegion::getPtrPhys(u64 paddr) {
    return &mem[paddr];
}

// Returns amount of available memory
u64 MemoryRegion::getAvailableMem() {
    u64 size = 0;
    for (auto& i : blocks)
        size += i.size;
    return RAM_SIZE - size;
}


// Returns a pointer to the memory region the address is part of (or nullptr if the address is unmapped), and an offset into the aforementioned memory (or 0 if the address is unmapped).
std::pair<u64, u8*> Memory::addrToOffsetInMemory(u64 vaddr) {
    for (auto& i : regions) {
        if (i->isMapped(vaddr).first) {
            return { i->translateAddr(vaddr), i->mem };
        }
    }

    return { 0, nullptr };
}

// Marks a page of memory as fastmem
void Memory::markAsFastMem(u64 page, u8* ptr, bool r, bool w) {
    if (r) read_table [page] = ptr;
    if (w) write_table[page] = ptr;
}

// Marks a page of memory as slowmem (removes it from the fastmem page table)
void Memory::markAsSlowMem(u64 page, bool r, bool w) {
    if (r) read_table [page] = 0;
    if (w) write_table[page] = 0;
}

// Returns a pointer to the data at the specified virtual address
u8* Memory::getPtr(u64 vaddr) {
    auto [offset, mem] = addrToOffsetInMemory(vaddr);
    if (!mem) Helpers::panic("Tried to access unmapped vaddr 0x%016llx\n", vaddr);
    return &mem[offset];
}

template<typename T>
T Memory::read(u64 vaddr) {
    const u64 page = vaddr >> PAGE_SHIFT;
    const u64 offs = vaddr & PAGE_MASK;

    u8* ptr = read_table[page];
    // Fastmem
    if (ptr)
        return Helpers::bswap<T>(*(T*)(&ptr[offs]));
    // Slowmem
    else {
        auto [offset, mem] = addrToOffsetInMemory(vaddr);
        Helpers::debugAssert(mem != nullptr, "Tried to read unmapped vaddr 0x%016llx\n", vaddr);
        T data;

        std::memcpy(&data, &mem[offset], sizeof(T));

        if (watchpoints_r.contains(vaddr))
            watchpoints_r[vaddr]();

        return Helpers::bswap<T>(data);
    }
}
template u8  Memory::read(u64 vaddr);
template u16 Memory::read(u64 vaddr);
template u32 Memory::read(u64 vaddr);
template u64 Memory::read(u64 vaddr);

template<typename T>
void Memory::write(u64 vaddr, T data) {
    data = Helpers::bswap<T>(data);
    
    const u64 page = vaddr >> PAGE_SHIFT;
    const u64 offs = vaddr & PAGE_MASK;

    u8* ptr = write_table[page];
    // Fastmem
    if (ptr)
        *(T*)(&ptr[offs]) = data;
    // Slowmem
    else {
        auto [offset, mem] = addrToOffsetInMemory(vaddr);
        Helpers::debugAssert(mem != nullptr, "Tried to write unmapped vaddr 0x%016llx\n", vaddr);

        std::memcpy(&mem[offset], &data, sizeof(T));

        if (watchpoints_w.contains(vaddr))
            watchpoints_w[vaddr]();
    }
}
template void Memory::write(u64 vaddr, u8  data);
template void Memory::write(u64 vaddr, u16 data);
template void Memory::write(u64 vaddr, u32 data);
template void Memory::write(u64 vaddr, u64 data);