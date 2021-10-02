#pragma once

#include <sys/socket.h>

#include "Network/Address.cpp"

namespace Network
{

    class EndPoint
    {
    private:

        // Common

        Address _Address;
        int _Port = 0;

        // IPv6 Specific

        uint32_t _Flow = 0;
        uint32_t _Scope = 0;

    public:

        // Constructors :
    
        EndPoint() = default;

        EndPoint(struct sockaddr *SocketAddress)
        {
            if (SocketAddress->sa_family == Address::IPv4)
            {
                struct sockaddr_in addressStruct = *((struct sockaddr_in *)SocketAddress);
                _Address.Set(addressStruct);
                _Port = addressStruct.sin_port;
            }
            else
            {
                struct sockaddr_in6 addressStruct = *((struct sockaddr_in6 *)SocketAddress);
                _Address.Set(addressStruct);
                _Port = addressStruct.sin6_port;
                _Flow = addressStruct.sin6_flowinfo;
                _Scope = addressStruct.sin6_scope_id;
            }
        }

        EndPoint(struct sockaddr_in& SocketAddress) : _Address(SocketAddress.sin_addr)
        {
            _Port = SocketAddress.sin_port;
        }

        EndPoint(struct sockaddr_in6& SocketAddress) : _Address(SocketAddress.sin6_addr)
        {
            _Port = SocketAddress.sin6_port;
            _Flow = SocketAddress.sin6_flowinfo;
            _Scope = SocketAddress.sin6_scope_id;
        }

        EndPoint(const Address& EndPointAddress, int Port) : _Address(EndPointAddress), _Port(Port){}

        EndPoint(const EndPoint& Othere) : _Address(Othere._Address), _Port(Othere._Port), _Flow(Othere._Flow), _Scope(Othere._Scope) {}

        // Functions :

        int sockaddr_in(struct sockaddr_in * SocketAddress){
            std::memset(SocketAddress, 0 , sizeof(struct sockaddr_in));
            _Address.in_addr(&(SocketAddress->sin_addr));
            SocketAddress->sin_family = (sa_family_t) _Address.Family();
            SocketAddress->sin_port = htons(_Port);
            return sizeof(struct sockaddr_in);
        }

        int sockaddr_in6(struct sockaddr_in6 * SocketAddress){
            std::memset(SocketAddress, 0 , sizeof(struct sockaddr_in6));
            _Address.in6_addr(&(SocketAddress->sin6_addr));
            SocketAddress->sin6_family = (sa_family_t) _Address.Family();
            SocketAddress->sin6_port = htons(_Port);
            SocketAddress->sin6_flowinfo = _Flow;
            SocketAddress->sin6_scope_id = _Scope;
            return sizeof(struct sockaddr_in6);
        }

        int sockaddr(struct sockaddr * SocketAddress){
            if(SocketAddress->sa_family == Address::IPv4){
                return sockaddr_in((struct sockaddr_in *) SocketAddress);
            }
            else{
                return sockaddr_in6((struct sockaddr_in6 *) SocketAddress);
            }
        }

        // Properties

            //  Setters

        Address& address(){return _Address;}
        const Address& address() const {return _Address;}

        int port(){return _Port;}
        int port(int Port){_Port = Port;return _Port;}
        int port() const {return _Port;}

        int flow() { return _Flow; }
        int flow() const { return _Flow; }

        int scope() { return _Scope; }
        int scope() const { return _Scope; }

            //  Setters

        void Set(Address address, int Port, int Scope, int Flow){
            _Address = address;
            _Port = Port;
            _Scope = Scope;
            _Flow = Flow;
        }

        friend std::ostream &operator << (std::ostream &os, const EndPoint& tc)
        {
            return os << tc._Address << ":" << tc._Port;
        }

    };
}