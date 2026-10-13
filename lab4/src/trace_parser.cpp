#include"trace_parser.hpp"
#include<iostream>
#include<cctype>
using namespace std;

static bool parse_address(const string& token, uint32_t& out) {
    size_t i = 0;
    while (i < token.size() && isspace(static_cast<unsigned char>(token[i]))) ++i;
    if (i == token.size()) return false;

    int base = 10;
    if (token.size() - i > 2 && token[i] == '0' &&
        (token[i + 1] == 'x' || token[i + 1] == 'X')) {
        base = 16;
        i += 2;
    }
    if (i == token.size()) return false;

    uint64_t value = 0;
    for (; i < token.size(); ++i) {
        const char c = token[i];
        int digit;
        if (c >= '0' && c <= '9')                  digit = c - '0';
        else if (base == 16 && c >= 'a' && c <= 'f') digit = c - 'a' + 10;
        else if (base == 16 && c >= 'A' && c <= 'F') digit = c - 'A' + 10;
        else return false;
        value = value * static_cast<uint64_t>(base) + static_cast<uint64_t>(digit);
    }

    out = static_cast<uint32_t>(value & 0xFFFFFFFFULL);
    return true;
}

TraceParser::TraceParser(const string& filename) : filename(filename) {
    file.open(filename);   
}

TraceParser::~TraceParser() {
    close();
}

bool TraceParser::is_open() const {
    return file.is_open();
}

bool TraceParser::next(TraceEntry& entry) {
    string line;
    while(getline(file, line)) {
        if(line.empty() || line[0] == '#' || line[0] == '<') continue;
        size_t first_comma = line.find(',');
        if(first_comma == string::npos) continue;
        size_t second_comma = line.find(',', first_comma+1);
        if(second_comma == string::npos) continue;

        uint32_t addr = 0;
        if(!parse_address(line.substr(0, first_comma), addr)) continue;
        entry.address = addr;
        entry.type = line[first_comma+1];
        entry.rw = line[second_comma+1];
        return true;
    }
    return false;
}

void TraceParser::reset() {
    file.clear();
    file.seekg(0);
}

void TraceParser::close() {
    if(file.is_open()) file.close();
}