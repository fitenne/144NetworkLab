#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

void TCPConnection::_send_over_internet() {
    auto &que = _sender.segments_out();
    while (!que.empty()) {
        auto &front = que.front();
        if (_receiver.ackno().has_value()) {
            front.header().ack = true;
            front.header().ackno = _receiver.ackno().value();
        }
        front.header().win = _receiver.window_size();

        // !debug
        // std::cerr << "sending: " << front.header().summary() << std::endl;

        _segments_out.emplace(front);

        if (front.header().rst) {
            _sender.stream_in().set_error();
            _receiver.stream_out().set_error();
            _active = false;
            return;
        }
        que.pop();
    }
}

void TCPConnection::_may_shutdown() {
    if (_input_ended && _output_ended) {
        if (_linger_after_streams_finish == false) {
            _active = false;
        } else {
            _lingering = true;
        }
    }
}

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return _time_since_last_segment_received; }

void TCPConnection::segment_received(const TCPSegment &seg) {
    // !debug
    // std::cerr << "received: " << seg.header().summary() << std::endl;

    if (seg.header().rst) {
        _sender.stream_in().set_error();
        _receiver.stream_out().set_error();
        _active = false;
        return;
    }

    //! keep alive segment
    if (_receiver.ackno().has_value() and (seg.length_in_sequence_space() == 0) and
        seg.header().seqno == _receiver.ackno().value() - 1) {
        _sender.send_empty_segment();
        _send_over_internet();
        return;
    }

    if (seg.header().ack) {
        if (_sender.next_seqno_absolute() == 0) {
            return;
        }

        _sender.ack_received(seg.header().ackno, seg.header().win);
        if (_sender.stream_in().input_ended() && _sender.next_seqno() == seg.header().ackno) {
            _output_ended = true;
        }
    }

    _receiver.segment_received(seg);
    if (_receiver.stream_out().input_ended() && !_sender.stream_in().input_ended()) {
        _linger_after_streams_finish = false;
    }
    size_t qsize = _sender.segments_out().size();
    _sender.fill_window();
    if (seg.length_in_sequence_space() != 0 && qsize == _sender.segments_out().size()) {
        //! prefer fill, fallback to empty segment to asure at least one segment is sent
        _sender.send_empty_segment();
    }
    _send_over_internet();

    if (seg.header().fin && _receiver.unassembled_bytes() == 0) {
        _input_ended = true;
    }

    _time_since_last_segment_received = 0;

    _may_shutdown();
}

bool TCPConnection::active() const { return _active; }

size_t TCPConnection::write(const string &data) {
    auto bytes = _sender.stream_in().write(data);
    _sender.fill_window();
    _send_over_internet();
    return bytes;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    _time_since_last_segment_received += ms_since_last_tick;
    _sender.tick(ms_since_last_tick);
    if (_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS) {
        //! not excepted to get empty queue
        _sender.segments_out().back().header().rst = true;
    }

    if (_lingering && _time_since_last_segment_received >= 10 * _cfg.rt_timeout) {
        _active = false;
    }
    _send_over_internet();
}

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
    _sender.fill_window();
    _send_over_internet();
}

void TCPConnection::connect() {
    _sender.fill_window();
    _send_over_internet();
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
            _sender.send_empty_segment();
            _sender.segments_out().back().header().rst = true;
            _send_over_internet();
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}
