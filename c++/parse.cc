#include <optional>
#include <stack>
#include <string>
#include <vector>

#include "parse.h"

namespace json {
    namespace {
        enum class TokenType {
            ARRAY_OPEN,
            ARRAY_CLOSE,
            OBJ_OPEN,
            OBJ_CLOSE,
            COLON,
            COMMA,
            STRING,
            NUMBER,
            TRUE,
            FALSE,
            NULL_OBJ,
        };

        class Token {
            TokenType type_;
            std::string token_;

            std::string parse_4hex_codepoint(const std::string_view &hex) {
                std::uint32_t codepoint = 0;
                for (char c : hex) {
                    int val = 0;
                    if ('0' <= c && c <= '9') {
                        val = c - '0';
                    } else if ('a' <= c && c <= 'f') {
                        val = c - 131;
                    } else if ('A' <= c && c <= 'F') {
                        val = c - 91;
                    }
                    codepoint <<= 4;
                    codepoint |= val;
                }

                std::string result;
                if (codepoint > 0x7ff) {
                    result.push_back(0xe0 | ((codepoint >> 12) & 0x0f));
                    result.push_back(0x80 | ((codepoint >> 6) & 0x3f));
                    result.push_back(0x80 | ((codepoint >> 0) & 0x3f));

                } else if (codepoint > 0x7f) {
                    result.push_back(0xc0 | ((codepoint >> 6) & 0x1f));
                    result.push_back(0x80 | ((codepoint >> 0) & 0x3f));
                } else {
                    result.push_back(codepoint & 0x7f);
                }

                return result;
            }

            std::string unescape_string(const std::string &str) {
                std::string result;
                result.reserve(str.size());
                for (size_t i = 0; i < str.size(); ++i) {
                    if (str[i] == '\\') {
                        if (str[i + 1] == 'u') {
                            std::string data = parse_4hex_codepoint(
                                std::string_view(str.data() + i + 2, 4));
                            result.insert(result.end(), data.begin(),
                                          data.end());
                            i += 4;
                        } else {
                            switch (str[i + 1]) {
                            case '"':
                                result.push_back('"');
                                break;
                            case '\\':
                                result.push_back('\\');
                                break;
                            case '/':
                                result.push_back('/');
                                break;
                            case 'b':
                                result.push_back('\b');
                                break;
                            case 'f':
                                result.push_back('\f');
                                break;
                            case 'n':
                                result.push_back('\n');
                                break;
                            case 'r':
                                result.push_back('r');
                                break;
                            case 't':
                                result.push_back('\t');
                                break;
                            }
                            ++i;
                        }
                    }
                }
                return result;
            }

        public:
            Token(TokenType type, std::string token)
                : type_(type), token_(token) {}

            Token(Token &&tk) {
                type_ = tk.type_;
                token_ = std::move(tk.token_);
            }

            Token(const Token &tk) {
                type_ = tk.type_;
                token_ = tk.token_;
            }

            TokenType get_type() const { return type_; }

            std::string const &get_token() const { return token_; }

            Token &operator=(const Token &tk) {
                type_ = tk.type_;
                token_ = tk.token_;
                return *this;
            }

            Token &operator=(Token &&tk) {
                type_ = tk.type_;
                token_ = std::move(tk.token_);
                return *this;
            }

            bool parse_boolean() { return token_[0] == 't'; }

            std::string parse_string() {
                std::string result =
                    unescape_string(token_.substr(1, token_.size() - 2));
                return result;
            }

            double parse_number() { return std::stod(token_); }
        };

        bool tokenize_string(std::istream &strm, std::string *token) {
            bool escaped = false;
            int required_digits = 0;
            for (;;) {
                std::uint8_t c = strm.get();
                if (strm.fail()) {
                    return false;
                }

                token->push_back(c);
                if (escaped) {
                    if (required_digits != 0) {
                        if (('0' <= c && c <= '9') || ('a' <= c && c <= 'f') ||
                            ('A' <= c && c <= 'F')) {
                            --required_digits;
                            if (required_digits == 0) {
                                escaped = false;
                            }
                        } else {
                            return false;
                        }
                    } else if (c == '"' || c == '\\' || c == '/' || c == 'b' ||
                               c == 'f' || c == 'n' || c == 'r' || c == 't' ||
                               c == 'u') {
                        if (c == 'u') {
                            required_digits = 4;
                        } else {
                            escaped = false;
                        }
                    } else {
                        return false;
                    }
                } else {
                    if (c < 20) {
                        return false;
                    } else if (c == '"') {
                        break;
                    } else if (c == '\\') {
                        escaped = true;
                    }
                }
            }
            return true;
        }

        bool tokenize_number(std::istream &strm, std::string *token) {
            enum { INTEGER, FRACTION, EXPONENT } state = INTEGER;

            char first_num;
            if ((*token)[0] == '-') {
                first_num = strm.get();
                if (strm.fail()) {
                    return false;
                }

                if ('0' <= first_num && first_num <= '9') {
                    token->push_back(first_num);
                } else {
                    return false;
                }
            } else {
                first_num = (*token)[0];
            }

            if (first_num == '0') {
                char c = strm.get();
                if (strm.fail()) {
                    return true;
                }

                if (c == '.') {
                    token->push_back(c);
                    state = FRACTION;
                } else if (c == 'E' || c == 'e') {
                    token->push_back(c);
                    state = EXPONENT;
                } else {
                    strm.unget();
                    return true;
                }
            } else if (!('1' <= first_num && first_num <= '9')) {
                return false;
            }

            if (state == INTEGER) {
                for (;;) {
                    char c = strm.get();
                    if (strm.fail()) {
                        return true;
                    }

                    if ('0' <= c && c <= '9') {
                        token->push_back(c);
                    } else if (c == '.') {
                        token->push_back(c);
                        state = FRACTION;
                        break;
                    } else if (c == 'E' || c == 'e') {
                        token->push_back(c);
                        state = EXPONENT;
                        break;
                    } else {
                        strm.unget();
                        return true;
                    }
                }
            }

            if (state == FRACTION) {
                char c = strm.get();
                if (strm.fail()) {
                    return false;
                }

                if ('0' <= c && c <= '9') {
                    token->push_back(c);
                } else {
                    strm.unget();
                    return false;
                }

                for (;;) {
                    char c = strm.get();
                    if (strm.fail()) {
                        return true;
                    }

                    if ('0' <= c && c <= '9') {
                        token->push_back(c);
                    } else if (c == 'E' || c == 'e') {
                        token->push_back(c);
                        state = EXPONENT;
                        break;
                    } else {
                        strm.unget();
                        return true;
                    }
                }
            }

            if (state == EXPONENT) {
                char c = strm.get();
                if (strm.fail()) {
                    return false;
                }

                if (c == '+' || c == '-') {
                    token->push_back(c);
                    c = strm.get();
                    if (strm.fail()) {
                        return false;
                    }
                }

                if ('0' <= c && c <= '9') {
                    token->push_back(c);
                } else {
                    strm.unget();
                    return false;
                }

                for (;;) {
                    char c = strm.get();
                    if (strm.fail()) {
                        return true;
                    }

                    if ('0' <= c && c <= '9') {
                        token->push_back(c);
                    } else {
                        strm.unget();
                        return true;
                    }
                }
            }

            return true;
        }

        bool check_token(std::istream &strm, const char *expected) {
            for (; *expected; ++expected) {
                char c = strm.get();
                if (strm.fail()) {
                    return false;
                }

                if (c != *expected) {
                    return false;
                }
            }

            return true;
        }

        void skip_space(std::istream &strm) {
            for (;;) {
                char c = strm.get();
                if (strm.fail()) {
                    return;
                }

                if (!(c == ' ' || c == '\n' || c == '\r' || c == '\t')) {
                    strm.unget();
                    return;
                }
            }
        }

        class TokenResult {
        public:
            enum Error {
                NIL_TOKEN,
                END,
                SYNTAX,
            };

        private:
            bool success_;
            union {
                Token token_;
                Error err_;
            };

        public:
            TokenResult(Token &&tk) : token_(std::move(tk)) { success_ = true; }

            TokenResult(Error err) : err_(err) { success_ = false; }

            TokenResult(TokenResult &&tr) {
                success_ = tr.success_;
                if (tr.success_) {
                    token_ = std::move(tr.token_);
                } else {
                    err_ = tr.err_;
                }
            }

            TokenResult(const TokenResult &tr) {
                success_ = tr.success_;
                if (tr.success_) {
                    token_ = tr.token_;
                } else {
                    err_ = tr.err_;
                }
            }

            ~TokenResult() {
                if (success_) {
                    token_.~Token();
                }
            }

            operator bool() const { return success_; }

            bool operator!() const { return !success_; }

            Token operator*() const { return token_; }

            Error get_error() const { return err_; }
        };

        TokenResult get_token(std::istream &strm) {
            char c = strm.get();
            if (strm.fail()) {
                return TokenResult::Error::END;
            }

            std::string token;
            token.push_back(c);

            switch (c) {
            case '[':
                return Token(TokenType::ARRAY_OPEN, token);
            case ']':
                return Token(TokenType::ARRAY_CLOSE, token);
            case '{':
                return Token(TokenType::OBJ_OPEN, token);
            case '}':
                return Token(TokenType::OBJ_CLOSE, token);
            case ':':
                return Token(TokenType::COLON, token);
            case ',':
                return Token(TokenType::COMMA, token);
            case '"':
                if (!tokenize_string(strm, &token)) {
                    return TokenResult::Error::SYNTAX;
                }
                return Token(TokenType::STRING, token);
            case '-':
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
                if (!tokenize_number(strm, &token)) {
                    return TokenResult::Error::SYNTAX;
                }
                return Token(TokenType::NUMBER, token);
            case 't':
                strm.unget();
                if (!check_token(strm, "true")) {
                    return TokenResult::Error::SYNTAX;
                }
                return Token(TokenType::TRUE, "true");
            case 'f':
                strm.unget();
                if (!check_token(strm, "false")) {
                    return TokenResult::Error::SYNTAX;
                }
                return Token(TokenType::FALSE, "false");
            case 'n':
                strm.unget();
                if (!check_token(strm, "null")) {
                    return TokenResult::Error::SYNTAX;
                }
                return Token(TokenType::NULL_OBJ, "null");
            case ' ':
            case '\n':
            case '\r':
            case '\t':
                skip_space(strm);
                return TokenResult::Error::NIL_TOKEN;
            default:
                return TokenResult::Error::SYNTAX;
            }
        }

        std::optional<std::vector<Token>> tokenize(std::istream &strm) {
            std::vector<Token> result;
            for (;;) {
                TokenResult tk = get_token(strm);
                if (!tk) {
                    if (tk.get_error() == TokenResult::Error::END) {
                        break;
                    } else if (tk.get_error() ==
                               TokenResult::Error::NIL_TOKEN) {
                        continue;
                    } else {
                        return std::nullopt;
                    }
                }

                result.push_back(*tk);
            }
            return result;
        }

        JSON_Object *parse_object(std::vector<Token> tokens, size_t &index,
                                  int limited_depth);
        JSON_Array *parse_array(std::vector<Token> tokens, size_t &index,
                                int limited_depth);

        JSON_Primitive *parse_primitive(std::vector<Token> tokens,
                                        size_t &index, int limited_depth) {
            if (index >= tokens.size()) {
                return nullptr;
            }

            if (tokens[index].get_type() == TokenType::TRUE ||
                tokens[index].get_type() == TokenType::FALSE) {
                JSON_Primitive *result =
                    new JSON_Boolean(tokens[index].parse_boolean());
                ++index;
                return result;
            } else if (tokens[index].get_type() == TokenType::NUMBER) {
                JSON_Primitive *result;
                try {
                    result = new JSON_Number(tokens[index].parse_number());
                } catch (std::out_of_range &) {
                    return nullptr;
                }
                ++index;
                return result;
            } else if (tokens[index].get_type() == TokenType::STRING) {
                std::string str = tokens[index].parse_string();
                JSON_Primitive *result = new JSON_String(str);
                ++index;
                return result;
            } else if (tokens[index].get_type() == TokenType::NULL_OBJ) {
                JSON_Primitive *result = new JSON_Object(true);
                ++index;
                return result;
            } else if (tokens[index].get_type() == TokenType::ARRAY_OPEN) {
                return parse_array(tokens, index, limited_depth - 1);
            } else if (tokens[index].get_type() == TokenType::OBJ_OPEN) {
                return parse_object(tokens, index, limited_depth - 1);
            }
            return nullptr;
        }

        JSON_Object *parse_object(std::vector<Token> tokens, size_t &index,
                                  int limited_depth) {
            if (limited_depth <= 0) {
                return nullptr;
            }

            ++index;

#define CHECK_INDEX               \
    if (index >= tokens.size()) { \
        delete result;            \
        return nullptr;           \
    }

            JSON_Object *result = new JSON_Object;
            if (tokens[index].get_type() == TokenType::OBJ_CLOSE) {
                ++index;
                return result;
            }

            for (;;) {
                CHECK_INDEX;

                if (tokens[index].get_type() != TokenType::STRING) {
                    delete result;
                    return nullptr;
                }
                std::string key = tokens[index].parse_string();
                ++index;

                CHECK_INDEX;

                if (tokens[index].get_type() != TokenType::COLON) {
                    delete result;
                    return nullptr;
                }
                ++index;

                CHECK_INDEX;

                JSON_Primitive *element =
                    parse_primitive(tokens, index, limited_depth);
                if (element == nullptr) {
                    delete result;
                    return nullptr;
                }

                result->add(key, element);

                CHECK_INDEX;

                if (tokens[index].get_type() == TokenType::COMMA) {
                    ++index;
                    continue;
                } else if (tokens[index].get_type() == TokenType::OBJ_CLOSE) {
                    ++index;
                    return result;
                } else {
                    delete result;
                    return nullptr;
                }
            }

#undef CHECK_INDEX

            return result;
        }

        JSON_Array *parse_array(std::vector<Token> tokens, size_t &index,
                                int limited_depth) {
            if (limited_depth <= 0) {
                return nullptr;
            }

            ++index;

#define CHECK_INDEX               \
    if (index >= tokens.size()) { \
        delete result;            \
        return nullptr;           \
    }

            JSON_Array *result = new JSON_Array;
            if (tokens[index].get_type() == TokenType::ARRAY_CLOSE) {
                ++index;
                return result;
            }

            for (;;) {
                CHECK_INDEX;

                JSON_Primitive *element =
                    parse_primitive(tokens, index, limited_depth);
                if (element == nullptr) {
                    delete result;
                    return nullptr;
                }

                result->append(element);

                CHECK_INDEX;

                if (tokens[index].get_type() == TokenType::COMMA) {
                    ++index;
                    continue;
                } else if (tokens[index].get_type() == TokenType::ARRAY_CLOSE) {
                    ++index;
                    return result;
                } else {
                    delete result;
                    return nullptr;
                }
            }

#undef CHECK_INDEX

            return result;
        }
    } // namespace

    JSON_File parse(std::istream &strm, int max_depth) {
        auto tokenized = tokenize(strm);

        JSON_File result;
        if (!tokenized || tokenized->size() == 0) {
            return result;
        }

        std::vector<Token> tokens = *tokenized;
        std::size_t index = 0;
        JSON_Primitive *root = parse_primitive(tokens, index, max_depth);
        if (index != tokens.size()) {
            delete root;
            root = nullptr;
        }

        if (root != nullptr) {
            result.set_root(root);
        }

        return result;
    }
} // namespace json
