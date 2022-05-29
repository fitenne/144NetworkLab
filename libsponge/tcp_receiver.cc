#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    auto &p = seg.payload();
    auto &h = seg.header();
    if (h.syn) {
        _isn = seg.header().seqno;
        _ackno = 0;
    }

    if (_ackno.has_value() && !_reassembler.stream_out().input_ended()) {  // syned
        uint64_t abs_seqno = unwrap(h.seqno, _isn, _ackno.value());
        if (abs_seqno + seg.length_in_sequence_space() > _ackno) {
            _reassembler.push_substring(p.copy(), abs_seqno > 0 ? abs_seqno - 1 : 0, h.fin);
        }
        _ackno = 1 + _reassembler.stream_out().bytes_written();
        if (_reassembler.stream_out().input_ended()) {
            _ackno.value()++;
        }
    }
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    if (!_ackno.has_value())
        return std::optional<WrappingInt32>();

    return std::make_optional<WrappingInt32>(wrap(_ackno.value(), _isn));
}

size_t TCPReceiver::window_size() const {
    if (!_ackno.has_value() || _ackno.value() <= 1) {
        return _capacity;
    }

    return _reassembler.stream_out().bytes_read() + _capacity + 1 - _ackno.value();
}
