#include <iostream>
#include <string>
using namespace std;

// Include all algorithm implementations.
// Each file defines exactly one function with no name collisions.
#include "Algorithm/common.h"
#include "Algorithm/FIFO.cpp"
#include "Algorithm/FIFO2.cpp"
#include "Algorithm/RR.cpp"
#include "Algorithm/RR2.cpp"
#include "Algorithm/MLFQ.cpp"
#include "Algorithm/MLFQ2.cpp"

// ---------------------------------------------------------------------------
// Usage:
//   ./scheduler FIFO   <workload_file>
//   ./scheduler FIFO2  <workload_file>
//   ./scheduler RR     <workload_file> <quantum>
//   ./scheduler RR2    <workload_file> <quantum>
//   ./scheduler MLFQ   <workload_file> [--boost 20]
//   ./scheduler MLFQ2  <workload_file> [--boost 20]
// ---------------------------------------------------------------------------
int main(int argc, char* argv[]) {
    if (argc < 3) {
        cerr << "Usage: " << argv[0]
             << " <algorithm> <workload_file> [options]\n";
        return 1;
    }

    string algo     = argv[1];
    string workload = argv[2];

    if (algo == "FIFO") {
        fifo(workload);

    } else if (algo == "FIFO2") {
        fifo2(workload);

    } else if (algo == "RR") {
        if (argc < 4) { cerr << "RR requires a quantum argument.\n"; return 1; }
        int quantum = stoi(argv[3]);
        rr(workload, quantum);

    } else if (algo == "RR2") {
        if (argc < 4) { cerr << "RR2 requires a quantum argument.\n"; return 1; }
        int quantum = stoi(argv[3]);
        rr2(workload, quantum);

    } else if (algo == "MLFQ") {
        // Optional: --boost <interval>
        int boost = 0;
        for (int i = 3; i < argc - 1; ++i) {
            if (string(argv[i]) == "--boost")
                boost = stoi(argv[i + 1]);
        }
        mlfq(workload, boost);

    } else if (algo == "MLFQ2") {
        int boost = 0;
        for (int i = 3; i < argc - 1; ++i) {
            if (string(argv[i]) == "--boost")
                boost = stoi(argv[i + 1]);
        }
        mlfq2(workload, boost);

    } else {
        cerr << "Unknown algorithm: " << algo << "\n";
        cerr << "Supported: FIFO, FIFO2, RR, RR2, MLFQ, MLFQ2\n";
        return 1;
    }

    return 0;
}
