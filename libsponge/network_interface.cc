#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

#include <iostream>

// Dummy implementation of a network interface
// Translates from {IP datagram, next hop address} to link-layer frame, and from link-layer frame to IP datagram

// For Lab 5, please replace with a real implementation that passes the
// automated checks run by `make check_lab5`.

// You will need to add private members to the class declaration in `network_interface.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(const EthernetAddress &ethernet_address, const Address &ip_address)
    : _ethernet_address(ethernet_address), _ip_address(ip_address) {
    cerr << "DEBUG: Network interface has Ethernet address " << to_string(_ethernet_address) << " and IP address "
         << ip_address.ip() << "\n";
}

EthernetFrame NetworkInterface::_new_ARP_reply(const EthernetAddress &dst_eth, const uint32_t dst_ip) {
    ARPMessage msg;
    msg.opcode = msg.OPCODE_REPLY;
    msg.sender_ethernet_address = _ethernet_address;
    msg.sender_ip_address = _ip_address.ipv4_numeric();
    msg.target_ethernet_address = dst_eth;
    msg.target_ip_address = dst_ip;

    EthernetFrame reply;
    reply.header().src = _ethernet_address;
    reply.header().dst = dst_eth;
    reply.header().type = EthernetHeader::TYPE_ARP;
    reply.payload() = std::move(msg.serialize());
    return reply;
}

EthernetFrame NetworkInterface::_new_ARP_request(const uint32_t addr) {
    ARPMessage msg;
    msg.opcode = msg.OPCODE_REQUEST;
    msg.sender_ethernet_address = _ethernet_address;
    msg.sender_ip_address = _ip_address.ipv4_numeric();
    msg.target_ip_address = addr;

    EthernetFrame reply;
    reply.header().src = _ethernet_address;
    reply.header().dst = ETHERNET_BROADCAST;
    reply.header().type = EthernetHeader::TYPE_ARP;
    reply.payload() = std::move(msg.serialize());
    return reply;
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but may also be another host if directly connected to the same network as the destination)
//! (Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) with the Address::ipv4_numeric() method.)
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop) {
    // convert IP address of next hop to raw 32-bit representation (used in ARP header)
    const uint32_t next_hop_ip = next_hop.ipv4_numeric();

    if (!_lookup.count(next_hop_ip)) {
        if (!_ARP_request_ttl.count(next_hop_ip)) {
            _frames_out.emplace(_new_ARP_request(next_hop_ip));
            _ARP_request_ttl[next_hop_ip] = NetworkInterface::_ms_ARP_request_timeout;
        }
        _send_later[next_hop_ip].emplace_back(dgram, next_hop);
        return;
    }

    const auto &next_hop_eth = _lookup[next_hop_ip].ethaddr;
    EthernetFrame frame{};
    frame.header().type = EthernetHeader::TYPE_IPv4;
    frame.header().src = _ethernet_address;
    frame.header().dst = next_hop_eth;
    frame.payload() = std::move(dgram.serialize());
    _frames_out.emplace(std::move(frame));
}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    const auto &h = frame.header();
    //! ignore frame not destined to us
    if (h.dst != _ethernet_address && h.dst != ETHERNET_BROADCAST) return {};

    if (h.type == EthernetHeader::TYPE_IPv4) {
        InternetDatagram dgram;
        if (dgram.parse(frame.payload()) != ParseResult::NoError) {
            throw std::runtime_error("InternetDatagram parse error");
        }
        return std::make_optional(std::move(dgram));
    }
    
    if (h.type == EthernetHeader::TYPE_ARP) {
        ARPMessage msg;
        if (msg.parse(frame.payload()) != ParseResult::NoError) {
            throw std::runtime_error("InternetDatagram parse error");
        }

        //! only ip in ethernet is supported
        if (msg.hardware_type == ARPMessage::TYPE_ETHERNET && msg.protocol_type == EthernetHeader::TYPE_IPv4) {
            auto &srcip = msg.sender_ip_address;
            _lookup[srcip] = _LookupEntry(msg.sender_ethernet_address);
            if (_send_later.count(srcip)) {
                for (auto &f: _send_later[srcip]) {
                    send_datagram(f.first, f.second);
                }
            }
            _send_later.erase(srcip);

            if (msg.opcode == ARPMessage::OPCODE_REQUEST && msg.target_ip_address == _ip_address.ipv4_numeric()) {
                _frames_out.emplace(_new_ARP_reply(msg.sender_ethernet_address, msg.sender_ip_address));
            }
        }
    } 
    return {};
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) {
    for (auto it = _lookup.begin(); it != _lookup.end(); ) {
        auto &entry = it->second;
        if (entry.ttl < ms_since_last_tick) {
            it = _lookup.erase(it);
        } else {
            entry.ttl -= ms_since_last_tick;
            ++it;
        }
    }
    for (auto it = _ARP_request_ttl.begin(); it != _ARP_request_ttl.end(); ) {
        auto &ttl = it->second;
        if (ttl < ms_since_last_tick) {
            it = _ARP_request_ttl.erase(it);
        } else {
            ttl -= ms_since_last_tick;
            ++it;
        }
    }
}
