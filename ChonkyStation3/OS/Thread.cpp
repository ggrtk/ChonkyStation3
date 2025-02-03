#include "Thread.hpp"
#include "ThreadManager.hpp"
#include "PlayStation3.hpp"


Thread::Thread(u64 entry, u64 stack_size, u64 arg, const u8* name, u32 id, u32 tls_vaddr, u32 tls_filesize, u32 tls_memsize, ThreadManager* mgr) : mgr(mgr) {
    const u32 real_entry = mgr->ps3->mem.read<u32>(entry);
    this->id = id;
    this->name = Helpers::readString(name);
    stack = mgr->allocateStack(stack_size);
    this->stack_size = stack_size;  // Add 8 because we simulate 1 empty argument and 1 of whatever env is

    const u64 sp = STACK_REGION_START + stack_size;
    state.pc = real_entry;
    state.gprs[0] = state.pc;
    state.gprs[1] = sp - 8;
    state.gprs[2] = mgr->ps3->mem.read<u32>(entry + 4);

    if (tls_vaddr) {
        // Initialize TLS
        mgr->ps3->module_manager.sysThread.initializeTLS(id, tls_vaddr, tls_filesize, tls_memsize, state);
    }

    state.gprs[3] = arg;
    state.gprs[7] = id;
    state.gprs[8] = tls_vaddr;
    state.gprs[9] = tls_filesize;
    state.gprs[10] = tls_memsize;
    state.gprs[12] = PAGE_SIZE;
}

void Thread::addArg(u64 arg) {
    args.push_back(arg);
}

void Thread::finalizeArgs() {
    for (int i = 0; i < args.size(); i++) {
        state.gprs[1] -= 8;
        mgr->ps3->mem.write<u64>(state.gprs[1], args[i]);
    }

    state.gprs[3] = args.size();
    state.gprs[4] = state.gprs[1];
    printf("argc for thread \"%s\": %lld\n", name.c_str(), state.gprs[3]);
    printf("argv for thread \"%s\": 0x%08x\n", name.c_str(), (u32)state.gprs[4]);

    state.gprs[28] = state.gprs[4];
    state.gprs[29] = state.gprs[3];
}

void Thread::addEnv(u64 arg) {
    env.push_back(arg);
}

void Thread::finalizeEnv() {
    for (int i = 0; i < env.size(); i++) {
        state.gprs[1] -= 8;
        mgr->ps3->mem.write<u64>(state.gprs[1], env[i]);
    }

    state.gprs[6] = args.size();
    state.gprs[5] = state.gprs[1];
    printf("envc for thread \"%s\": %lld\n", name.c_str(), state.gprs[6]);
    printf("envv for thread \"%s\": 0x%08x\n", name.c_str(), (u32)state.gprs[5]);

    state.gprs[31] = state.gprs[5];
}

void Thread::finalizeArgsAndEnv() {
    state.gprs[1] -= 8;
    if (state.gprs[1] & 0xf)
        state.gprs[1] -= 8;

    // Should be unreachable.
    // The args/env variables we push are all u64s, if we are still not aligned to 16 byte boundary after subtracting 8 it means something went wrong.
    if (state.gprs[1] & 0xf)
        Helpers::panic("Bad stack alignment after arg and env setup: 0x%08x\n", state.gprs[1]);

    printf("sp for thread \"%s\": 0x%08x\n", name.c_str(), (u32)state.gprs[1]);
}

// We put the reschedule on the scheduler instead of having it happen instantly because it would break
// if a reschedule is called in the middle of an HLE function
void Thread::reschedule(u64 cycles) {
    mgr->ps3->scheduler.push(std::bind(&ThreadManager::reschedule, mgr), mgr->ps3->curr_block_cycles + cycles, "thread reschedule");
    mgr->ps3->forceSchedulerUpdate();
}

void Thread::sleep(u64 us) {
    const u64 cycles = Scheduler::uSecondsToCycles(us);
    mgr->ps3->scheduler.push(std::bind(&Thread::wakeUp, this), mgr->ps3->curr_block_cycles + cycles, "thread wakeup");
    status = THREAD_STATUS::Sleeping;
    reschedule();
    log("Sleeping thread %d for %d us\n", id, us);
}

void Thread::sleepForCycles(u64 cycles) {
    mgr->ps3->scheduler.push(std::bind(&Thread::wakeUp, this), mgr->ps3->curr_block_cycles + cycles, "thread wakeup");
    status = THREAD_STATUS::Sleeping;
    reschedule();
    log("Sleeping thread %d for %lld cycles\n", id, cycles);
}

void Thread::wait() {
    status = THREAD_STATUS::Waiting;
    reschedule();
    log("Thread %d is waiting\n", id);
}

void Thread::wakeUp() {
    status = THREAD_STATUS::Running;
    reschedule();
    log("Woke up thread %d\n", id);
}

void Thread::exit() {
    status = THREAD_STATUS::Terminated;
    reschedule();
    log("Thread %d exited\n", id);
}