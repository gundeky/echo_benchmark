using System;
using System.Net;
using System.Net.Sockets;
using System.Threading;

namespace AsyncSocketServer
{
	public class EchoClient
	{
		private const int BUFFER_SIZE = 1024;

		private Socket soc;
		private SocketAsyncEventArgs recvArgs;
		private SocketAsyncEventArgs sendArgs;

		public EchoClient(Socket accepted)
		{
			soc = accepted;

			recvArgs = new SocketAsyncEventArgs();
			recvArgs.Completed += new EventHandler<SocketAsyncEventArgs>(OnReceive);

			sendArgs = new SocketAsyncEventArgs();
			sendArgs.Completed += new EventHandler<SocketAsyncEventArgs>(OnSend);

			recvArgs.SetBuffer(new byte[BUFFER_SIZE], 0, BUFFER_SIZE);
			soc.ReceiveAsync(recvArgs);
		}

		private void OnReceive(object sender, SocketAsyncEventArgs args)
		{
			if (!soc.Connected || args.BytesTransferred <= 0) {
				soc.Dispose();
				Console.WriteLine("OnReceive error");
				return;
			}

			sendArgs.SetBuffer(args.Buffer, 0, args.BytesTransferred);
			soc.SendAsync(sendArgs);
		}

		private void OnSend(object sender, SocketAsyncEventArgs args)
		{
			if (!soc.Connected || args.BytesTransferred <= 0) {
				soc.Dispose();
				Console.WriteLine("OnSend error");
				return;
			}

			recvArgs.SetBuffer(args.Buffer, 0, BUFFER_SIZE);
			soc.ReceiveAsync(recvArgs);
		}
	};

	public class Tester
	{
		private Socket _listener;

		public Tester()
		{
			_listener = new Socket(AddressFamily.InterNetwork, SocketType.Stream, ProtocolType.Tcp);
			IPEndPoint ep = new IPEndPoint(IPAddress.Any, 9000);
			_listener.Bind(ep); 
			_listener.Listen(1024); 

			SocketAsyncEventArgs args = new SocketAsyncEventArgs();
			args.Completed += new EventHandler<SocketAsyncEventArgs>(OnAccept);
			_listener.AcceptAsync(args);
		}

		private void OnAccept(object sender, SocketAsyncEventArgs e)
		{
			Socket accepted = e.AcceptSocket;
			if (accepted == null) {
				Console.WriteLine("OnAccept error 1");
				return;
			}
			if (!accepted.Connected) {
				accepted.Dispose();
				Console.WriteLine("OnAccept error 2");
				return;
			}

			new EchoClient(accepted);

			e.AcceptSocket = null;
			_listener.AcceptAsync(e);
		}

		public static void Main(string[] args)
		{
			new Tester();

			while(true)
			{
				Thread.Sleep(1000);
			}
		}
	}
}

