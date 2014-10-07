/*
 * Copyright (C) 2014 Cloudius Systems, Ltd.
 */

#ifndef NET_HH_
#define NET_HH_

#include "core/reactor.hh"
#include "core/deleter.hh"
#include "core/queue.hh"
#include "core/stream.hh"
#include "ethernet.hh"
#include "packet.hh"
#include <unordered_map>

namespace net {

class packet;
class interface;
class device;
class l3_protocol;

struct hw_features {
    // Enable tx checksum offload
    bool tx_csum_offload;
    // Enable rx checksum offload
    bool rx_csum_offload;
};

class l3_protocol {
    interface* _netif;
    uint16_t _proto_num;
public:
    explicit l3_protocol(interface* netif, uint16_t proto_num);
    subscription<packet, ethernet_address> receive(
            std::function<future<> (packet, ethernet_address)> rx_fn);
    future<> send(ethernet_address to, packet p);
private:
    friend class interface;
};

class interface {
    std::unique_ptr<device> _dev;
    subscription<packet> _rx;
    struct l3_rx_stream {
        stream<packet, ethernet_address> packet_stream;
        future<> ready;
        l3_rx_stream() : ready(packet_stream.started()) {}
    };
    std::unordered_map<uint16_t, l3_rx_stream> _proto_map;
    ethernet_address _hw_address;
    net::hw_features _hw_features;
private:
    future<> dispatch_packet(packet p);
    future<> send(uint16_t proto_num, ethernet_address to, packet p);
public:
    explicit interface(std::unique_ptr<device> dev);
    ethernet_address hw_address() { return _hw_address; }
    net::hw_features hw_features() { return _hw_features; }
    subscription<packet, ethernet_address> register_l3(uint16_t proto_num,
            std::function<future<> (packet p, ethernet_address from)> next);
    friend class l3_protocol;
};

class device {
public:
    virtual ~device() {}
    virtual subscription<packet> receive(std::function<future<> (packet)> next_packet) = 0;
    virtual future<> send(packet p) = 0;
    virtual future<> l2inject(packet p) { assert(0); return make_ready_future(); }
    virtual ethernet_address hw_address() = 0;
    virtual net::hw_features hw_features() = 0;
};

extern __thread device *dev;
}

#endif /* NET_HH_ */
