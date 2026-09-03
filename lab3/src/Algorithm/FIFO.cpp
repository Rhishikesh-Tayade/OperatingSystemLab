#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <unistd.h>
#include <vector>
using namespace std;


vector<vector<int>> Process(const string& filename){
    ifstream file(filename);
    vector<vector<int>> Pro;


    string line;
    while(getfile(file, line)){
        stringstream ss(line);
        vector<int> row;
        int x;
        //so basically it does for each 0 100 -1 it does this ans its attribute length is also variable so its good thing
        while (ss >> x){
            row.push_back(x);
        }
        //now this push thet row(thet is a vector 1d) element in  the 2d vector dimention
        if(!row.empty()){
            Pro.push_back(row);
        }
    }

    return Pro;
}




void fifo(string filename){
    
    vector<vector<int>> Pro = Process(filename);
    queue<int>
}
