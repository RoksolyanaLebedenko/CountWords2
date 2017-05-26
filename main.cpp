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

deque<vector<string>> allwords;
condition_variable condV;
atomic <bool> res = {false};
mutex mutex1;

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

void producer(const string& file) {
    string line;
    int block_size = 30;
    cout << "Enter size of block:";
    cin >> block_size;
    map<string, int> myMap;
    auto open_start_time = get_current_time_fenced();
    ifstream myfile(file);

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
        cout << "Unable to open file" << file << endl;
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
void consumer(map<string, int>& myMap){
    while(true){
        unique_lock<mutex> lock1(mutex1);
        if (!allwords.empty()) {
            vector<string> vector1 = allwords.front();
            allwords.pop_front();
            lock1.unlock();
            string word;
            for(int i = 0; i < vector1.size(); i++) {
                istringstream iss(vector1[i]);
                while(iss >> word){
                    unique_lock<mutex> lock2(mutex1);
                    ++myMap[word];
                }
            }
        }
        else {
            if(res)
                break;
            else
                condV.wait(lock1);
        }
    }
}

int main()
{

    map<string, int> myMap;
    string file;
    cout << "Please enter a path to file with words: ";
    cin >> file;
    //file = "/home/roksoliana/file.txt";
    auto total_start_time = get_current_time_fenced();
    thread threads[2];
    threads[0] = thread(producer, file);
    threads[1] = thread(consumer, ref(myMap));

    for(int i = 0; i < 2; i++){
        threads[i].join();
    }
    //------------Write to file by word------------------
    string wrFile1;
    cout << "Please enter a path to file, where words will be sorted alphabetically: ";
    cin >> wrFile1;
    //wrFile1 = "/home/roksoliana/fileout_by_word.txt";
    ofstream outmyfile;
    outmyfile.open(wrFile1);
    if (outmyfile.is_open()){
        for (auto it = myMap.begin(); it != myMap.end(); ++it)
        {
            outmyfile << (*it).first << " : " << (*it).second << "\n";
        }
        outmyfile.close();}
    else{
        cout << "Enable to open a file" << wrFile1 << endl;
        return 0;
    }

    //===================================================

    //-------------Write to file by numbers----------------

    vector<pair<string, int> > mapcopy(myMap.begin(), myMap.end());
    sort(mapcopy.begin(), mapcopy.end(), less_second<string, int>());
    string wrFile2;
    cout << "Please enter a path to file, where words will be sorted by number of each word: ";
    cin >> wrFile2;
    //wrFile2 = "/home/roksoliana/fileout_by_number.txt";
    ofstream outmyfile2;
    outmyfile2.open (wrFile2);
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
        cout << "Enable to open a file" << wrFile2 << endl;
        return 0;
    }
    return 0;

}


