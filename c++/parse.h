/* -*- mode: c++ -*- */
#ifndef PARSE_H
#define PARSE_H

#include <vector>
#include <fstream>
#include <unordered_map>

namespace json {
    enum class JSON_Type { BOOLEAN, NUMBER, STRING, OBJECT, ARRAY };

    class JSON_Primitive {
    public:
        virtual ~JSON_Primitive() {}
        virtual JSON_Type get_type() const = 0;
        virtual std::string to_string() const = 0;
    };

    class JSON_Boolean : public JSON_Primitive {
        bool value_;

    public:
        JSON_Boolean(bool value) : value_(value) {}

        JSON_Type get_type() const override { return JSON_Type::BOOLEAN; }

        std::string to_string() const override {
            if (value_) {
                return "true";
            } else {
                return "false";
            }
        }

        bool get_value() const { return value_; }
    };

    class JSON_Number : public JSON_Primitive {
        double value_;

    public:
        JSON_Number(double value) : value_(value) {}

        JSON_Type get_type() const override { return JSON_Type::NUMBER; }

        std::string to_string() const override {
            return std::to_string(value_);
        }
    };

    class JSON_String : public JSON_Primitive {
        std::string value_;

    public:
        JSON_String(std::string value) : value_(value) {}

        JSON_Type get_type() const override { return JSON_Type::STRING; }

        std::string to_string() const override { return "\"" + value_ + "\""; }
    };

    class JSON_Object : public JSON_Primitive {
        bool null_object_ = false;
        std::unordered_map<std::string, JSON_Primitive *> children;

    public:
        explicit JSON_Object(bool nullobj) : null_object_(nullobj) {}

        JSON_Object() = default;

        JSON_Type get_type() const override { return JSON_Type::OBJECT; }

        bool is_null() { return null_object_; }

        void add(const std::string &key, JSON_Primitive *element) {
            children[key] = element;
        }

        std::string to_string() const override {
            if (null_object_) {
                return "null";
            }

            std::string result;
            result.push_back('{');
            for (auto &e : children) {
                result.push_back('"');
                result.insert(result.end(), e.first.begin(), e.first.end());
                result.push_back('"');
                result.push_back(':');
                std::string repr = e.second->to_string();
                result.insert(result.end(), repr.begin(), repr.end());
                result.push_back(',');
            }
            result.push_back('}');
            return result;
        }
    };

    class JSON_Array : public JSON_Primitive {
        std::vector<JSON_Primitive *> elements;

    public:
        JSON_Type get_type() const override { return JSON_Type::ARRAY; }

        std::string to_string() const override {
            std::string result;
            result.push_back('[');
            for (auto &e : elements) {
                std::string repr = e->to_string();
                result.insert(result.end(), repr.begin(), repr.end());
                result.push_back(',');
            }
            result.push_back(']');
            return result;
        }

        void append(JSON_Primitive *element) {
            elements.push_back(element);
        }
    };

    class JSON_File {
        bool ok_ = false;
        JSON_Primitive *root_ = nullptr;

    public:
        JSON_File() = default;

        JSON_File(JSON_File &&another): ok_(another.ok_), root_(another.root_) {
            another.ok_ = false;
            another.root_ = nullptr;
        }

        ~JSON_File() { delete root_; }

        bool ok() const { return ok_; }

        void set_root(JSON_Primitive *root) {
            ok_ = true;
            delete root_;
            root_ = root;
        }

        const JSON_Primitive *get_root() { return root_; }

        JSON_File &operator=(JSON_File &&another) {
            ok_ = another.ok_;
            root_ = another.root_;
            another.ok_ = false;
            another.root_ = nullptr;
            return *this;
        }
    };

    JSON_File parse(std::istream &strm, int max_dept = 64);
} // namespace json

#endif
