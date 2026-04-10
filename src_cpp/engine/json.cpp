// ─────────────────────────────────────────────────────────────────────
// fliq::json — Zero-dependency recursive-descent JSON parser & renderer
// Translated from Rust crates/runtime/src/json.rs (359 LOC)
// ─────────────────────────────────────────────────────────────────────

#include "fliq/json.hpp"

#include <algorithm>
#include <cstdint>
#include <sstream>

namespace fliq {

// ── Rendering helpers ────────────────────────────────────────────────

static void push_unicode_escape(std::string& out, char32_t ch) {
    static const char HEX[] = "0123456789abcdef";
    out += "\\u";
    out += HEX[(ch >> 12) & 0xF];
    out += HEX[(ch >> 8)  & 0xF];
    out += HEX[(ch >> 4)  & 0xF];
    out += HEX[ch & 0xF];
}

static std::string render_string(const std::string& value) {
    std::string r;
    r.reserve(value.size() + 2);
    r += '"';
    for (unsigned char ch : value) {
        switch (ch) {
            case '"':  r += "\\\""; break;
            case '\\': r += "\\\\"; break;
            case '\n': r += "\\n";  break;
            case '\r': r += "\\r";  break;
            case '\t': r += "\\t";  break;
            case '\b': r += "\\b";  break;
            case '\f': r += "\\f";  break;
            default:
                if (ch < 0x20) push_unicode_escape(r, ch);
                else r += static_cast<char>(ch);
        }
    }
    r += '"';
    return r;
}

// ── JsonNode accessors ───────────────────────────────────────────────

bool JsonNode::is_null() const {
    return std::holds_alternative<JsonNull>(value);
}

std::optional<bool> JsonNode::as_bool() const {
    if (auto* v = std::get_if<bool>(&value)) return *v;
    return std::nullopt;
}

std::optional<int64_t> JsonNode::as_i64() const {
    if (auto* v = std::get_if<int64_t>(&value)) return *v;
    return std::nullopt;
}

std::optional<std::string> JsonNode::as_str() const {
    if (auto* v = std::get_if<std::string>(&value)) return *v;
    return std::nullopt;
}

const std::vector<JsonNode>* JsonNode::as_array() const {
    return std::get_if<std::vector<JsonNode>>(&value);
}

const std::map<std::string, JsonNode>* JsonNode::as_object() const {
    return std::get_if<std::map<std::string, JsonNode>>(&value);
}

// ── Render ───────────────────────────────────────────────────────────

std::string JsonNode::render() const {
    if (is_null()) return "null";
    if (auto b = as_bool()) return *b ? "true" : "false";
    if (auto n = as_i64()) return std::to_string(*n);
    if (auto s = as_str()) return render_string(*s);
    if (auto* arr = as_array()) {
        std::string r = "[";
        for (size_t i = 0; i < arr->size(); ++i) {
            if (i > 0) r += ",";
            r += (*arr)[i].render();
        }
        r += "]";
        return r;
    }
    if (auto* obj = as_object()) {
        std::string r = "{";
        bool first = true;
        for (auto& [k, v] : *obj) {
            if (!first) r += ",";
            first = false;
            r += render_string(k) + ":" + v.render();
        }
        r += "}";
        return r;
    }
    return "null";
}

// ── Parser ───────────────────────────────────────────────────────────

class Parser {
public:
    explicit Parser(const std::string& source) : source_(source), index_(0) {}

    JsonNode parse_value() {
        skip_ws();
        auto ch = peek();
        if (!ch) throw JsonError("unexpected end of input");
        switch (*ch) {
            case 'n': return parse_literal("null", JsonNode{JsonNull{}});
            case 't': return parse_literal("true", JsonNode{true});
            case 'f': return parse_literal("false", JsonNode{false});
            case '"': return JsonNode{parse_string()};
            case '[': return parse_array();
            case '{': return parse_object();
            default:
                if (*ch == '-' || (*ch >= '0' && *ch <= '9'))
                    return JsonNode{parse_number()};
                throw JsonError(std::string("unexpected character: ") + *ch);
        }
    }

    void skip_ws() {
        while (index_ < source_.size() &&
               (source_[index_] == ' ' || source_[index_] == '\n' ||
                source_[index_] == '\r' || source_[index_] == '\t'))
            ++index_;
    }

    bool is_eof() const { return index_ >= source_.size(); }

private:
    std::optional<char> peek() const {
        return index_ < source_.size() ? std::optional<char>(source_[index_]) : std::nullopt;
    }

    std::optional<char> next() {
        if (index_ >= source_.size()) return std::nullopt;
        return source_[index_++];
    }

    void expect(char expected) {
        auto ch = next();
        if (!ch) throw JsonError(std::string("expected '") + expected + "', found end of input");
        if (*ch != expected) throw JsonError(std::string("expected '") + expected + "', found '" + *ch + "'");
    }

    bool try_consume(char expected) {
        if (peek() == expected) { ++index_; return true; }
        return false;
    }

    JsonNode parse_literal(const char* expected, JsonNode value) {
        for (const char* p = expected; *p; ++p) {
            auto ch = next();
            if (!ch || *ch != *p)
                throw JsonError(std::string("invalid literal: expected ") + expected);
        }
        return value;
    }

    std::string parse_string() {
        expect('"');
        std::string result;
        while (true) {
            auto ch = next();
            if (!ch) throw JsonError("unterminated string");
            if (*ch == '"') return result;
            if (*ch == '\\') { result += parse_escape(); continue; }
            result += *ch;
        }
    }

    char parse_escape() {
        auto ch = next();
        if (!ch) throw JsonError("unexpected end of input in escape");
        switch (*ch) {
            case '"': return '"';
            case '\\': return '\\';
            case '/': return '/';
            case 'b': return '\b';
            case 'f': return '\f';
            case 'n': return '\n';
            case 'r': return '\r';
            case 't': return '\t';
            case 'u': return parse_unicode_escape();
            default: throw JsonError(std::string("invalid escape: ") + *ch);
        }
    }

    char parse_unicode_escape() {
        uint32_t val = 0;
        for (int i = 0; i < 4; ++i) {
            auto ch = next();
            if (!ch) throw JsonError("unexpected end in unicode escape");
            uint32_t digit;
            if (*ch >= '0' && *ch <= '9') digit = *ch - '0';
            else if (*ch >= 'a' && *ch <= 'f') digit = *ch - 'a' + 10;
            else if (*ch >= 'A' && *ch <= 'F') digit = *ch - 'A' + 10;
            else throw JsonError("invalid unicode escape");
            val = (val << 4) | digit;
        }
        if (val > 127) return '?';  // Simplified: ASCII only
        return static_cast<char>(val);
    }

    JsonNode parse_array() {
        expect('[');
        std::vector<JsonNode> items;
        while (true) {
            skip_ws();
            if (try_consume(']')) break;
            items.push_back(parse_value());
            skip_ws();
            if (try_consume(']')) break;
            expect(',');
        }
        return JsonNode{std::move(items)};
    }

    JsonNode parse_object() {
        expect('{');
        std::map<std::string, JsonNode> entries;
        while (true) {
            skip_ws();
            if (try_consume('}')) break;
            auto key = parse_string();
            skip_ws();
            expect(':');
            auto val = parse_value();
            entries[std::move(key)] = std::move(val);
            skip_ws();
            if (try_consume('}')) break;
            expect(',');
        }
        return JsonNode{std::move(entries)};
    }

    int64_t parse_number() {
        std::string buf;
        if (try_consume('-')) buf += '-';
        while (peek() && *peek() >= '0' && *peek() <= '9') {
            buf += *peek(); ++index_;
        }
        if (buf.empty() || buf == "-") throw JsonError("invalid number");
        try { return std::stoll(buf); }
        catch (...) { throw JsonError("number out of range"); }
    }

    const std::string& source_;
    size_t index_;
};

// ── Static parse entry point ─────────────────────────────────────────

JsonNode JsonNode::parse(const std::string& source) {
    Parser p(source);
    auto value = p.parse_value();
    p.skip_ws();
    if (!p.is_eof()) throw JsonError("unexpected trailing content");
    return value;
}

}  // namespace fliq
