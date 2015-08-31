#include <iostream>
#include <fstream>

#include "json.hh"

using namespace std;

int main(int argc, char **argv) {
    
    ifstream file;
    file.open(argv[1]);
    json::Reader reader(file);
    
	json::Writer writer(cout);
    try {
		json::Value value;
		while (reader.getValue(value)) {
			try {
				cout << "casting ";
				writer.putValue(value);
				cout << " to int[]... ";
				vector<int> arr = value.toVector<int>();
				json::Value tmp(arr);
				writer.putValue(tmp);
			} catch(runtime_error e) {
				cout << e.what() << '\n';
			}
			try {
				cout << "casting ";
				writer.putValue(value);
				cout << " to double[]... ";
				vector<double> arr = value.toVector<double>();
				json::Value tmp(arr);
				writer.putValue(tmp);
			} catch(runtime_error e) {
				cout << e.what() << '\n';
			}
			try {
				cout << "casting ";
				writer.putValue(value);
				cout << " to bool[]... ";
				vector<bool> arr = value.toVector<bool>();
				json::Value tmp(arr);
				writer.putValue(tmp);
			} catch(runtime_error e) {
				cout << e.what() << '\n';
			}
		}
	} catch(json::parser_error e) {
		cout << e.what() << '\n';
	}

}
