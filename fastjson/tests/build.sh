#!/bin/bash
g++ -O4 -pedantic -Wall -std=c++11 -I ../ -o echo  ../*.cc echo.cc
g++ -O4 -pedantic -Wall -std=c++11 -I ../ -o tovector  ../*.cc tovector.cc
g++ -O4 -pedantic -Wall -std=c++11 -I ../ -o stats  ../*.cc stats.cc
