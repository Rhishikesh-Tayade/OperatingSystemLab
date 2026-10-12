#ifndef TRACE_PARSER_HPP
#define TRACE_PARSER_HPP

#include<string>
#include<fstream>
#include<cstdint>

using namespace std;

struct TraceEntry {
    uint32_t address;
    char type;
    char rw;

    uint32_t get_vpn() const { return address >> 12; }
    uint32_t get_offset() const { return address & 0x0FFF; } 
    uint32_t get_l1_index() const { return (address >> 22) & 0X03FF; }
    uint32_t get_l2_index() const { return (address >> 12) & 0x03FF; }
};

class TraceParser {
public:
    explicit TraceParser(const string& filename);
    ~TraceParser();
    
    bool is_open() const;
    bool next(TraceEntry& entry);
    void reset();
    void close();

private:
    string filename;
    ifstream file;
};

#endif