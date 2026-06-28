#pragma once

#include <functional>
#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace softloaf::trichrome::desktop {

struct LogRecord {
    std::string category;
    std::vector<std::pair<std::string, std::string>> fields;
};

using SinkHandle = unsigned;
using LogField = std::pair<std::string_view, std::string_view>;

void ObsLog(std::string_view category, std::initializer_list<LogField> fields);
SinkHandle InstallSink(std::function<void(const LogRecord&)> sink);
void RemoveSink(SinkHandle handle);

}  // namespace softloaf::trichrome::desktop
