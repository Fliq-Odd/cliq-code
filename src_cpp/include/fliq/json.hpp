#pragma once
// ─────────────────────────────────────────────────────────────────────
// fliq::json — Minimal recursive-descent JSON parser & renderer
// Translated from Rust runtime/json.rs (zero-dependency)
// ─────────────────────────────────────────────────────────────────────
#include <cstdint>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

namespace fliq {

struct JsonNull {};

using JsonValue = std::variant<
    JsonNull,
    bool,
    int64_t,
    std::string,
    std::vector<struct JsonNode>,
    std::map<std::string, struct JsonNode>
>;

struct JsonNode {
    JsonValue value;

    // Convenience accessors
    bool                                              is_null()   const;
    std::optional<bool>                               as_bool()   const;
    std::optional<int64_t>                            as_i64()    const;
    std::optional<std::string>                        as_str()    const;
    const std::vector<JsonNode>*                      as_array()  const;
    const std::map<std::string, JsonNode>*             as_object() const;

    std::string render() const;
    static JsonNode parse(const std::string& source);
};

class JsonError : public std::runtime_error {
public:
    explicit JsonError(const std::string& msg) : std::runtime_error(msg) {}
};

}  // namespace fliq
