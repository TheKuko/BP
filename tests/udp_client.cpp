/**
 *  @file       udp_client.cpp
 *  @brief      Simple UDP client
 *  @author     Jozef Zuzelka <xzuzel00@stud.fit.vutbr.cz>
 *  @date
 *   - Created: 24.04.2017 02:17
 *   - Edited:  12.05.2017 09:26
 */

#include <iostream>         // cout, endl, cerr
#include <chrono>           // duration, duration_cast, milliseconds
#include <string>           // to_string()
#include <signal.h>         // signal(), SIGINT, SIGTERM, SIGABRT, SIGSEGV
#include <thread>           // this_thread::sleep_for()

#if defined(__linux__) || defined(__APPLE__)
#include <arpa/inet.h>      // sockaddr_in, htonl()
#include <unistd.h>         // close()
#include <netdb.h>          // gethostbyname   
#include <cstring>          // memset(), memcpy()
#elif defined(_WIN32)
#include <ws2tcpip.h>
#include <WinSock2.h>
#pragma comment(lib, "Ws2_32.lib")
#endif

#define     BUFFER	        1600    // length of the receiving buffer
#define     DEFAULT_PORT    58900   // default UDP port
#define     NANOSECOND      1000000000

using namespace std;
#define     clock_type      chrono::high_resolution_clock


int shouldStop = 0;         // Variable which is set if program should stop


void printHelp()
{
    cout << "Usage: udp_client <server IP address> [port-number] <pps> <packet-size>" << endl;
    cout << "Default port is " << DEFAULT_PORT << endl;
}


void signalHandler(int signum)
{
	//cerr << "Interrupt signal (" << signum << ") received." << endl;
	shouldStop = signum;
}


int main(int argc, char *argv[])
{
    int fd;                           // an incoming socket descriptor
    unsigned short port = DEFAULT_PORT;
    unsigned long sentPackets = 0;
    unsigned int pps = 0;
    unsigned int XXX = 0;
    unsigned short packetSize = 0;
    
	try
	{
		signal(SIGINT, signalHandler);      signal(SIGTERM, signalHandler);
		signal(SIGABRT, signalHandler);     signal(SIGSEGV, signalHandler);

		if (argc > 6)
			throw "Wrong arguments";
		else if (argc == 2 && strcmp("-h", argv[1]) == 0)
		{
			printHelp();
			return 0;
		}
		else if (argc == 6)
		{
			port = atoi(argv[2]);
			pps = atoi(argv[3]);
			packetSize = atoi(argv[4]);
			XXX = atoi(argv[5]);
		}
		else if (argc == 5)
		{
			port = atoi(argv[2]);
			pps = atoi(argv[3]);
			packetSize = atoi(argv[4]);
		}
		else if (argc == 4)
		{
			pps = atoi(argv[2]);
			packetSize = atoi(argv[3]);
		}
		else
			throw "Wrong arguments";


		if (pps == 0)
			throw "Invalid pps argument";

#ifdef _WIN32
		WSADATA wsa;
		//Initialise winsock
		if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
			throw ("Failed to initialise winsock.");
#endif

		struct hostent *servent;
		// make DNS resolution of the first parameter using gethostbyname()
		if ((servent = gethostbyname(argv[1])) == NULL)
		{
#ifdef _WIN32
			DWORD dwError;
			dwError = WSAGetLastError();
			if (dwError != 0)
			{
				if (dwError == WSAHOST_NOT_FOUND)
					throw ("Host not found\n");
				else if (dwError == WSANO_DATA)
					throw ("No data record found\n");
				else
				{
					cerr << "ERR CODE: " << dwError << endl;
					throw "GetHostByName() failed.";
				}
			}
#endif
			throw "gethostbyname() failed";
		}

		struct sockaddr_in server;        // server's address structure
		memset(&server, 0, sizeof(server)); // erase the server structure
		server.sin_family = AF_INET;
		// copy the first parameter to the server.sin_addr structure
		memcpy(&server.sin_addr, servent->h_addr, servent->h_length);
		server.sin_port = htons(port);        // server port (network byte order)

		if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)   //create a client socket
			throw "Can't open socket";

		socklen_t len = sizeof(server);
		char buffer[BUFFER];

		//long packetsToSend = 1000;
		const unsigned int headersSize = 18 + 20 + 8;
		const unsigned int dataSize = (packetSize <= headersSize) ? 0 : packetSize - headersSize;
		using std::chrono::nanoseconds;
		using std::chrono::duration_cast;

		//clock_type::time_point s = clock_type::now();
		//for(int i=packetsToSend; i; i--) 
		//    sendto(fd, buffer, dataSize, 0, (struct sockaddr *) &server, len);
		//clock_type::time_point e = clock_type::now();
		//clock_type::duration d = e - s;
	//	double sendTime = duration_cast<nanoseconds>(d).count() / packetsToSend;
		//long sleepTime = 0;
		unsigned int maxPps = 1000000000 / (packetSize * 8);
		//unsigned int maxPps = NANOSECOND / sendTime;
			// sleepTime = ((double)(pps * NANOSECOND) / maxPps) / pps;
		if (pps < maxPps)
			;//sleepTime = (NANOSECOND - (sendTime*pps)) / pps;
		else
			cerr << "Too high pps, maximum possible value is <" << maxPps << ">" << endl;

		if (dataSize >= BUFFER)
			throw "Too big packet size";

		clock_type::time_point start = clock_type::now();
		while (!shouldStop)
		{
			sendto(fd, buffer, dataSize, 0, (struct sockaddr *) &server, len);
			sentPackets++;
			//this_thread::sleep_for(nanoseconds(sleepTime));
			for (unsigned int i = 0; i < XXX; i++)
				;
		}
		clock_type::time_point end = clock_type::now();
		clock_type::duration duration = end - start;


		using std::chrono::milliseconds;
		long sentPps = sentPackets / (duration_cast<milliseconds>(duration).count() / 1000.);
		cout << "Packet size   Sent packets   Time [ms]   ~pps   ~Mb/s" << endl;
		cout << packetSize << " " << sentPackets << " " << duration_cast<milliseconds>(duration).count() << " "
			<< sentPps << " " << (sentPps * packetSize) * 8 / 1000000. << endl;
	}
	catch (const char *msg)
	{
		cerr << "ERROR: " << msg << endl;
		perror("Errno: ");
		printHelp();
		return EXIT_FAILURE;
	}

#if defined(__linux__)
	close(fd);
#elif defined(_WIN32)
	closesocket(fd);
	WSACleanup();
#endif
    return EXIT_SUCCESS;
}
