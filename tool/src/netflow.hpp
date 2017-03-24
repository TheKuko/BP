/** 
 *  @file       netflow.hpp
 *  @brief      Netflow structure header file
 *  @author     Jozef Zuzelka (xzuzel00)
 *  Mail:       xzuzel00@stud.fit.vutbr.cz
 *  Created:    26.02.2017 23:13
 *  Edited:     22.03.2017 20:02
 *  Version:    1.0.0
 */

#pragma once

#include <string>           //  string
#include <fstream>          //  ostream
#include <netinet/in.h>     //  in_addr, in6_addr
#include <arpa/inet.h>      //  inet_ntop()

class TEntry;


/*!
 * @class Netflow
 * @brief Netflow class contains information about packet needed to uniquely determine
 * a netflow it belongs to.
 */
class Netflow
{
    unsigned char ipVersion  =0;         //!< IP header version
    /*!
     * @brief   Pointer to a structure containing local IP address
     * @details Type of the pointer is determined using #Netflow::ipVersion
     */
    void *localIp            = nullptr;
    unsigned short localPort =0;         //!< Local port
    unsigned char proto      =0;         //!< Layer 4 protocol
    long startTime           =0;         //!< Time of the first packet which belongs to this netflow
    long endTime             =0;         //!< Time of the last packet which belongs to this netflow
public:
    /*!
     * @brief Defalut constructor
     */
    Netflow()                               {}
    /*!
     * @brief   Destructor cleans memory pointed by #Netflow::srcIp and #Netflow::dstIp pointers
     */
    ~Netflow();
    /*! 
     * @brief   Get method for #Netflow::ipVersion
     * @return  IP header version
     */
    unsigned char getIpVersion()            { return ipVersion; }
    /*! 
     * @brief       Set method for #Netflow::ipVersion
     * @param[in]   ipV       IP header version
     */
    void setIpVersion(unsigned char ipV)    { ipVersion = ipV; }
    /*! 
     * @brief   Get method for #Netflow::localIp
     * @return  Pointer to local IP structure
     */
    void * getLocalIp()                     { return localIp; }
    /*! 
     * @brief       Set method for #Netflow::localIp
     * @pre         newIp must point to a valid in*_addr structure
     * @post        Memory pointed by newIp must exist as long as Netflow object exists.
     *              Then it will be freed in destructor.
     * @param[in]   newIp     Local IP structure pointer
     */
    void setLocalIp(void *newIp)            { localIp = newIp; }
    /*! 
     * @brief   Get method for #Netflow::localPort
     * @return  Local port
     */
    unsigned short getLocalPort()           { return localPort; }
    /*! 
     * @brief       Set method for #Netflow::localPort
     * @param[in]   newPort   Local port
     */
    void setLocalPort(unsigned short newPort){ localPort = newPort; }
    /*! 
     * @brief   Get method for #Netflow::proto
     * @return  Layer 4 protocol
     */
    unsigned char getProto()                { return proto; }
    /*! 
     * @brief       Set method for #Netflow::proto
     * @param[in]   newProto  Layer 4 protocol
     */
    void setProto(unsigned char newProto)   { proto = newProto; }
    /*! 
     * @brief   Get method for #Netflow::startTime
     * @return  Time of the first packet which belongs to this netflow
     */
    int getStartTime()                      { return startTime; }
    /*! 
     * @brief       Set method for #Netflow::startTime
     * @param[in]   newTime   Time of the first packet which belongs to this netflow
     */
    void setStartTime(long newTime)         { startTime = newTime; }
    /*! 
     * @brief   Get method for #Netflow::endTime
     * @return  Time of the last packet which belongs to this netflow
     */
    int getEndTime()                        { return endTime; }
    /*! 
     * @brief       Set method for #Netflow::endTime
     * @param[in]   newTime   Time of the last packet which belongs to this netflow
     */
    void setEndTime(long newTime)           { endTime = newTime; }
    /*!
     * @brief   Function prints content of the Netflow structure
     */
    void print();
    /*!
     * @brief   Overloaded equality operator
     * @details Compares just netflow relevant variables
     */
    bool operator==(const Netflow& other) const;
    /*!
     * @brief       Writes structure into the output file
     * @param[in]   file    The output file
     * @return      Amount of written data to the output file in bytes
     */
    unsigned int write(std::ofstream & file);
    friend class TEntry;
};