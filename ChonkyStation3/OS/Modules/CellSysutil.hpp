#pragma once

#include <common.hpp>
#include <logger.hpp>
#include <BEField.hpp>

#include <CellTypes.hpp>


// Circular dependency
class PlayStation3;

using namespace CellTypes;

class CellSysutil {
public:
    CellSysutil(PlayStation3* ps3) : ps3(ps3) {}
    PlayStation3* ps3;

    u64 cellSysutilRegisterCallback();

private:
    MAKE_LOG_FUNCTION(log, cellSysutil);
};