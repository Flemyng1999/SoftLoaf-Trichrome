#include "obs_log.hpp"

#include <cstdio>
#include <mutex>
#include <unordered_map>

namespace softloaf::trichrome::desktop {
namespace {

std::string FormatRecord(const LogRecord& rec) {
    std::string line;
    line.reserve(192);
    line += "category=";
    line += rec.category;
    for (const auto& [key, value] : rec.fields) {
        line += ' ';
        line += key;
        line += '=';
        line += value;
    }
    line += '\n';
    return line;
}

struct SinkRegistry {
    std::mutex mu;
    std::unordered_map<SinkHandle, std::function<void(const LogRecord&)>> sinks;
    SinkHandle next = 1;

    void Emit(const LogRecord& rec) {
        const std::string line = FormatRecord(rec);
        std::fputs(line.c_str(), stderr);
        std::fflush(stderr);

        std::vector<std::function<void(const LogRecord&)>> snapshot;
        {
            std::lock_guard<std::mutex> lock(mu);
            snapshot.reserve(sinks.size());
            for (const auto& [_, sink] : sinks) snapshot.push_back(sink);
        }
        for (const auto& sink : snapshot) sink(rec);
    }
};

SinkRegistry& Registry() {
    static SinkRegistry registry;
    return registry;
}

}  // namespace

void ObsLog(std::string_view category, std::initializer_list<LogField> fields) {
    LogRecord rec;
    rec.category = std::string(category);
    rec.fields.reserve(fields.size());
    for (const auto& [key, value] : fields)
        rec.fields.emplace_back(std::string(key), std::string(value));
    Registry().Emit(rec);
}

SinkHandle InstallSink(std::function<void(const LogRecord&)> sink) {
    auto& registry = Registry();
    std::lock_guard<std::mutex> lock(registry.mu);
    const SinkHandle handle = registry.next++;
    registry.sinks[handle] = std::move(sink);
    return handle;
}

void RemoveSink(SinkHandle handle) {
    auto& registry = Registry();
    std::lock_guard<std::mutex> lock(registry.mu);
    registry.sinks.erase(handle);
}

}  // namespace softloaf::trichrome::desktop
