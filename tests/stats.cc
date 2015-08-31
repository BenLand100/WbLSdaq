#include <iostream>
#include "json.hh"

using namespace std;

int main(int argc, char **argv) {
	cout << "sizeof(json::Value) = " << sizeof(json::Value) << "\n";
	cout << "nominal bloat of " << (double)sizeof(json::Value)/(double)sizeof(void*) << "x\n";
}
