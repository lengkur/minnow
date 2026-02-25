Checkpoint 1 Writeup
====================

- 在这个实验中我学习了protocol5的ipv4和protocol17的ip包（udp包）结构，知道了网络中是怎么处理ip包，并使用RawSocket对象完成了相关编程

  遇到的问题：

  1.因为计算checksum是以2字节一对的，如果是奇数的话要补上0，在udp包构造过程中经常因为长度是奇数，然后有没有补0导致错误

  2.在计算protocol的值时，因为是以2字节一对的，如果protocol占用一个字节，要往protocol前面补0凑成2字节

  
