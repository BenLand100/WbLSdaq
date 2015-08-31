g++ -g -std=c++11 -DLINUX v1718reset.c -l CAENVME -o v1718reset

g++ -Ijson -pthread -O3 -std=c++11 -DLINUX v1730_dpppsd.cc json/json.cc -l hdf5_cpp -l hdf5 -l CAENVME -o WbLSdaq
