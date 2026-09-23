#include "request_handler.h"

#include <boost/json.hpp>

namespace http_handler {

namespace json = boost::json;
using namespace std::literals;

namespace {

constexpr std::string_view MAPS_PREFIX = "/api/v1/maps"sv;
constexpr std::string_view API_PREFIX = "/api/"sv;

StringResponse MakeJsonResponse(http::status status, const json::value& body,
                                unsigned http_version, bool keep_alive) {
    StringResponse response{status, http_version};
    response.set(http::field::content_type, ContentType::APPLICATION_JSON);
    response.body() = json::serialize(body);
    response.content_length(response.body().size());
    response.keep_alive(keep_alive);
    return response;
}

json::value MakeError(std::string_view code, std::string_view message) {
    return json::object{{"code", code}, {"message", message}};
}

json::value SerializeRoad(const model::Road& road) {
    const auto start = road.GetStart();
    const auto end = road.GetEnd();
    json::object obj{{"x0", start.x}, {"y0", start.y}};
    if (road.IsHorizontal()) {
        obj["x1"] = end.x;
    } else {
        obj["y1"] = end.y;
    }
    return obj;
}

json::value SerializeBuilding(const model::Building& building) {
    const auto& bounds = building.GetBounds();
    return json::object{{"x", bounds.position.x},
                        {"y", bounds.position.y},
                        {"w", bounds.size.width},
                        {"h", bounds.size.height}};
}

json::value SerializeOffice(const model::Office& office) {
    return json::object{{"id", *office.GetId()},
                        {"x", office.GetPosition().x},
                        {"y", office.GetPosition().y},
                        {"offsetX", office.GetOffset().dx},
                        {"offsetY", office.GetOffset().dy}};
}

json::value SerializeMap(const model::Map& map) {
    json::array roads;
    for (const auto& road : map.GetRoads()) {
        roads.push_back(SerializeRoad(road));
    }
    json::array buildings;
    for (const auto& building : map.GetBuildings()) {
        buildings.push_back(SerializeBuilding(building));
    }
    json::array offices;
    for (const auto& office : map.GetOffices()) {
        offices.push_back(SerializeOffice(office));
    }
    return json::object{{"id", *map.GetId()},
                        {"name", map.GetName()},
                        {"roads", std::move(roads)},
                        {"buildings", std::move(buildings)},
                        {"offices", std::move(offices)}};
}

json::value SerializeMapList(const model::Game& game) {
    json::array maps;
    for (const auto& map : game.GetMaps()) {
        maps.push_back(json::object{{"id", *map.GetId()}, {"name", map.GetName()}});
    }
    return maps;
}

}  // namespace

StringResponse HandleApiRequest(const model::Game& game, std::string_view target,
                                unsigned http_version, bool keep_alive) {
    const auto json_response = [http_version, keep_alive](http::status status,
                                                           const json::value& body) {
        return MakeJsonResponse(status, body, http_version, keep_alive);
    };
    const auto bad_request = [&] {
        return json_response(http::status::bad_request, MakeError("badRequest"sv, "Bad request"sv));
    };

    // Отрезаем завершающий слеш, чтобы /api/v1/maps/ обрабатывался как /api/v1/maps
    if (target.size() > 1 && target.back() == '/') {
        target.remove_suffix(1);
    }

    if (target == MAPS_PREFIX) {
        return json_response(http::status::ok, SerializeMapList(game));
    }

    if (target.starts_with(MAPS_PREFIX) && target.size() > MAPS_PREFIX.size()
        && target[MAPS_PREFIX.size()] == '/') {
        std::string_view map_id = target.substr(MAPS_PREFIX.size() + 1);
        if (map_id.empty() || map_id.find('/') != std::string_view::npos) {
            return bad_request();
        }
        if (const auto* map = game.FindMap(model::Map::Id{std::string{map_id}})) {
            return json_response(http::status::ok, SerializeMap(*map));
        }
        return json_response(http::status::not_found,
                             MakeError("mapNotFound"sv, "Map not found"sv));
    }

    // Любой другой запрос (в т.ч. начинающийся с /api/) считаем некорректным
    return bad_request();
}

}  // namespace http_handler
