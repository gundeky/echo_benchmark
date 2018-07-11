import os
import sys
import time
import socket
import SocketServer

def tcpclient2():
    HOST = 'localhost'
    #PORT = 9000
    PORT = 13579
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect((HOST, PORT))
    print 'send 1'
    s.sendall("1")
    time.sleep(1)
    print 'send 2'
    s.sendall("2")
    time.sleep(1)
    print 'send 3'
    s.sendall("3")
    time.sleep(1)
    #data = s.recv(10240)
    print 'sleep before close'
    time.sleep(3)
    print 'close now'
    s.close()
    #print 'Received:', data

def tcpclient():
    HOST = 'localhost'
    PORT = 9000
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect((HOST, PORT))
    s.sendall("GET /community/api_sim_start HTTP/1.1\r\nHost: localhost\r\nUser-Agent: Test\r\n\r\n")
    data = s.recv(10240)
    #print 'sleep before close'
    #time.sleep(3)
    #print 'close now'
    s.close()
    print 'Received:', data

def udpclient():
    HOST = 'localhost'
    PORT = 9000
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.sendto('this is test message', (HOST, PORT))
    data, addr = s.recvfrom(65535)
    s.close()
    print 'Received:', data, addr

def backend():
    HOST = 'localhost'
    PORT = 9001
    class MyTCPHandler(SocketServer.BaseRequestHandler):
        def handle(self):
            self.data = self.request.recv(1024)
            print 'addr : ', self.client_address[0]
            print 'data : ', self.data
            self.request.sendall(self.data)
    server = SocketServer.TCPServer((HOST, PORT), MyTCPHandler)
    server.allow_reuse_address = True
    server.serve_forever()

def usage():
    print 'usage) python %s [tcp/udp/backend]' % sys.argv[0]

if len(sys.argv) != 2:
    usage()
    os._exit(1)

if sys.argv[1] == 'tcp':
    tcpclient()
elif sys.argv[1] == 'tcp2':
    tcpclient2()
elif sys.argv[1] == 'udp':
    udpclient()
elif sys.argv[1] == 'backend':
    backend()
else:
    usage()
    os._exit(1)

