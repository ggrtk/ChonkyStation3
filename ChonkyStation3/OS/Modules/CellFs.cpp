#include "CellFs.hpp"
#include "PlayStation3.hpp"


u64 CellFs::cellFsClose() {
    const u32 file_id = ARG0;
    log("cellFsClose(file_id: %d)\n", file_id);

    ps3->fs.close(file_id);
    return Result::CELL_OK;
}

u64 CellFs::cellFsRead() {
    const u32 file_id = ARG0;
    const u32 buf = ARG1;
    const u64 size = ARG2;
    const u32 bytes_read_ptr = ARG3;    // bytes_read is u64
    log("cellFsRead(file_id: %d, buf: 0x%08x, size: %lld, bytes_read_ptr: 0x%08x)\n", file_id, buf, size, bytes_read_ptr);

    const u64 bytes_read = ps3->fs.read(file_id, ps3->mem.getPtr(buf), size);
    ps3->mem.write<u64>(bytes_read_ptr, bytes_read);

    return Result::CELL_OK;
}

u64 CellFs::cellFsOpen() {
    const u32 path_ptr = ARG0;
    const s32 flags = ARG1;
    const u32 file_id_ptr = ARG2;
    const u32 arg_ptr = ARG3;
    const u64 size = ARG4;
    const std::string path = Helpers::readString(ps3->mem.getPtr(path_ptr));
    log("cellFsOpen(path_ptr: 0x%08x, flags: %d, file_id_ptr: 0x%08x, arg_ptr: 0x%08x, size: %d) [path: %s]\n", path_ptr, flags, file_id_ptr, arg_ptr, size, path.c_str());

    const u32 file_id = ps3->fs.open(path);
    ps3->mem.write<u32>(file_id_ptr, file_id);

    return Result::CELL_OK;
}

u64 CellFs::cellFsStat() {
    const u32 path_ptr = ARG0;
    const u32 stat_ptr = ARG1;
    const std::string path = Helpers::readString(ps3->mem.getPtr(path_ptr));
    log("cellFsStat(path_ptr: 0x%08x, stat_ptr: 0x%08x) [path: %s]\n", path_ptr, stat_ptr, path.c_str());

    return Result::CELL_OK;
}

u64 CellFs::cellFsLseek() {
    const u32 file_id = ARG0;
    const s64 offs = ARG1;
    const u32 seek_mode = ARG2;
    const u32 pos_ptr = ARG3;   // pos is u64
    log("cellFSLseek(file_id: %d, offs: %d, seek_mode: %d, pos_ptr: 0x%08x)\n", file_id, offs, seek_mode, pos_ptr);
    
    const u64 pos = ps3->fs.seek(file_id, offs, seek_mode);
    ps3->mem.write<u64>(pos_ptr, pos);

    return Result::CELL_OK;
}