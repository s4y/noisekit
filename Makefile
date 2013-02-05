default: noisekit

noisekit: main.cpp
	clang++ -std=c++11 -stdlib=libc++ -framework CoreFoundation -framework AudioToolbox -o noisekit main.cpp
