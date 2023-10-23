#pragma once

#include "player.h"

#include <memory>

namespace app {

class UnitOfWork {
public:
    virtual app::RetiredPlayerRepository& PlayerRepository() = 0;
    virtual void Commit() = 0;
    virtual ~UnitOfWork() = default;
};

class UnitOfWorkFactory {
public:
    virtual std::unique_ptr<UnitOfWork> CreateUnitOfWork() = 0;
protected:
    ~UnitOfWorkFactory() = default;
};

} // namespace app