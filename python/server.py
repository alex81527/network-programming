#!/usr/bin/env python3
import socket
import time
import select
# import sys
# import threading

#
UDP_IP = '10.0.1.1'
# UDP_IP = '127.0.0.1'
UDP_PORT = 56456
TCP_IP = '140.113.207.245'
TCP_PORT = 56123

# throughput 100 Mbps, packet size 1024 byte
DESIRED_THROUGHPUT = 50 * 1000000
DESIRED_PACKET_SIZE = 1024
SEND_INTERVAL = DESIRED_PACKET_SIZE * 8 / DESIRED_THROUGHPUT
SEQ_DIGITS = 9

#


class UdpServer:

    def __init__(self, tcpip, tcpport, udpip, udpport, pktsize, interval,
                 digits):
        self.fd_tcp = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.fd_tcp.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.fd_tcp.bind((tcpip, tcpport))
        self.fd_tcp.listen(5)

        self.fd_udp = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.fd_udp.bind((udpip, udpport))

        self.fd_tcp_send = None

        self.inputs = [self.fd_tcp, self.fd_udp]
        self.outputs = []

        self.tcp_addr = None
        self.udp_addr = None
        self.seq = 0
        self.link = None

        self.pktsize = pktsize
        self.send_interval = interval
        self.cur_time = None
        self.nexttime_tx = None

        self.digits = digits
        print('The server is ready to receive')

    def run(self):
        
        msg, self.udp_addr = self.fd_udp.recvfrom(1500)
        self.cur_time = time.time()
        self.nexttime_tx = self.cur_time + self.send_interval
        print('Got UDP connection from {0}, start sending traffic.'.format(self.udp_addr))
        while True:
            self.cur_time = time.time()
            if self.cur_time < self.nexttime_tx:
                continue

            self.nexttime_tx += self.send_interval
            msg = '{0:0>{1}d} '.format(self.seq, self.digits)
            msg_len = self.digits
            assert self.pktsize > msg_len, "packet size set too small"
            msg += 'A' * (self.pktsize - msg_len)
            self.fd_udp.sendto(msg.encode(), self.udp_addr)
            # f.write(msg+"\n")
            self.seq += 1

        # while 1:
        #     readables, writables, errors = select.select(
        #         self.inputs, self.outputs, self.inputs)

        #     for fd in readables:
        #         if fd is self.fd_tcp:
        #             self.fd_tcp_send, self.tcp_addr = self.fd_tcp.accept()
        #             self.outputs.append(self.fd_tcp_send)
        #             self.cur_time = time.time()
        #             self.nexttime_tx = self.cur_time + self.send_interval
        #             print('Got TCP connection from {0}, start sending traffic.'.
        #                   format(self.tcp_addr))
        #         elif fd is self.fd_udp:
        #             msg, self.udp_addr = fd.recvfrom(1500)
        #             self.outputs.append(fd)
        #             self.cur_time = time.time()
        #             self.nexttime_tx = self.cur_time + self.send_interval
        #             print('Got UDP connection from {0}, start sending traffic.'.
        #                   format(self.udp_addr))
        #         # elif fd is self.fd_tunnel:
        #         #     data = fd.recv(1500).decode()
        #         #     print(data)

        #         #     if data == "wifi":
        #         #         self.link = "wifi"
        #         #     elif data == "wigig":
        #         #         self.link = "wigig"

        #     for fd in writables:
        #         if fd is self.fd_udp:
        #             self.cur_time = time.time()
        #             if self.cur_time < self.nexttime_tx:
        #                 continue

        #             self.nexttime_tx += self.send_interval
        #             msg = '{0:0>{1}d} '.format(self.seq, self.digits)
        #             msg_len = self.digits
        #             assert self.pktsize > msg_len, "packet size set too small"
        #             msg += 'A' * (self.pktsize - msg_len)
        #             fd.sendto(msg.encode(), self.udp_addr)
        #             # f.write(msg+"\n")
        #             self.seq += 1
        #             # sk.sendto(bytes(200), client_addr);
        #         if fd is self.fd_tcp_send:
        #             self.cur_time = time.time()
        #             if self.cur_time < self.nexttime_tx:
        #                 continue

        #             self.nexttime_tx += self.send_interval
        #             msg = '{0:0>{1}d} '.format(self.seq, self.digits)
        #             msg_len = self.digits
        #             assert self.pktsize > msg_len, "packet size set too small"
        #             msg += 'A' * (self.pktsize - msg_len)
        #             fd.send(msg.encode())
        #             # f.write(msg+"\n")
        #             self.seq += 1
        #             # sk.sendto(bytes(200), client_addr);


def main():
    u = UdpServer(TCP_IP, TCP_PORT, UDP_IP, UDP_PORT, DESIRED_PACKET_SIZE,
                  SEND_INTERVAL, SEQ_DIGITS)
    u.run()
    return


if __name__ == '__main__':
    main()
