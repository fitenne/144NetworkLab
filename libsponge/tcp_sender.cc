#include "tcp_sender.hh"

#include "tcp_config.hh"

#include "buffer.hh"

#include <util.hh>

#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;
 //! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity) {}

uint64_t TCPSender::bytes_in_flight() const {
    return _bytes_in_flight;
}

void TCPSender::fill_window() {
    bool backoff_timer = true;
    if (_window_size == 0) {
        //! act like the window size is one
        _window_size = 1;
        backoff_timer = false;
    }

    uint64_t win_rbound = _last_acked + _window_size;
    while (_next_seqno < win_rbound) {
        TCPSegment seg;
        seg.header().seqno = wrap(_next_seqno, _isn);
        if (_next_seqno == 0) {
            seg.header().syn = true;
        }

        if (!_stream.buffer_empty()) {
            size_t len = min(win_rbound - _next_seqno, TCPConfig::MAX_PAYLOAD_SIZE);
            len = min(len, _stream.buffer_size());
            std::string data = _stream.read(len);

            InternetChecksum chksum;
            chksum.add(data);
            seg.payload() = std::move(data);
            seg.header().cksum = chksum.value();
        }
        
        if (_stream.eof()) {
            uint64_t r = _next_seqno + seg.length_in_sequence_space();
            if (r <= _stream.bytes_written() + 1 && r < win_rbound) {
                seg.header().fin = true;
            }
        }   

        if (seg.length_in_sequence_space()) {
            _send_segment(seg);
            _in_flight.emplace_back(_next_seqno, seg, backoff_timer);
            _next_seqno += seg.length_in_sequence_space();
            _bytes_in_flight += seg.length_in_sequence_space();
        } else break;
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    _window_size = window_size;
    uint64_t ackno_abs = unwrap(ackno, _isn, _next_seqno);
    if (ackno_abs > _next_seqno) return;
    if (ackno_abs <= _last_acked) return;

    _last_acked = ackno_abs;

    while (!_in_flight.empty()) {
        auto &fnt = _in_flight.front();
        uint64_t r_bound = fnt.seqno_absolute + fnt.seg.length_in_sequence_space();
        if (ackno_abs >= r_bound) {
            _bytes_in_flight -= fnt.seg.length_in_sequence_space();
            _in_flight.pop_front();
        } else {
            break;
        }
    }

    _RTO = _initial_retransmission_timeout;
    _consec_retx = 0;
    _retx_timer.reset();
    if (!_in_flight.empty()) {
        _retx_timer.emplace(_RTO);
    }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    if (!_retx_timer.has_value()) return;
    if (_retx_timer.value() > ms_since_last_tick) {
        //! not expired
        _retx_timer.value() -= ms_since_last_tick;
        return;
    }

    _retx_timer.reset();
    InFlightSegment &ifs = _in_flight.front();
    //! ignoring window size
    if (ifs.backoff_timer) {
        ++_consec_retx;
        _RTO *= 2;
    }
    _send_segment(ifs.seg);
}

unsigned int TCPSender::consecutive_retransmissions() const {
    return _consec_retx;
}

void TCPSender::send_empty_segment() {
    TCPSegment seg = TCPSegment();
    //! should generate and send a TCPSegment that has zero length in sequence space
    seg.header().seqno = WrappingInt32(_next_seqno);
    seg.payload() = Buffer();

    _segments_out.push(seg);
    return;
}

void TCPSender::_send_segment(const TCPSegment &segment) {
    _segments_out.push(segment);
    if (!_retx_timer.has_value()) _retx_timer.emplace(_RTO);
}