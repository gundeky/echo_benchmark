import os
import sys
import time
import socket
import struct

HOST = 'localhost'
msg = "GET /community/api_sim_start HTTP/1.1\r\nHost: localhost\r\nUser-Agent: Test\r\n\r\n"

# readSome, readAll test
def test1(port):
    print 'connect -> close'
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect((HOST, port))
    s.close()
    print 'completed'

def test2(port):
    print 'connect -> sleep 1 -> send'
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect((HOST, port))
    time.sleep(1)
    s.sendall(msg)
    data = s.recv(10240)
    s.close()
    print 'Received:', data

def test3(port):
    print 'connect -> reset'
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect((HOST, port))
    l_onoff = 1
    l_linger = 0
    s.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, struct.pack('ii', l_onoff, l_linger))
    s.close()
    print 'completed'

def test4(port):
    print 'connect -> send all'
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect((HOST, port))
    s.sendall(msg)
    data = s.recv(10240)
    s.close()
    print 'Received:', data

def test5(port):
    print 'connect -> partial send -> sleep -> partial send'
    third = len(msg) / 3
    msg1 = msg[:third]
    msg2 = msg[third:third*2]
    msg3 = msg[third*2:]
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect((HOST, port))
    s.sendall(msg1)
    time.sleep(1)
    s.sendall(msg2)
    time.sleep(1)
    s.sendall(msg3)
    data = s.recv(10240)
    s.close()
    print 'Received:', data

# writeAll test
def test6(port):
    print 'connect -> send -> close'
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect((HOST, port))
    s.sendall(msg)
    s.close()
    print 'completed'

def test7(port):
    print 'connect -> send -> reset'
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect((HOST, port))
    s.sendall(msg)
    time.sleep(0.1)
    l_onoff = 1
    l_linger = 0
    s.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, struct.pack('ii', l_onoff, l_linger))
    s.close()
    print 'completed'

def test8(port):
    print 'connect -> recv buf 0 -> send all -> sleep 100'
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect((HOST, port))
    rcvbuf = s.getsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF)
    print 'rcvbuf 1:', rcvbuf
    s.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 1)
    rcvbuf = s.getsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF)
    print 'rcvbuf 2:', rcvbuf
    s.sendall(msg)
    print 'enter sleep'
    time.sleep(3)
    print 'exit sleep'
    #data = s.recv(10240)
    s.close()
    print 'complete'

# onClose
def test9(port):
    print 'connect -> recv buf 0 -> send all -> sleep 1 -> close'
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect((HOST, port))
    rcvbuf = s.getsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF)
    print 'rcvbuf 1:', rcvbuf
    s.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 1)
    rcvbuf = s.getsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF)
    print 'rcvbuf 2:', rcvbuf
    s.sendall(msg)
    print 'enter sleep'
    time.sleep(1)
    print 'exit sleep'
    #data = s.recv(10240)
    s.close()
    print 'complete'

# onWrite 2 ? -> onClose
def test10(port):
    print 'connect -> recv buf 0 -> send all -> sleep 1 -> reset'
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect((HOST, port))
    rcvbuf = s.getsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF)
    print 'rcvbuf 1:', rcvbuf
    s.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 1)
    rcvbuf = s.getsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF)
    print 'rcvbuf 2:', rcvbuf
    s.sendall(msg)
    print 'enter sleep'
    time.sleep(1)
    print 'exit sleep'
    #data = s.recv(10240)
    l_onoff = 1
    l_linger = 0
    s.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, struct.pack('ii', l_onoff, l_linger))
    s.close()
    print 'complete'

# onWrite 3 ? not working
def test11(port):
    print 'connect -> recv buf 0 -> send all -> sleep 1 -> read 1 byte -> sleep 10 -> close'
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect((HOST, port))
    rcvbuf = s.getsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF)
    print 'rcvbuf 1:', rcvbuf
    s.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 1)
    rcvbuf = s.getsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF)
    print 'rcvbuf 2:', rcvbuf
    s.sendall(msg)
    print 'enter sleep'
    time.sleep(3)
    print 'exit sleep'
    data = s.recv(19000)
    time.sleep(3)
    s.close()
    print 'complete'

# onWrite 4
def test12(port):
    print 'connect -> recv buf 0 -> send all -> sleep 1 -> read 20480 byte -> sleep 10 -> close'
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect((HOST, port))
    rcvbuf = s.getsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF)
    print 'rcvbuf 1:', rcvbuf
    s.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 1)
    rcvbuf = s.getsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF)
    print 'rcvbuf 2:', rcvbuf
    s.sendall(msg)
    print 'enter sleep'
    time.sleep(3)
    print 'exit sleep'
    data = s.recv(20480)
    time.sleep(3)
    s.close()
    print 'complete'

def test13():
    port = 9004
    print 'connect -> send -> sleep 1 -> send'
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect((HOST, port))
    s.sendall('a')
    time.sleep(3)
    s.sendall('bc')
    time.sleep(3)
    s.sendall('d')
    data = s.recv(10240)
    s.close()
    print 'Received:', data
    assert data == 'abcd'

#def testServer():
    #PORT = 10000
    #s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    #s.bind((HOST, PORT))
    #s.listen(1)
    #conn, address = s.accept()    # do not accept
    #print 'accepted:', address
    #print 'enter sleep'
    #time.sleep(100)

def testConn():
    PORT = 10000
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.bind((HOST, PORT))
    s.listen(1)
    print 'listen start. telnet localhost 9005. and press any key to accept'
    data = sys.stdin.readline();
    print 'accept start'
    conn, address = s.accept()    # do not accept
    print 'accepted:', address

    #port = 9005
    #print 'server start port 10000'
    #import thread
    #thread.start_new_thread(testServer, ())
    #time.sleep(3)
    #s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    #s.connect((HOST, port))
    #s.sendall('a')
    #print 'sleep for 3 sec 1'
    #time.sleep(3)
    #print 'sleep for 3 sec 2'
    #time.sleep(3)

PORT = 9000
#PORT = 9001
#PORT = 9002
#PORT = 9003
#PORT = 9004
#PORT = 9005
# 9000 : readSome test
# 9001 : readAll  test
# 9002 : writeAll test
# 9003 : writeAll 2 test : send buffer full test
# 9004 : readvAll + writevAll test
# 9005 : connect test

def usage():
    print 'usage) python %s [1, 2, 3, 4...]' % sys.argv[0]

if len(sys.argv) != 2 and len(sys.argv) != 3:
    usage()
    os._exit(1)

if len(sys.argv) is 3:
    PORT = int(sys.argv[2])

print 'port:', PORT

if sys.argv[1] == '1':
    test1(PORT)
elif sys.argv[1] == '2':
    test2(PORT)
elif sys.argv[1] == '3':
    test3(PORT)
elif sys.argv[1] == '4':
    test4(PORT)
elif sys.argv[1] == '5':
    test5(PORT)
elif sys.argv[1] == '6':
    test6(PORT)
elif sys.argv[1] == '7':
    test7(PORT)
elif sys.argv[1] == '8':
    test8(PORT)
elif sys.argv[1] == '9':
    test9(PORT)
elif sys.argv[1] == '10':
    test10(PORT)
elif sys.argv[1] == '11':
    test11(PORT)
elif sys.argv[1] == '12':
    test12(PORT)
elif sys.argv[1] == '13':
    test13()
elif sys.argv[1] == 'all':
    test1(9000)
    test2(9000)
    test3(9000)
    test4(9000)
    test5(9000)

    test1(9001)
    test2(9001)
    test3(9001)
    test4(9001)
    test5(9001)

    test6(9002)
    test7(9002)
    test8(9002)
    test9(9002)
    test10(9002)
    test11(9002)
    test12(9002)

    test6(9003)
    test7(9003)
    test8(9003)
    test9(9003)
    test10(9003)
    test11(9003)
    test12(9003)

    test1(9004)
    test2(9004)
    test3(9004)
    test4(9004)
    test5(9004)

    test6(9004)
    test7(9004)
    test8(9004)
    test9(9004)
    test10(9004)
    test11(9004)
    test12(9004)

    test13()

elif sys.argv[1] == 'conn':
    testConn()
else:
    usage()
    os._exit(1)

