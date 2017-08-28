#!/usr/bin/env python3
import socket
import time
import select
import signal
import subprocess
import sys

UDP_IP = '10.0.1.1'
# UDP_IP = '127.0.0.1'
UDP_PORT = 56456
TCP_IP = "140.113.207.229"
TCP_PORT = 56123


class UdpClient:
    """Docstring for UdpClient. """

    def __init__(self, tcpip, tcpport, udpip, udpport):
        self.fd_tcp = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.tcp_dst = (tcpip, tcpport)
        # self.fd_tcp.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        # self.fd_tcp.bind((tcpip, tcpport))
        # self.fd_tcp.listen(5)

        self.fd_udp = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.udp_dst = (udpip, udpport)
        # self.fd_udp.bind(self.udp_dst)

        # self.fd_tcp = None

        self.inputs = [self.fd_tcp, self.fd_udp]
        self.outputs = []

        self.result = []

        self.tcp_addr = None
        self.udp_addr = None
        # self.link = None
        self.do_once = True

        # tmp = subprocess.run(
        #     "tail -1 /var/log/kern.log".split(),
        #     check=True,
        #     stdout=subprocess.PIPE).stdout.decode()
        # while not 'ML RSSI' in tmp:
        #     time.sleep(0.1)
        #     tmp = subprocess.run(
        #         "tail -1 /var/log/kern.log".split(),
        #         check=True,
        #         stdout=subprocess.PIPE).stdout.decode()

        # self.rssi_begin = tmp
        # print('rssi_begin:', self.rssi_begin)

    def run(self):
        """TODO: Docstring for run.
        :returns: TODO

        """
        self.start_rx()
        while True:
            data = self.fd_udp.recvfrom(1500)

        # while 1:
        #     readables, writables, errors = select.select(
        #         self.inputs, self.outputs, self.inputs)

        #     for fd in readables:
        #         if fd is self.fd_tcp:
        #             data = fd.recv(1500)
        #             # self.fd_tunnel, self.tcp_addr = fd.accept()
        #             # self.inputs.append(self.fd_tunnel)
        #         elif fd is self.fd_udp:
        #             data, self.udp_addr = fd.recvfrom(1500)
        #             # server_time, seq, payload, link = data.decode().split()
        #             # print(server_time, seq, link)
        #             # TODO: wigig rssi
        #             # self.result.append(
        #             #     "client: {0} server: {1} seq: {2} link: {3} pkt_size: {4} wigig_rssi: {5}\n".format(time.time(), server_time, seq, link, len(data), 123))
        #         # elif fd is self.fd_tunnel:
        #         #     data = fd.recv(1500).decode()
        #         #     print(data)
        #         #     if 'handoff' in data:
        #         #         self.result.append(
        #         #             "client: {0} handoff\n".format(time.time()))
        #         #     # else:
        #         #     #     self.link = data
        #         #     #     assert len(self.link <= 5), "get link error"

        #         #     if self.do_once:
        #         #         self.do_once = False
        #         #         self.start_rx()

    def start_rx(self):
        # tell udpserver I'm ready to receive
        self.fd_udp.sendto("Hello".encode(), self.udp_dst)
        # self.fd_tcp.connect(self.tcp_dst)

    def signal_handler(self, signal, frame):
        print('You pressed Ctrl+C!')

        tmp = subprocess.run(
            "tail -1 /var/log/kern.log".split(),
            check=True,
            stdout=subprocess.PIPE).stdout.decode()
        while not 'ML RSSI' in tmp:
            time.sleep(0.1)
            tmp = subprocess.run(
                "tail -1 /var/log/kern.log".split(),
                check=True,
                stdout=subprocess.PIPE).stdout.decode()

        rssi_end = tmp
        print('rssi_end:', rssi_end)

        begin = False
        end = False
        rssi_result = []
        with open('/var/log/kern.log', 'r') as f:
            for line in f:
                if line == self.rssi_begin:
                    begin = True

                if line == rssi_end:
                    end = True

                if begin and 'ML RSSI' in line:
                    rssi_result.append(line)

                if end:
                    dot = sys.argv[1].rfind(".")
                    filename = sys.argv[1][:dot] + ".client.rssi"
                    print('Save to', filename)
                    with open(filename, 'w') as ff:
                        ff.write("\n".join(rssi_result))
                    break
        sys.exit(0)


def main():
    u = UdpClient(TCP_IP, TCP_PORT, UDP_IP, UDP_PORT)
    # signal.signal(signal.SIGINT, u.signal_handler)
    u.run()
    return


if __name__ == "__main__":
    main()
