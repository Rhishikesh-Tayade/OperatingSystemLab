#include"trace_parser.hpp"
#include<iostream>
using namespace std;

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

        try{
            uint64_t raw_addr = stoull(line.substr(0, first_comma), nullptr, 0);
            entry.address = static_cast<uint32_t>(raw_addr & 0xFFFFFFFF);
            entry.type = line[first_comma+1];
            entry.rw = line[second_comma+1];
            return true;
        } catch (...){ continue; }
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
