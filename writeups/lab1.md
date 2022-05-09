Lab 1 Writeup
=============

# StreamReassembler

简单地用 `list<pair<index, data>>` 存储数据片段。

`push_substring(data, index, eof)` 方法截取 data 中我们感兴趣的取件，将其不重叠的存入 list，然后立刻尝试从 list 中取出数据，合并到我们维护的有序字节流中。之后检查字节流是否到达了 eof。

