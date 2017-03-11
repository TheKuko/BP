/** 
 *  @file       main.cpp
 *  @brief      Network Traffic Capturing With Application Tags
 *  @details    Bachelor's Thesis, FIT VUT Brno
 *  @author     Jozef Zuzelka (xzuzel00)
 *  Mail:       xzuzel00@stud.fit.vutbr.cz
 *  Created:    18.02.2017 08:03
 *  Edited:     09.03.2017 19:59
 *  Version:    1.0.0
 *  g++:        Apple LLVM version 8.0.0 (clang-800.0.42.1)
 *  @todo       change tool name
 *  @todo       change default tool name in <char *oFilename>
 */

#include <iostream>             //  EXIT_*, cout, cerr
#include <getopt.h>             //  getopt_long()

#if defined(__linux__)
#include <signal.h>             //  signal(), SIGINT, SIGTERM, SIGABRT, SIGSEGV
#endif

#include "capturing.hpp"        //  startCapture()
#include "debug.hpp"
#include "main.hpp"

using namespace std;

LogLevel generalLogLevel = LogLevel::INFO;  // TODO -v,-vv,-vvv parameters
int shouldStop = false;
mutex m_debugPrint;
mutex m_shouldStopVar;
extern const char * g_dev;


static struct option longopts[] = 
{
    { "interface",   required_argument, nullptr,    'i' },
    { "output-file", required_argument, nullptr,    'w' },
    { "help",        no_argument,       nullptr,    'h' },
    { nullptr,       0,                 nullptr,    0 }
};



int main (int argc, char *argv[])
{
/*
#ifdef WIN32
    SetConsoleCtrlHandler((PHANDLER_ROUTINE)signalHandler, TRUE);
#else
*/
    signal(SIGINT, signalHandler);      signal(SIGTERM, signalHandler);
    signal(SIGABRT, signalHandler);     signal(SIGSEGV, signalHandler);
/*
#endif

*/
    char *oFilename = "tool_capturedTraffic.pcapng"; //TODO zmenit tool za nazov nastroja

    int optionIndex = 0;
    char opt = 0;
    while((opt = getopt_long(argc, argv, "i:w:h", longopts, &optionIndex)) != -1)
    {
        switch (opt)
        {
            case 0:                     break;
            case 'i':   g_dev = optarg;   break;
            case 'w':   oFilename = optarg; break;
            case 'h':   printUsage();   return EXIT_SUCCESS;
            default:    printUsage();   return EXIT_FAILURE;
        }
    }

    return startCapture(oFilename);
}


bool stop()
{
    lock_guard<mutex> guard(m_shouldStopVar);
    return shouldStop;
}


void signalHandler(int signum)
{
    cerr << "Interrupt signal (" << signum << ") received.\n";
    lock_guard<mutex> guard(m_shouldStopVar);
    shouldStop = signum;
}


void printUsage()
{
    cout << "Usage: tool [-i <interface>] [-w <output_filename>]" << endl; //TODO zmenit nazov nastroja
    cout << "Note: 'tool_capturedTraffic.pcapng' is used as default filename" << endl;
}