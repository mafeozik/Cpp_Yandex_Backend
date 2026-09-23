#include "json_loader.h"

#include <boost/json.hpp>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace json_loader {

namespace json = boost::json;
using namespace std::literals;

namespace {

std::string ReadFile(const std::filesystem::path& path) {
    std::ifstream file{path};
    if (!file) {
        throw std::runtime_error("Failed to open file: "s + path.string());
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

int GetInt(const json::object& obj, std::string_view key) {
    return static_cast<int>(obj.at(key).as_int64());
}

model::Road ParseRoad(const json::object& obj) {
    const model::Point start{GetInt(obj, "x0"sv), GetInt(obj, "y0"sv)};
    if (obj.contains("x1"sv)) {
        return model::Road{model::Road::HORIZONTAL, start, GetInt(obj, "x1"sv)};
    }
    return model::Road{model::Road::VERTICAL, start, GetInt(obj, "y1"sv)};
}

model::Building ParseBuilding(const json::object& obj) {
    return model::Building{model::Rectangle{
        model::Point{GetInt(obj, "x"sv), GetInt(obj, "y"sv)},
        model::Size{GetInt(obj, "w"sv), GetInt(obj, "h"sv)}}};
}

model::Office ParseOffice(const json::object& obj) {
    return model::Office{model::Office::Id{json::value_to<std::string>(obj.at("id"sv))},
                         model::Point{GetInt(obj, "x"sv), GetInt(obj, "y"sv)},
                         model::Offset{GetInt(obj, "offsetX"sv), GetInt(obj, "offsetY"sv)}};
}

model::Map ParseMap(const json::object& obj) {
    model::Map map{model::Map::Id{json::value_to<std::string>(obj.at("id"sv))},
                   json::value_to<std::string>(obj.at("name"sv))};

    for (const auto& road : obj.at("roads"sv).as_array()) {
        map.AddRoad(ParseRoad(road.as_object()));
    }
    if (const auto* buildings = obj.if_contains("buildings"sv)) {
        for (const auto& building : buildings->as_array()) {
            map.AddBuilding(ParseBuilding(building.as_object()));
        }
    }
    if (const auto* offices = obj.if_contains("offices"sv)) {
        for (const auto& office : offices->as_array()) {
            map.AddOffice(ParseOffice(office.as_object()));
        }
    }
    return map;
}

}  // namespace

model::Game LoadGame(const std::filesystem::path& json_path) {
    const json::value config = json::parse(ReadFile(json_path));

    model::Game game;
    for (const auto& map : config.as_object().at("maps"sv).as_array()) {
        game.AddMap(ParseMap(map.as_object()));
    }
    return game;
}

}  // namespace json_loader
