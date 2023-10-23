#include "postgres.h"

#include <pqxx/pqxx>

#include <string>

namespace postgres {

using namespace std::literals;
using pqxx::operator"" _zv;
// RetiredPlayerRepoImpl

RetiredPlayerRepoImpl::RetiredPlayerRepoImpl(pqxx::work& work)
    : work_{work} {
}

void RetiredPlayerRepoImpl::Save(const app::RetiredPlayer& player) {
    work_.exec_params(
        "INSERT INTO retired_players (id, name, score, play_time_ms) "
        "VALUES ($1, $2, $3, $4) "_zv,
        player.GetId().ToString(), player.GetName(), player.GetScore(), player.PlayTime());
}

std::vector<app::RetiredPlayer> RetiredPlayerRepoImpl::GetSavedRetiredPlayers(int offset, int limit) {
    std::vector<app::RetiredPlayer> players;
    std::ostringstream query_text;
    query_text
        << "SELECT id, name, score, play_time_ms FROM retired_players "sv
        << "ORDER BY score DESC, play_time_ms, name LIMIT "sv << limit << " OFFSET "sv << offset << ";"sv;
    // Выполняем запрос и итерируемся по строкам ответа
    for (auto [id, name, score, play_time] : work_.query<std::string, std::string, int, int>(query_text.str())) {
        players.emplace_back(app::RetiredPlayerId::FromString(id), std::move(name), score, play_time);
    }
    return players;
}


// UnitOfWorkImpl::
UnitOfWorkImpl::UnitOfWorkImpl(conn_pool::ConnectionPool::ConnectionWrapper&& connection)
: connection_{std::move(connection)}
, work_{*connection_} {
}

app::RetiredPlayerRepository& UnitOfWorkImpl::PlayerRepository() {
    return player_rep_;
}

void UnitOfWorkImpl::Commit() {
    work_.commit();
}

// UnitOfWorkFactoryImpl::
UnitOfWorkFactoryImpl::UnitOfWorkFactoryImpl(size_t thread_num, const std::string& db_url)
    : conn_pool_{thread_num, [db_url] {return std::make_shared<pqxx::connection>(db_url);}} {
    }

std::unique_ptr<app::UnitOfWork> UnitOfWorkFactoryImpl::CreateUnitOfWork() {
    return std::make_unique<UnitOfWorkImpl>(conn_pool_.GetConnection());
}

//Database
Database::Database(size_t thread_num, const std::string& db_url)
    : unit_factory_{thread_num, db_url} {
    pqxx::connection conn(db_url);
    pqxx::work work(conn);
    work.exec(
R"(CREATE TABLE IF NOT EXISTS retired_players (
    id UUID CONSTRAINT firstindex PRIMARY KEY,
    name varchar(100) NOT NULL,
    score INT NOT NULL,
    play_time_ms INT NOT NULL
);
CREATE UNIQUE INDEX IF NOT EXISTS
    score_play_time_idx
ON
retired_players (score DESC,
play_time_ms,
name);
)"_zv
    );
    work.commit();
    }

} //namespace postgres