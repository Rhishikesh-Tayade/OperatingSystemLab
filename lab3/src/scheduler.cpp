#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <unistd.h>
#include "Algorithm/FIFO.cpp"

using namespace std;

int main(int argc,char* argv[]){
    if (string(argv[1])=="FIFO") {
        fifo(argv[2]);
    }
    // else if (string(argv[1])=="RR") {
    //     rr(argv[2]);
    // }
}
