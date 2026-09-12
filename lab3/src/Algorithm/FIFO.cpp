#include <iostream>
#include <fstream>
#include <string>
#include <unistd.h>
#include <vector>
using namespace std;


vector<vector<int>> Process(const string& filename){
    ifstream file(filename);
    vector<vector<int>> Pro;

    string line;
    while(getline(file, line)){
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
    queue<int> Ready,Blocked;
    push(Ready);
    // i was thinkn ig usign oen queus of the proess that have requested I/O so its a queue named block
    // it will be easy because indecies can be used t o pos and push across the queues,
    // it also redue the compelxi thet we have to do eht only usign one que with two tuple anss need to udpate it contionoulsy
    // maybe ther ia anoute idea thet we push the io requesito process inteh end my there are soem restrictio thet we need to do thet are more difficaul thie way
    // for example if there are two three porcess an tow of the haev consequsiteivky requeo 10 sec io while the fot process wil soo finnish in  4 seconds
    // it will haev t aserap if condio for hte i/o request proces othewise the fifi will run them as well
    // here is how we are goinfoty lead hte proces
    // we will only feth the firs abalu emlemt onto the array and next only if its curren si sompeted
    // for exampe p1{0,10,2,10,-1} p2{0, 5,2,10,-1} p3{10,30,2,10,-1} here the firs valu represen the arrival time teh avey alretenate alu is lie a
    // cpuburstfollowed by io request the cup buts agian ans so on till -1, ie jsut ther to indicate EOF or end of process
    // so the is and indices 't'(time) thet we will follow
    // there is a while loop
    // then there is condito on the loop to see i the ready queue is compleatly empty or not
    // if all ready then exit elsee stay inthe loop
    // (edot condit wriye to all empty)
    // then the is the firs porcess teht will be printed like this
    // for process P1 first instand we call P(process_nuumber,process_instance) becua its process 1 ans its forst instance eg P1,1
    // what eselto prit we willl deal later
    // now we will pus the proces sthet have arrived in ready queue
    // rememebr it before the while loop
    // then the while loop starts
    // then process will start to pop and will run for that interval of time(as for the FIFO non-premptive scheduling)
    // now when a process it runnint it musht haev the neze occuran as an io as of the rule in the assigenmetn
    // <process-arrival-time> <cpu-burst-1-duration> <io-burst-1-duration> <cpu-burst-2-duration> <io-burst-2-duration> … -1
    // so next it have to go to the blocked queue sand the next process inteh ready queue will run
    // it will run dosen matter io is runnin or not
    // if for a long time ther is no porecess and and becu the thers or nect porces arrive at a very long time we shou wait the that porcess
    // we also haev to wrote to the scheduler.txt file
    // next it will pop form the blok ans put it back othte ready if nto terminated
    // and this cycle continues

}
