#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cstring>
#include <map>
#include <sstream>
#include <algorithm>
#include <array>
#include <sstream>
using namespace std;
string CMD_REQUEST_SPOT = "request_spot";
string CMD_ASSIGN_SPOT = "assign_spot";
string CMD_CHECKINGOUT = "checkout";
string CMD_PASS_TIME = "pass_time"; 

map<string, int> Car;
struct Parking {
    string id;
    int size;
    string type;
};

vector<Parking> parkings;
map<int , array<int, 2>> Price;
map<int , array<int, 3>> reserved_Parkings;

void readCars(const string& filename, map<string, int>& Car) {
    ifstream file(filename);
    string line;
    getline(file, line);
    while (getline(file, line)) {
        std::istringstream ss(line);
        string name, size_Str;
        getline(ss, name, ',');
        getline(ss, size_Str, ',');
        Car[name] = stoi(size_Str);
    }
}

void readParkings(const string& filename, vector<Parking>& parkings) {
    ifstream file(filename);
    string line;
    getline(file, line);
    while (getline(file, line)) {
        Parking parking;
        std::istringstream ss(line);
        string size_Str;
        getline(ss, parking.id, ',');
        getline(ss, size_Str, ',');
        parking.size = stoi(size_Str);
        getline(ss, parking.type, ',');
        parkings.push_back(parking);
    }
}

void readPrices(const string& filename, map<int , array<int, 2>>& Price ) {
    ifstream file(filename);
    string line;
    getline(file, line);

    while (getline(file, line)) {
        std::istringstream ss(line);
        string size_Str, static_price_Str, price_per_day_Str;
        getline(ss, size_Str, ',');
        int size_Str_inted = stoi(size_Str);
        getline(ss, static_price_Str, ',');
        int static_price_Str_inted = stoi(static_price_Str);
        getline(ss, price_per_day_Str, ',');
        int price_per_day_Str_inted = stoi(price_per_day_Str);
        Price[size_Str_inted] = {static_price_Str_inted, price_per_day_Str_inted};
    }
    
}

bool compare_By_ID(const Parking& a, const Parking& b){
    return(a.id < b.id);
}

void sort_Parkings_By_ID(vector<Parking>& parkings){
    sort(parkings.begin(), parkings.end(), compare_By_ID);
}
bool check_If_Reserved(int parking_Id){
    auto it3 = reserved_Parkings.find(parking_Id);
    if(it3 != reserved_Parkings.end())
        return true;
    else
        return false;

}

void findParkingPrices(int car_Size, string parking_Type, int& parking_Static_price, int& parking_Price_per_day){
        auto it2 = Price.find(car_Size);
        parking_Static_price = it2->second[0];
        parking_Price_per_day = it2->second[1]; 

        if(parking_Type == "covered"){
            parking_Static_price += 50;
            parking_Price_per_day += 30;
        }

        else if(parking_Type == "CCTV"){
            parking_Static_price += 80;
            parking_Price_per_day += 60;
        }
}


void handleRequesting(string line){
    string car_Name = line.substr(CMD_REQUEST_SPOT.length()+1);
    int car_Size = Car[car_Name];
    int parking_Static_Price = 0;
    int parking_Price_Per_Day = 0;
    for(auto it = parkings.begin(); it != parkings.end(); it++){
        if(it->size == car_Size){
            int parking_Id = stoi(it->id);
            if(check_If_Reserved(parking_Id) == false){
                string parking_Type = it->type;
                findParkingPrices(car_Size, parking_Type, parking_Static_Price, parking_Price_Per_Day);
                cout << parking_Id << " :  " << parking_Type << "  " << parking_Static_Price << "  " << parking_Price_Per_Day << endl;       
            }   
        }
    }    
}

void handleAssigning(string line){
    string parking_Id_string = line.substr(CMD_ASSIGN_SPOT.length()+1);
    int parking_Id = stoi(parking_Id_string);
    int parking_Size;
    string parking_Type;
    for(auto it = parkings.begin(); it != parkings.end(); it++){
        if(stoi(it->id) == parking_Id){
            parking_Type = it->type;
            parking_Size = it->size;
            break;
        }
    }

    int parking_Static_Price = 0;
    int parking_Price_Per_Day = 0;
    int days_Reserved = 0;
    findParkingPrices(parking_Size, parking_Type, parking_Static_Price, parking_Price_Per_Day);
    reserved_Parkings[parking_Id] = {parking_Static_Price, parking_Price_Per_Day, days_Reserved};
    cout<< "spot  "<< parking_Id << " is occupied now." << endl;

}

void handleCheckingout(string line){
    string parking_Id_string = line.substr(CMD_CHECKINGOUT.length()+1);
    int parking_Id = stoi(parking_Id_string);
    int parking_Static_Price;
    int parking_Price_Per_Day;
    int days_Reserved;

    auto it = reserved_Parkings.find(parking_Id);
    int total_Cost = it->second[0] + (it->second[1] * it->second[2]);
    reserved_Parkings.erase(it);
    cout<< "spot " << parking_Id << " is free now." << endl;
    cout<< "Total cost: " << total_Cost << endl;
}

void handlePassingTime(string line){
    if (line.find(CMD_PASS_TIME) != string::npos) {
        size_t pos = line.find(CMD_PASS_TIME) + CMD_PASS_TIME.length();
        string number_of_Days_String = line.substr(pos);
        int number_of_Days = stoi(number_of_Days_String);   
        int parking_Static_Price = 0;
        int parking_Price_Per_Day = 0;
        int days_Reserved = 0;
        int parking_Id = 0;
        for(auto it = reserved_Parkings.begin(); it != reserved_Parkings.end(); it++){
           parking_Id = it->first;
           parking_Static_Price = it->second[0];
           parking_Price_Per_Day = it->second[1];
           days_Reserved = it->second[2] + number_of_Days;
           reserved_Parkings[parking_Id] = {parking_Static_Price, parking_Price_Per_Day, days_Reserved};

        }
    }    
}


int main(int argc, char* argv[]) {
    if (argc != 4) {
        cerr << "You should open 3 files: " << argv[0] << " <cars_file> <parkings_file> <prices_file>\n";
        return 1;
    }

    readCars(argv[1], Car);
    readParkings(argv[2], parkings);
    readPrices(argv[3], Price);
    sort_Parkings_By_ID(parkings);

    string line;
    while(getline(cin, line)){

        if(line.find(CMD_REQUEST_SPOT) == 0)
            handleRequesting(line);
        else if(line.find(CMD_ASSIGN_SPOT) == 0)
            handleAssigning(line);
        else if(line.find(CMD_CHECKINGOUT) == 0)
           handleCheckingout(line); 
        else if(line.find(CMD_PASS_TIME) == 0)
            handlePassingTime(line);
    }

    return 0;
}
    











