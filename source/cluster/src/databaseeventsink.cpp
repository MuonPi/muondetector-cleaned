#include "databaseeventsink.h"

#include "event.h"
#include "databaselink.h"
#include "utility.h"
#include "log.h"

#include <sstream>

namespace MuonPi {

DatabaseEventSink::DatabaseEventSink(DatabaseLink& link)
    : m_link { link }
{
    start();
}

auto DatabaseEventSink::step() -> int
{
    if (has_items()) {
        process(next_item());
    }
    std::this_thread::sleep_for(std::chrono::microseconds{50});
    return 0;
}

void DatabaseEventSink::process(Event event)
{
    if (event.n() == 1) {
        // by default, don't write the single events to the db
        return;
    }

    const std::int64_t cluster_coinc_time = event.end() - event.start();
    GUID guid{event.hash(), static_cast<std::uint64_t>(event.start())};
    for (auto& evt: event.events()) {
        DbEntry entry { "L1Event" };
        // timestamp
        entry.timestamp()=std::to_string(evt.start());
        // tags
        entry.tags().push_back(std::make_pair("user", evt.data().user));
        entry.tags().push_back(std::make_pair("detector", evt.data().station_id));
        entry.tags().push_back(std::make_pair("site_id", evt.data().user+evt.data().station_id));
        // fields
        entry.fields().push_back(std::make_pair("accuracy", static_cast<int>(evt.data().time_acc)));

        entry.fields().push_back(std::make_pair("uuid", guid.to_string()));

        entry.fields().push_back(std::make_pair("coinc_level", static_cast<unsigned short>(event.n())));
        entry.fields().push_back(std::make_pair("counter", static_cast<unsigned short>(evt.data().ublox_counter)));
        entry.fields().push_back(std::make_pair("length", static_cast<long long>(evt.duration())));

        const std::int_fast64_t evt_coinc_time = (evt.start() - event.start());

        entry.fields().push_back(std::make_pair("coinc_time", static_cast<long long>(evt_coinc_time)));
        entry.fields().push_back(std::make_pair("cluster_coinc_time", static_cast<long long>(cluster_coinc_time)));
        entry.fields().push_back(std::make_pair("time_ref", static_cast<short>(evt.data().gnss_time_grid)));
        entry.fields().push_back(std::make_pair("valid_fix", static_cast<short>(evt.data().fix)));

        if (!m_link.write_entry(entry)) {
            Log::error()<<"Could not write event to database.";
            return;
        }
    }
}


}
