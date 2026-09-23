#pragma once
#include "http_server.h"
#include "model.h"

#include <string>
#include <string_view>

namespace http_handler {
namespace beast = boost::beast;
namespace http = beast::http;

using StringResponse = http::response<http::string_body>;

struct ContentType {
    ContentType() = delete;
    constexpr static std::string_view APPLICATION_JSON = "application/json";
};

// Формирует ответ на запрос к REST API.
// target — путь запроса, например "/api/v1/maps/map1"
StringResponse HandleApiRequest(const model::Game& game, std::string_view target,
                                unsigned http_version, bool keep_alive);

class RequestHandler {
public:
    explicit RequestHandler(model::Game& game)
        : game_{game} {
    }

    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;

    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        send(HandleApiRequest(game_, req.target(), req.version(), req.keep_alive()));
    }

private:
    model::Game& game_;
};

}  // namespace http_handler
