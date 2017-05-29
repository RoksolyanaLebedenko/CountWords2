#include <map>
#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <thread>
#include <mutex>
#include <algorithm>
#include <queue>
#include <condition_variable>
#include <atomic>
#include <sstream>

using namespace std;


//realization of clock
inline chrono::high_resolution_clock::time_point get_current_time_fenced()
{
    atomic_thread_fence(memory_order_seq_cst);
    auto res_time = chrono::high_resolution_clock::now();
    atomic_thread_fence(memory_order_seq_cst);
    return res_time;
}
template<class D>
inline long long to_us(const D& d)
{
    return chrono::duration_cast<chrono::microseconds>(d).count();
}


//----For sorting-----
template <typename T1, typename T2>
struct less_second {
    typedef pair<T1, T2> type;
    bool operator ()(type const& a, type const& b) const {
        return a.second < b.second;
    }
};

void producer(ifstream& myfile, int& block_size,mutex& mutex1,
              deque<vector<string>>& allwords,condition_variable& condV,
              atomic<bool>& res) {

    string line;

    auto open_start_time = get_current_time_fenced();


    vector<string> block = {};

    if (myfile.is_open()){
        int i = 0;
        while (getline(myfile,line) && i < 100){
            block.push_back(line);
            ++i;
            if(i == block_size){
                i = 0;
                {
                    lock_guard<mutex> lBlock(mutex1);
                    allwords.push_back(block);
                }
                condV.notify_one();
                block = {};
            }

        }
        myfile.close();
    }

    else{
        cout << "Unable to open file with words" << endl;
        return;
    }
    if (block.size() != 0){
        {
            lock_guard<mutex> lBlock(mutex1);
            allwords.push_back(block);
        }
        condV.notify_one();
    }
    condV.notify_all();
    res = true;
    auto open_end_time = get_current_time_fenced();
    cout << "\nOpening time: "<< to_us(open_end_time-open_start_time) / (double)(1000) << " ms\n" << endl;
    return;

}
void consumer(deque<map<string,int>>& que_join,mutex& mutex1,mutex& mutex2,deque<vector<string>>& allwords,
              condition_variable& condV,atomic<bool>& res,condition_variable& condV_join,atomic<bool>& res_join){
    while(true){
        unique_lock<mutex> lock1(mutex1);
        if (!allwords.empty()) {
            vector<string> vector1 = allwords.front();
            allwords.pop_front();
            lock1.unlock();
            string word;
            for(int i = 0; i < vector1.size(); i++) {
                istringstream iss(vector1[i]);
                map<string,int> w;
                while(iss >> word){
                    lock_guard<mutex> lock2(mutex2);
                    ++w[word];
                }
                que_join.push_back(w);
                if(que_join.size()>1){
                    condV_join.notify_one();
                }

            }
        }
        else {
            if(res){
                res_join = true;
                condV_join.notify_all();
                break;}
            else{
                condV.wait(lock1);}
        }
    }
}

void joinWords(deque<map<string,int>>& que_join,mutex& mutex2,
               condition_variable& condV_join,atomic<bool>& res_join){
    while(true){
        unique_lock<mutex> lock2(mutex2);
        if(que_join.size() > 1){
            map<string,int> first = que_join.front();
            que_join.pop_front();
            map<string,int> second= que_join.front();
            que_join.pop_front();
            lock2.unlock();
            for(const auto& i:first){
                second[i.first] += i.second;
            }

            unique_lock<mutex> lock2(mutex2);
            que_join.push_back(second);
            lock2.unlock();

        }
        else{
            if(res_join){
                break;
            }
            else {
                condV_join.wait(lock2);}
        }
    }

}

map<string, string> configm(string filename) {
    string line;
    ifstream myfile;
    map<string, string> cmap;
    myfile.open(filename);

    if (myfile.is_open())
    {
        while (getline(myfile,line))
        {
            int pos = line.find("=");
            string key = line.substr(0, pos);
            string value = line.substr(pos + 1);
            cmap[key] = value;
        }

        myfile.close();
    }
    else {
        cout << "Error with opening the file!" << endl;
    }
    return cmap;

}

int main()
{

    map<string, int> myMap;
    string file;
      cout << "Please enter a path to configuration file: ";
      cin >> file;
    //file = "/home/roksoliana/config.txt";
    map<string, string> cmap = configm(file);
    string filewithwords = cmap["filewithwords"];
    string writeByWords = cmap["writeByWords"];
    string writeByNumber = cmap["writeByNumber"];
    int numOfthreads = stoi(cmap["numOfthreads"]);
    int block_size = stoi(cmap["blocksize"]);

    mutex mutex1;
    mutex mutex2;
    deque<vector<string>> allwords;
    deque<map<string,int>> que_join;
    condition_variable condV;
    atomic <bool> res = {false};
    condition_variable condV_join;
    atomic <bool> res_join = {false};

    ifstream myfile(filewithwords);

    auto total_start_time = get_current_time_fenced();
    thread cons_threads[numOfthreads];
    thread join_threads[numOfthreads];
    thread prod_thread = thread(producer, ref(myfile), ref(block_size),ref(mutex1),ref(allwords),ref(condV),ref(res));

    for(int i = 0; i < numOfthreads;i++){
        cons_threads[i] = thread(consumer, ref(que_join),ref(mutex1),ref(mutex2),ref(allwords),ref(condV),
                                 ref(res),ref(condV_join),ref(res_join));
    }
    for(int i = 0; i < numOfthreads;i++){
        join_threads[i] = thread(joinWords, ref(que_join),ref(mutex2),ref(condV_join),ref(res_join));
    }


    prod_thread.join();
    for(int i = 0; i < numOfthreads;i++){
        cons_threads[i].join();
    }
    for(int i = 0; i < numOfthreads;i++){
        join_threads[i].join();
    }
    myMap = que_join.front();

    //------------Write to file by word------------------
    ofstream outmyfile;
    outmyfile.open(writeByWords);
    if (outmyfile.is_open()){
    for (auto it = myMap.begin(); it != myMap.end(); ++it)
    {
      outmyfile << (*it).first << " : " << (*it).second << "\n";
    }
      outmyfile.close();}
    else{
        cout << "Enable to open a file" << writeByWords << endl;
        return 0;
    }

    //===================================================

    //-------------Write to file by numbers----------------

    vector<pair<string, int> > mapcopy(myMap.begin(), myMap.end());
    sort(mapcopy.begin(), mapcopy.end(), less_second<string, int>());
    ofstream outmyfile2;
    outmyfile2.open (writeByNumber);
    if (outmyfile2.is_open()){
        for (auto it = mapcopy.begin(); it != mapcopy.end(); ++it)
        {
            outmyfile2 << (*it).first << " : " << (*it).second << "\n";
        }
        outmyfile2.close();
        auto total_end_time = get_current_time_fenced();
        cout << "Total time: "<< to_us(total_end_time-total_start_time) / (double)(1000) << " ms\n" << endl;
    }
    else{
        cout << "Enable to open a file" << writeByNumber << endl;
        return 0;
    }
    return 0;

}


