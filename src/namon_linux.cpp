/**
 *  @file       namon_linux.cpp
 *  @brief      Determining applications and their sockets in Linux
 *  @author     Jozef Zuzelka <xzuzel00@stud.fit.vutbr.cz>
 *  @date
 *   - Created: 18.02.2017 23:32
 *   - Edited:  23.06.2017 12:12
 */

#include <fstream>              //  ifstream
#include <dirent.h>             //  opendir(), readdir()
#include <unistd.h>             //  getpid()
#include <cstring>              //  memset(), strchr()

#include "tcpip_headers.hpp"    //
#include "netflow.hpp"          //  Netflow
#include "cache.hpp"            //  Cache, TEntry
#include "debug.hpp"            //  log()
#include "utils.hpp"            //  pidToInt()
#include "namon_linux.hpp"

using namespace std;

extern const char *g_dev;
extern unsigned int g_notFoundApps;
extern NAMON::mac_addr g_devMac;




namespace NAMON
{


int setDevMac()
{
    string ifname;
    if (strncmp(g_dev, "netmap:", 7) == 0)
    {
        const char* tmpPtr = &g_dev[7];
        // scan for a separator
        for ( ; *tmpPtr && !index("-*^{}/@", *tmpPtr); tmpPtr++)
            ;

        ifname = &g_dev[7];
        ifname.erase(tmpPtr - &g_dev[7]);
    }
    else if (strncmp(g_dev, "pfq:", 4) == 0)
        ifname = &g_dev[4];
    else 
        ifname = g_dev;
 
    // it's 2B type, so >> will read 2 hexa chars, which is 1 normal Byte
    uint16_t twoCharsInByte {0};
    const string macAddrPath = "/sys/class/net/" + ifname + "/address";
    ifstream devMacFile(macAddrPath);
    if (!devMacFile)
        return -1;
    int i{0};
    do {
        if (i >= ETHER_ADDRLEN)
            return -1;

        devMacFile >> hex >> twoCharsInByte;
        g_devMac.bytes[i] = twoCharsInByte;
        i++; 
    } while (devMacFile.get() != '\n');
    return 0;

    //! @todo reimplement to MAC address + multicast
    //! sysctl with the MIB { CTL_NET, PF_ROUTE, 0, AF_LINK, NET_RT_IFLIST, 0 }
    //! SIOCGIFADDR and SIOCGIFHWADDR
    //! http://stackoverflow.com/questions/2283494/get-ip-address-of-an-interface-on-linux/9436692#9436692
}


int getSocketFile(Netflow *n, string &file)
{
    const unsigned int proto = n->getProto();
    const unsigned char ipVer = n->getIpVersion();

    if (proto == PROTO_UDP)
        file = "/proc/net/udp";
    else if (proto == PROTO_UDPLITE)
        file = "/proc/net/udplite";
    else if (proto == PROTO_TCP)
        file = "/proc/net/tcp";
    else
    {
        log(LogLevel::ERR, "Unsupported L4 protocol");
        return -1;
    }
    
    if (ipVer == 6)
        file += '6';
    else if (ipVer != 4)
    {
        log(LogLevel::ERR, "Unsupported IP protocol");
        return -1;
    }
    return 0;
}


int getInode(Netflow *n)
{
    // in6_addr will be always bigger than in_addr so we can use it to store both IPv4 and IPv6
    static ip6_addr foundIp;
    static size_t ipSize;

    const unsigned char ipVer = n->getIpVersion();
    if (ipVer == 4)
       ipSize = IPv4_ADDRLEN;
    else if (ipVer == 6)
       ipSize = IPv6_ADDRLEN;
    else
    { 
        log(LogLevel::ERR, "IP protocol ", ipVer, " is not supported."); 
        return -2; 
    }
    memset(&foundIp, 0, sizeof(foundIp));

    try
    {
        static string filename;

        if (getSocketFile(n, filename))
            return -2;

        ifstream socketsFile;
        socketsFile.open(filename);
        if (!socketsFile)
            throw ("Can't open file " + filename);

        static streamoff pos_localIp, pos_localPort, pos_inode;
        static string dummyStr;
        static int lineLength, inode;
        static uint16_t foundPort;
        static uint16_t wantedPort;

        wantedPort = n->getLocalPort();
        inode = -1;
#if 1
        const char IP_SIZE = (ipVer == 4) ? IPv4_ADDRLEN : IPv6_ADDRLEN;

        getline(socketsFile, dummyStr); // get first line to find out length of the other lines
        lineLength = dummyStr.length() + 1;
        getline(socketsFile, dummyStr, ':'); // get rid of the first column
        pos_localIp = socketsFile.tellg();
        pos_localIp++; // space after first column ("sl")
        pos_localPort = pos_localIp + IP_SIZE*2 + 1; // localIP + ':' delimiter
        // localPort remoteIp:remotePort st tx_queue:rx_queue tr:tm->when retrnsmt
        pos_inode = pos_localPort+3+ 1 +IP_SIZE*2+1+4+ 1+2+1 +8+1+8+ 1 +2+1+8 +1+8+1;
        
        // cycle over remaining lines
        do {
            socketsFile.seekg(pos_localPort); // move before localPort

            socketsFile >> hex >> foundPort;
            //D(wantedPort << " vs. " << foundPort);
            if (foundPort == wantedPort)
            {
                char c{0};
                // compare localIp
                socketsFile.seekg(pos_localIp);
                if (ipVer == 4)
                {
                    char i{0};
                    uint8_t parts[IPv4_ADDRLEN];
                    const unsigned char CHARS_PER_OCTET = 2;

                    while (socketsFile.get(c), c != ':' && c != 0) //! @todo why 0?
                    {
                        if (c >= '0' && c <= '9')
                            c -= '0';
                        else if (c >= 'A' && c <= 'F')
                            c = 10 + c - 'A'; // get from 'A' decimal 10
                        else
                        {
                            log(LogLevel::ERR, "An Unexpected hexadecimal character in IP address in procfs: ", c);
                            break;
                        }
                        // 01 00 00 7F      :c  == 127.0.0.1     (IP address char)
                        // 01 23 45 67      :i                   (position)
                        // 0  1  2  3       :i / CHARS_PER_OCTET (corresponding octet)
                        parts[i / CHARS_PER_OCTET] = parts[i/CHARS_PER_OCTET]*16 + c;
                        i++;
                    }
                    reinterpret_cast<ip4_addr*>(&foundIp)->addr |= (parts[0] << 24) | (parts[1] << 16) | (parts[2] << 8) | parts[3];
                }
                else
                {
                    static uint8_t indexes[16] = {3,2,1,0,7,6,5,4,11,10,9,8,15,14,13,12};
                    static ip6_addr* foundIp6;
                    static uint8_t halfByte;
                    static uint8_t byte;
                    foundIp6 = reinterpret_cast<ip6_addr*>(&foundIp);
                    for (int i=0; i < 16; i++)
                    {
                        socketsFile >> halfByte;
                        if (halfByte >= 'A')
                            byte = (halfByte - 'A' + 10) * 16;
                        else
                            byte = (halfByte - '0') * 16;
                        
                        socketsFile >> halfByte;
                        if (halfByte >= 'A')
                            byte += (halfByte - 'A' + 10);
                        else
                            byte += (halfByte - '0');

                        foundIp6->addr.addr8[indexes[i]] = byte;
                    }
                }

                // static variables are automatically initialized to zero unless there is an initializer
                // ip6_addr is bigger so we can use it to compare for both ip versions
                // if it is our IP address or broadcast
                static char zeroBlock [sizeof(ip6_addr)];
                if (!memcmp(n->getLocalIp(), &foundIp, ipSize) || !memcmp(&foundIp, zeroBlock, ipSize))
                {

                    socketsFile.seekg(pos_inode);
                    // other columns (uid, timeout) have variable width
                    char column = 0;
                    bool inColumn = false;
                    while(column != 3 && socketsFile.good())
                    {
                        //! @todo getc stucked
                        //! @warning can stuck (when file is closed? I don't know yet)
                        // it got the same char all the time (':' when it occured after signal 2 call)
                        // so column is never equal 3
                        socketsFile.get(c);
                        if (c != ' ')
                        {
                            if (!inColumn)
                            {
                                column++;
                                inColumn = true;
                            }
                        }
                        else
                            inColumn = false;
                    }
                    if (column != 3)
                        throw "Inode column not found";
                    socketsFile.unget();

                    socketsFile >> dec >> inode;
                    break;
                }
            }
            //! @todo for sure lines have same length?
            pos_localIp += lineLength; // all lines are the same length
            pos_localPort += lineLength;
            pos_inode += lineLength;
        } while (getline(socketsFile, dummyStr));
#else
        socketsFile.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        ///
        //
        //
        string nextLine(istream&, string&& = string());

        // calling once, or when allocation cost doesn't matter
        auto line = nextLine(strm);
        
        // calling in a loop
        string line;
        for (...)
            line = nextLine(strm, std::move(line));
        ///
        //
        //https://channel9.msdn.com/Events/GoingNative/2013/Writing-Quick-Code-in-Cpp-Quickly
        typedef std::istreambuf_iterator<char> iter;

        std::ifstream input_file("textfile.txt");
        iter file_begin(input_file);
        iter file_end;

        for (iter i = file_begin; i != file_end; ++i)
            std::clog << *i;
#endif
        if (inode == -1)
            log(LogLevel::WARNING, "Inode not found for port <",wantedPort,">");
        return inode;
    }
    catch(char const *msg)
    {
        log(LogLevel::ERR, msg);
        return -2;
    }
}


int getApp(const int inode, string &appName)
{
    DIR *procDir{nullptr}, *fdDir{nullptr};
    try
    {
        static char inodeBuff[64] ={0}; //! @todo size
        static string tmpString;
        // WIN: https://msdn.microsoft.com/en-us/library/ms683180(VS.85).aspx
        static int myPid = ::getpid();

        dirent *pidEntry{nullptr}, *fdEntry{nullptr};
        int pid{0}, fd{0};

        if ((procDir = opendir("/proc/")) == nullptr)
            throw std_ex("Can't open /proc/ directory");

        while ((pidEntry = readdir(procDir)))
        {
            if (chToInt(pidEntry->d_name, pid))
                continue;
            if (myPid == pid || pid == 0)
                continue;

            tmpString = "/proc/"; tmpString += pidEntry->d_name; tmpString += "/fd/";
            if ((fdDir = opendir(tmpString.c_str())) == nullptr)
                throw std_ex("Can't open " + tmpString);

            while ((fdEntry = readdir(fdDir)))
            {
                if (chToInt(fdEntry->d_name, fd))
                    continue;

                if (fd <= 2) // stdin, stdout, stderr
                    continue;
                // the fastest option
                tmpString = "/proc/";  tmpString += pidEntry->d_name;
                tmpString += "/fd/";   tmpString += fdEntry->d_name;
                int ll = readlink(tmpString.c_str(), inodeBuff, sizeof(inodeBuff));
                if (ll == -1)
                    log(LogLevel::ERR, "Readlink error: ", tmpString, "\n", strerror(errno));
                if (inodeBuff[0] != 's' || inodeBuff[6] != ':') // socket:[<inode>]
                    continue;
                char *tmpPtr = strchr(&inodeBuff[7], ']');
                if (tmpPtr == nullptr)
                    throw std_ex(concatenate("Right ']' not found in the socket link: ", inodeBuff));
                *tmpPtr = '\0';
                int foundInode {0};
                if (chToInt(&inodeBuff[8], foundInode))
                    throw "Can't convert socket inode to integer";
                if (foundInode == inode)
                {
                    ifstream appNameFile(concatenate("/proc/", pidEntry->d_name, "/cmdline"));
                    // arguments are delimited with '\0'
                    getline(appNameFile,appName);

                    closedir(fdDir);
                    goto END;
                }
            }
            closedir(fdDir);
        }
END:
        closedir(procDir);
        if (pidEntry == nullptr)
        {
            log(LogLevel::ERR, "Application not found for inode ", inode);
            g_notFoundApps++;
        }
        return 0;
    }
    catch(std_ex& e) {
        cerr << e.what() << std::endl;
        if (procDir)
            closedir(procDir);
        if (fdDir)
            closedir(fdDir);
        return -1;
    }
    catch(char const *msg)
    {
        cerr << "ERROR: " << msg << endl;
        if (procDir)
            closedir(procDir);
        if (fdDir)
            closedir(fdDir);
        return -1;
    }
}


}
