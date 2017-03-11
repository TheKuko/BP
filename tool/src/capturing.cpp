/** 
 *  @file       capturing.cpp
 *  @brief      Network Traffic Capturing With Application Tags
 *  @details    Bachelor's Thesis, FIT VUT Brno
 *  @author     Jozef Zuzelka (xzuzel00)
 *  Mail:       xzuzel00@stud.fit.vutbr.cz
 *  Created:    18.02.2017 22:45
 *  Edited:     11.03.2017 06:25
 *  Version:    1.0.0
 *  g++:        Apple LLVM version 8.0.0 (clang-800.0.42.1)
 */

#include <iostream>             //  cerr, endl
#include <fstream>              //  fstream
#include <pcap.h>               //  pcap_lookupdev(), pcap_open_live(), pcap_dispatch(), pcap_close()
#include "netflow.hpp"          //  Netflow
#include "debug.hpp"            //  DEBUG()
#include "fileHandler.hpp"      //  initOFile()
#include "main.hpp"             //  stop()
#include "cache.hpp"            //  
#include "capturing.hpp"

#if defined(__linux__)
#include "tool_linux.hpp"
#include <netinet/if_ether.h>   //  SIZE_ETHERNET, ETHERTYPE_IP, ETHERTYPE_IPV6, ether_header
#include <netinet/ip.h>         //  ip
#include <netinet/ip6.h>        //  ip6_hdr
#include <netinet/tcp.h>        //  tcphdr
#include <netinet/udp.h>        //  udphdr
#endif
#if defined(__FreeBSD__)
#include "tool_bsd.hpp"
//#include <in.h>
//#include <arpa/inet.h>
#endif
#if defined(__APPLE__)
#include "tool_apple.hpp"
#include <netinet/if_ether.h>   //  SIZE_ETHERNET, ETHERTYPE_IP, ETHERTYPE_IPV6, ether_header
#include <netinet/ip.h>         //  ip
#include <netinet/ip6.h>        //  ip6_hdr
#include <netinet/tcp.h>        //  tcphdr
#include <netinet/udp.h>        //  udphdr
#endif
#if defined(WIN32) || defined(WINx64) || (defined(__MSDOS__) || defined(__WIN32__))
#include "tool_win.hpp"
//#include <winsock2.h>
//#include <ws2tcpip.h>
#endif

#define RING_BUFFER_SIZE    1024

using namespace std;


// https://www.iana.org/assignments/ieee-802-numbers/ieee-802-numbers.xhtml
// https://wiki.wireshark.org/Ethernet
const unsigned char     IPV6_SIZE   =    40;
const unsigned char     PROTO_IPV4  =    0x08;
const unsigned char     PROTO_IPV6  =    0x86;
const unsigned char     PROTO_UDP   =    17;
const unsigned char     PROTO_TCP   =    6 ;

const char * g_dev = nullptr;
ofstream oFile;
unsigned long g_droppedPackets;



int startCapture(const char *oFilename)
{
    char errbuf[PCAP_ERRBUF_SIZE];
    pcap_t *handle;
    
    try
    {
        // if the interface wasn't specified by user open the first active one
        if (g_dev == nullptr && (g_dev = pcap_lookupdev(errbuf)) == nullptr)
            throw pcap_ex("Can't open input device.",errbuf);
        
        if ((handle = pcap_open_live(g_dev, BUFSIZ, 0, 1000, errbuf)) == NULL)
            throw pcap_ex("pcap_open_live() failed.",errbuf);
        if (pcap_setnonblock(handle, 1, errbuf) == -1)
            throw pcap_ex("pcap_setnonblock() failed.",errbuf);
        log(LogLevel::INFO, "Capturing device '", g_dev, "' was opened.");

        // Open the output file
        oFile.open(oFilename);
        if (!oFile)
            throw "Can't open output file: '" + string(oFilename) + "'";
        log(LogLevel::INFO, "Output file '", oFilename, "' was opened.");
        
        // Write Section Header Block and Interface Description Block to the file
        initOFile(oFile);

        // Create ring buffer and run writing to file in a new thread
        RingBuffer rb(RING_BUFFER_SIZE);
        thread t1 ( [&rb]() { rb.write(); } );

        //Create cache and periodically refresh it in a new thread;
        //new cache;
        //run new thread  refreshCache();
        
        void *arg_arr[2] = { &rb, /*&cache*/nullptr};

        while (!stop())
            pcap_dispatch(handle, -1, packetHandler, reinterpret_cast<u_char*>(arg_arr));
//        if (pcap_loop(handle, -1, packetHandler, NULL) == -1)
//            throw "pcap_loop() failed";   //pcap_breakloop()?

        this_thread::sleep_for(chrono::seconds(1)); // because of possible deadlock, get some time to return from RingBuffer::receivedPacket() to condVar.wait()
        rb.notifyCondVar(); // notify thread, it should end
        t1.join();
    
        log(LogLevel::INFO, "Dropped '", g_droppedPackets, "' packets.");
        pcap_close(handle);
    }
    catch(pcap_ex &e)
    {
        cerr << "ERROR: " << e.what() << endl;
        if (handle != nullptr) 
            pcap_close(handle);
        return EXIT_FAILURE;
    }
    catch(const char *msg) 
    {
        cerr << "ERROR: " << msg << endl;
        return EXIT_FAILURE;
    }
    return stop();
}


void packetHandler(u_char *arg_array, const struct pcap_pkthdr *header, const u_char *packet)
{    
    static Netflow n(g_dev);
    static unsigned int ip_size;
    static ether_header *eth_hdr;
    void ** arg_arr = reinterpret_cast<void**>(arg_array);
    n.setStartTime(header->ts.tv_usec);

    eth_hdr = (ether_header*) packet;
    RingBuffer *rb = reinterpret_cast<RingBuffer *>(arg_arr[0]);
    if(rb->push(header, packet))
    {
        g_droppedPackets++;
        return; //TODO ok?
    }
    
    // Parse IP header
    if (parseIp(n, ip_size, (void*)(packet + ETHER_HDR_LEN), eth_hdr->ether_type))
        return;

    // Parse transport layer header
    if (parsePorts(n, (void*)(packet + ETHER_HDR_LEN + ip_size)))
        return;

    D("srcPort:" << n.getSrcPort() << ", dstPort:" << n.getDstPort() << ", proto:" << (int)n.getProto());
    //hash5Tuple(n);

    if (/*map.count(hash) == */NULL)
    {
        //netflow *n = new netflow(srcIp,dstIp,srcPort,dstPort,proto);
        //appname = determineApp(n); // predat uzol
        //map[hash] = tuple(appname,inode,n);
        //n.setStartTime(header->ts.tv_usec);
    }
    //map[hash][3]->setEndTime(header->ts.tv_usec);
}


inline int parseIp(Netflow &n, unsigned int &ip_size, void * const ip_hdr, const unsigned short ether_type)
{
    if (ether_type == PROTO_IPV4)
    {
        const ip * const ipv4_hdr = (ip*)ip_hdr;
        ip_size = ipv4_hdr->ip_hl *4; // the length of the internet header in 32 bit words
        if (ip_size < 20)
        {
            log(LogLevel::WARNING, "Incorrect IPv4 header received.");
            return EXIT_FAILURE;
        }
        n.setSrcIp((void*)(&ipv4_hdr->ip_src));
        n.setDstIp((void*)(&ipv4_hdr->ip_dst));
        n.setIpVersion(4);
        n.setProto(ipv4_hdr->ip_p);
    }
    else if (ether_type == PROTO_IPV6)
    {
        const ip6_hdr * const ipv6_hdr = (ip6_hdr*)ip_hdr;
        ip_size = IPV6_SIZE;
        n.setSrcIp((void*)(&ipv6_hdr->ip6_src));
        n.setDstIp((void*)(&ipv6_hdr->ip6_dst));
        n.setIpVersion(6);
        n.setProto(ipv6_hdr->ip6_nxt);
    }
    else    // TODO what to do with 802.3?
        n.setProto(0); // Netflow structure is reused with next packet so we have to delete old value. We don't care about the value, because we ignore everything except 6 (TCP) and 17 (UDP).
    // NOTE: we can't determine app for IGMP, ICMP, etc. https://en.wikipedia.org/wiki/List_of_IP_protocol_numbers
    return EXIT_SUCCESS;
}


inline int parsePorts(Netflow &n, void *hdr)
{
    if (n.getProto() == PROTO_TCP)
    {
        const struct tcphdr *tcp_hdr = (struct tcphdr*)hdr;
        unsigned tcp_size = tcp_hdr->th_off *4; // number of 32 bit words in the TCP header
        if (tcp_size < 20)
        {
            log(LogLevel::WARNING, "Incorrect TCP header received.");
            return EXIT_FAILURE;
        }
        n.setSrcPort(ntohs(tcp_hdr->th_sport));
        n.setDstPort(ntohs(tcp_hdr->th_dport));
    }
    else if (n.getProto() == PROTO_UDP)
    {
        const struct udphdr *udp_hdr = (struct udphdr*)hdr;
        unsigned short udp_size = udp_hdr->uh_ulen; // length in bytes of the UDP header and UDP data
        if (udp_size < 8)
        {
            log(LogLevel::WARNING, "Incorrect UDP packet received.");
            return EXIT_FAILURE;
        }
        n.setSrcPort(ntohs(udp_hdr->uh_sport));
        n.setDstPort(ntohs(udp_hdr->uh_dport));
    }   
    else
        return EXIT_FAILURE;
    return EXIT_SUCCESS;
}