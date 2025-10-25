#include "json.h"

namespace retort
{
std::string json_escape(const std::string &value) {
    std::string escaped;
    escaped.reserve(value.size());
    for (char ch : value) {
        switch (ch) {
        case '\\':
            escaped.append("\\\\");
            break;
        case '\"':
            escaped.append("\\\"");
            break;
        case '\n':
            escaped.append("\\n");
            break;
        case '\r':
            escaped.append("\\r");
            break;
        case '\t':
            escaped.append("\\t");
            break;
        default:
            if (static_cast<unsigned char>(ch) < 0x20U) {
                escaped.push_back(' ');
            }
            else {
                escaped.push_back(ch);
            }
            break;
        }
    }
    return escaped;
}
}
