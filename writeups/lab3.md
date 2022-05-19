Lab 3 Writeup
=============

# 需要注意的点

`fill_window` 需要尽可能填充接收方的窗口，可能会在一次调用中发送多个包。

header 中 fin 的设置与否，需要同时注意窗口大小和输入流的状态。

舍弃不可能的 ack。（比如对没有发送的包的 ack。）

特别的，当 windows size 为 0 时，`fill_window` 表现得像是 window size 为 1，但应该假设这种包不会被 ack，即使超时，也不应修改连续重传数和 RTO。