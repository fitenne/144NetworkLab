#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity)
    : _stroage{}, _unasmed(0), _unassembled_bytes(0), _eof_flag(false), _last_byte(0), _output(capacity), _capacity(capacity) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    size_t unaccp = _output.bytes_read() + _capacity;

    // interval we are interested in
    size_t l = max(index, _unasmed),
           r = min(index + data.size(), unaccp); 
    
    // push [l, r) to _stroage
    _push_stroage(data, index, l, r, unaccp);
    
    // assemble
    _assemble();

    if (eof) {
        _eof_flag = true;
        _last_byte = index + data.size();
    }

    if (_eof_flag && _unasmed == _last_byte) {
        _output.end_input();
    }
}

size_t StreamReassembler::unassembled_bytes() const { return _unassembled_bytes; }

size_t StreamReassembler::next_unassembled() const { return _unasmed; }

bool StreamReassembler::empty() const { return _unassembled_bytes == 0; }

void StreamReassembler::_push_stroage(const std::string &data, size_t index, size_t interested_l, size_t interested_r, size_t unaccp) {
    size_t interval_start = _unasmed;
    for (auto it = _stroage.begin(); it != _stroage.end(); ++it) {
        size_t interval_end = it->index;
        size_t b = max(interested_l, interval_start);
        size_t e = min(interested_r, interval_end);
        if (e > b) {
            _stroage.emplace(it, b, std::string(data.begin() + (b - index), data.begin() + (e - index)));
            _unassembled_bytes += (e - b);
        }

        interval_start = it->index + it->data.size();
        if (interval_start >= interested_r) break;
    }
    size_t b = max(interested_l, interval_start);
    size_t e = min(interested_r, unaccp);
    if (e > b) {
        _stroage.emplace_back(b, std::string(data.begin() + (b - index), data.begin() + (e - index)));
        _unassembled_bytes += (e - b);
    }    
}

void StreamReassembler::_assemble() {
    while (!_stroage.empty() && _stroage.front().index == _unasmed) {
        auto &d = _stroage.front().data;
        size_t bytes = _output.write(d);

        _unassembled_bytes -= d.size();
        _unasmed += bytes;
        if (bytes != d.size()) {
            // try later?
            _stroage.front().index += bytes;
            d = std::string(d.begin() + bytes, d.end());
            break;
        }

        _stroage.pop_front();
    }
}