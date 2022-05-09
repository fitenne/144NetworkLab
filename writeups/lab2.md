Lab 2 Writeup
=============

# wrapping integers

wrap 可以视为模 $2^{32}$ 意义下的加法。

unwrap 根据接口定义以及 `int32_t operator-(WrappingInt32 a, WrappingInt32 b);` 的定义，不难得到实现。唯一需要注意的是处理下溢。

# tcp receiver

这个类的实现需要处理不少边界情况，最大的问题来源是内部的 StreamReassembler 接受的 stream index 与 absulote seqno 的转换。影响到的地方主要有

- `segment_received`
```c++
...
uint64_t abs_seqno = unwrap(h.seqno, _isn, _ackno.value());
if (abs_seqno + seg.length_in_sequence_space() > _ackno) {
    _reassembler.push_substring(p.copy(), abs_seqno > 0 ? abs_seqno - 1 : 0, h.fin); // ! here
}
_ackno = _reassembler.next_unassembled() + 1;
if (_reassembler.stream_out().input_ended()) { // ! and here
    _ackno.value()++;
}
...
```

- `window_size`
```c++
if (!_ackno.has_value() || _ackno.value() <= 1) {
    return _capacity;
}
...
```