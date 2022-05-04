#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity)
    : _capacity{capacity + 1}, _front{0}, _end{0}, _bytes_written{0}, _bytes_read{0}, _input_ended{false}, _error{false} {
    _buffer.reserve(capacity + 1); // one extra space to keep code simple
}

size_t ByteStream::write(const string &data) {
    size_t bw = data.size();
    if (remaining_capacity() < data.size()) {
        bw = remaining_capacity();
    }

    // we are sure to have sufficient space to write bw bytes
    size_t p = 0;
    if (_end + bw >= _capacity) {
        p = _capacity - _end;
        copy(data.begin(), data.begin() + p, _buffer.begin() + _end);
        _end = 0;
    }
    copy(data.begin() + p, data.begin() + bw, _buffer.begin() + _end);
    _end += (bw - p);
    _bytes_written += bw;
    return bw;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    // peek the maximum available if len > buffer size
    size_t l = len;
    if (l > buffer_size()) l = buffer_size();

    if (_end > _front) {
        return std::string(_buffer.begin() + _front, _buffer.begin() + _front + l);
    } 

    std::string s(_buffer.begin() + _front, _buffer.begin() + _capacity);
    s += std::string(_buffer.begin(), _buffer.begin() + l - (_capacity - _front));
    return s;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    if (len > buffer_size()) {
        set_error();
        return;
    }

    _front = (_front + len) % _capacity;
    _bytes_read += len;
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    if (!len) return std::string();

    auto s = peek_output(len);
    pop_output(min(len, buffer_size()));
    return s;
}

void ByteStream::end_input() {
    _input_ended = true;
}

bool ByteStream::input_ended() const {
    return _input_ended;
}

size_t ByteStream::buffer_size() const {
    if (buffer_empty()) return 0;

    if (_end > _front) {
        return _end - _front;
    }

    return _capacity - _front + _end;
}

bool ByteStream::buffer_empty() const {
    return _front == _end;
}

bool ByteStream::eof() const {
    return input_ended() && buffer_empty();
}

size_t ByteStream::bytes_written() const {
    return _bytes_written;
}

size_t ByteStream::bytes_read() const {
    return _bytes_read;
}

size_t ByteStream::remaining_capacity() const {
    return _capacity - 1 - buffer_size();
}
