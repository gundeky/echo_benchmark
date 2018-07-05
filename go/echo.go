package main

import (
	"fmt"
	"io"
	"net"
	"os"
	"sync"
)

const (
	HOST = "localhost"
	PORT = "9000"
)

var BUF_SIZE = 1024    // TODO pool
var bufPool = sync.Pool{New: func() interface{} {return make([]byte, BUF_SIZE)}}    // TODO pool

func echo(conn net.Conn) {
	defer conn.Close()
	// defer fmt.Println("")

	// fmt.Printf("Connected to: %s\n", conn.RemoteAddr().String())

	// buf := make([]byte, 1024)
	for {
		// buf := make([]byte, 1024)
		buf := bufPool.Get().([]byte)    // TODO pool
		readSize, err := conn.Read(buf)
		if err == io.EOF {
			return
		}
		if err != nil {
			fmt.Println("Error reading:")
			fmt.Println(err)
			os.Exit(1)
		}

		// fmt.Println(fmt.Sprintf("[%s]", conn.RemoteAddr().String()), string(buf))

		_, err = conn.Write(buf[0:readSize])
		if err != nil {
			fmt.Println("Error writing:")
			fmt.Println(err)
			os.Exit(1)
		}
		bufPool.Put(buf)    // TODO pool
	}
}

func exit_on_error(err error) {
	if err != nil {
		fmt.Println(err)
		os.Exit(1)
	}
}

func main() {
	addr, err := net.ResolveTCPAddr("tcp", HOST+":"+PORT)
	exit_on_error(err)

	listener, err := net.ListenTCP("tcp", addr)
	exit_on_error(err)

	fmt.Println("Listening on port", PORT)

	for {
		conn, err := listener.Accept()
		if err != nil {
			fmt.Println(err)
		} else {
			go echo(conn)
		}
	}
}
