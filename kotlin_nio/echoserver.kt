import java.io.IOException
import java.net.InetSocketAddress
import java.nio.ByteBuffer
import java.nio.channels.SelectionKey
import java.nio.channels.Selector
import java.nio.channels.ServerSocketChannel
import java.nio.channels.SocketChannel

@Throws(IOException::class)
fun main(args: Array<String>) {
    val PORT_NUMBER = 9000
    val BUFFER_SIZE = 1024

    val selector = Selector.open()

    val channel = ServerSocketChannel.open()
    val address = InetSocketAddress("localhost", PORT_NUMBER)
    channel.apply {
        bind(address)
        configureBlocking(false)
        register(selector, channel.validOps())
    }

    while (true) {
        val channelCount = selector.select()
		if (channelCount <= 0)
			continue
        val keys = selector.selectedKeys()
        val iterator = keys.iterator()
        while (iterator.hasNext()) {
            val key = iterator.next()
            iterator.remove()

            if (key.isAcceptable) {
                channel.accept().apply {
                    configureBlocking(false)
                    register(selector, SelectionKey.OP_READ, ByteBuffer.allocate(BUFFER_SIZE))
                }
            } else if (key.isReadable) {
                val client = key.channel() as SocketChannel
                val buffer = key.attachment() as ByteBuffer
                if (client.read(buffer) < 0) {
                    key.cancel();
                    client.close();
                } else {
                    buffer.flip(); // read from the buffer
                    client.write(buffer);
                    buffer.clear(); // write into the buffer            }
                }
            }
        }
    }
}
