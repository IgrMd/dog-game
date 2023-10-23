#pragma once

#include "../app/unit_of_work.h"
#include "../app/player.h"
#include "connection_pool.h"

namespace postgres {

class RetiredPlayerRepoImpl : public app::RetiredPlayerRepository {
public:
    explicit RetiredPlayerRepoImpl(pqxx::work& work);

    void Save(const app::RetiredPlayer& player) override;

    std::vector<app::RetiredPlayer> GetSavedRetiredPlayers(int offset, int count) override;

private:
    pqxx::work& work_;
};

class UnitOfWorkImpl : public app::UnitOfWork{
public:
    explicit UnitOfWorkImpl(conn_pool::ConnectionPool::ConnectionWrapper&& connection);

    app::RetiredPlayerRepository& PlayerRepository() override;

    void Commit() override;

private:
    conn_pool::ConnectionPool::ConnectionWrapper&& connection_;
    pqxx::work work_;
    RetiredPlayerRepoImpl player_rep_{work_};
};


class UnitOfWorkFactoryImpl : public app::UnitOfWorkFactory {
public:
    explicit UnitOfWorkFactoryImpl(size_t thread_num, const std::string& db_url);

    std::unique_ptr<app::UnitOfWork> CreateUnitOfWork() override;
private:
    conn_pool::ConnectionPool conn_pool_;
};

class Database {
public:
    explicit Database(size_t thread_num, const std::string& db_url);

    UnitOfWorkFactoryImpl& GetUnitOfWorkFactory() {
        return unit_factory_;
    }

private:
    UnitOfWorkFactoryImpl unit_factory_;
};

} // namespace postgres