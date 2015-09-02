g++ -Wall -Werror -pedantic -O3 -std=c++11 -DLINUX v1718reset.c -l CAENVME -o v1718reset

g++ -Wall -Werror -pedantic -Ifastjson -pthread -g -std=c++11 -DLINUX *.cc fastjson/*.cc -l hdf5_cpp -l hdf5 -l CAENVME -o WbLSdaq
