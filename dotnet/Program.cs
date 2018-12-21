using System.Net.Sockets;
using System.Net;
using System;
using System.Threading.Tasks;
using System.Text;
using System.IO;
 
namespace TcpSrvAsync
{
    class Program
    {
		static int PORT = 9000;
        static void Main(string[] args)
        {
            AysncEchoServer().Wait();
        }
 
        async static Task AysncEchoServer()
        {           
            TcpListener listener = new TcpListener(IPAddress.Any, PORT);
            listener.Start();            
            while (true)
            {
                // 비동기 Accept                
                TcpClient tc = await listener.AcceptTcpClientAsync().ConfigureAwait(false);                
                 
                // 새 쓰레드에서 처리
                Task.Factory.StartNew(AsyncTcpProcess, tc);                
            }
        }
 
        async static void AsyncTcpProcess(object o)
        {
            TcpClient tc = (TcpClient)o;
 
            int MAX_SIZE = 1024;  // 가정
            NetworkStream stream = tc.GetStream();
 
            // 비동기 수신            
            var buff = new byte[MAX_SIZE];

			try {
				while (true)
				{
					var nbytes = await stream.ReadAsync(buff, 0, buff.Length).ConfigureAwait(false);
					if (nbytes <= 0)
						break;

					string msg = Encoding.ASCII.GetString(buff, 0, nbytes);
					// Console.WriteLine($"{msg} at {DateTime.Now}");                
					await stream.WriteAsync(buff, 0, nbytes).ConfigureAwait(false);
				}
			}
			catch (ArgumentNullException e) {
				Console.WriteLine(">>>>> ArgumentNullException");
			}
			catch (ArgumentOutOfRangeException e) {
				Console.WriteLine(">>>>> ArgumentOutOfRangeException");
			}
			catch (IOException e) {
				Console.WriteLine(">>>>> IOException");
			}
			catch (ObjectDisposedException e) {
				Console.WriteLine(">>>>> ObjectDisposedException ");
			}
			finally {
				stream.Close();
				tc.Close();
			}
        }
    }
}



// using System;
// using System.Net;
// using System.Net.Sockets;
// using System.Threading;
//
// namespace AsyncSocketServer
// {
// 	public class EchoClient
// 	{
// 		private const int BUFFER_SIZE = 1024;
//
// 		private Socket soc;
// 		private SocketAsyncEventArgs recvArgs;
// 		private SocketAsyncEventArgs sendArgs;
//
// 		public EchoClient(Socket accepted)
// 		{
// 			soc = accepted;
//
// 			recvArgs = new SocketAsyncEventArgs();
// 			recvArgs.Completed += new EventHandler<SocketAsyncEventArgs>(OnReceive);
//
// 			sendArgs = new SocketAsyncEventArgs();
// 			sendArgs.Completed += new EventHandler<SocketAsyncEventArgs>(OnSend);
//
// 			recvArgs.SetBuffer(new byte[BUFFER_SIZE], 0, BUFFER_SIZE);
// 			soc.ReceiveAsync(recvArgs);
// 		}
//
// 		private void OnReceive(object sender, SocketAsyncEventArgs args)
// 		{
// 			Console.WriteLine(">>>>> OnReceive");
// 			if (!soc.Connected || args.BytesTransferred <= 0) {
// 				soc.Dispose();
// 				Console.WriteLine("OnReceive error");
// 				return;
// 			}
//
// 			sendArgs.SetBuffer(args.Buffer, 0, args.BytesTransferred);
// 			soc.SendAsync(sendArgs);
// 		}
//
// 		private void OnSend(object sender, SocketAsyncEventArgs args)
// 		{
// 			Console.WriteLine(">>>>> OnSend");
// 			if (!soc.Connected || args.BytesTransferred <= 0) {
// 				soc.Dispose();
// 				Console.WriteLine("OnSend error");
// 				return;
// 			}
//
// 			recvArgs.SetBuffer(args.Buffer, 0, BUFFER_SIZE);
// 			soc.ReceiveAsync(recvArgs);
// 		}
// 	};
//
// 	public class Tester
// 	{
// 		private Socket _listener;
//
// 		public Tester()
// 		{
// 			_listener = new Socket(AddressFamily.InterNetwork, SocketType.Stream, ProtocolType.Tcp);
// 			IPEndPoint ep = new IPEndPoint(IPAddress.Any, 9000);
// 			_listener.Bind(ep); 
// 			_listener.Listen(1024); 
//
// 			SocketAsyncEventArgs args = new SocketAsyncEventArgs();
// 			args.Completed += new EventHandler<SocketAsyncEventArgs>(OnAccept);
// 			_listener.AcceptAsync(args);
// 		}
//
// 		private void OnAccept(object sender, SocketAsyncEventArgs e)
// 		{
// 			Console.WriteLine(">>>>> OnAccept");
// 			Socket accepted = e.AcceptSocket;
// 			if (accepted == null) {
// 				Console.WriteLine("OnAccept error 1");
// 				return;
// 			}
// 			if (!accepted.Connected) {
// 				accepted.Dispose();
// 				Console.WriteLine("OnAccept error 2");
// 				return;
// 			}
//
// 			new EchoClient(accepted);
//
// 			e.AcceptSocket = null;
// 			_listener.AcceptAsync(e);
// 		}
//
// 		public static void Main(string[] args)
// 		{
// 			// Console.WriteLine(">>>>> 1");
// 			new Tester();
// 			// Console.WriteLine(">>>>> 2");
//
// 			while(true)
// 			{
// 				// Console.WriteLine(">>>>> 3");
// 				Thread.Sleep(1000);
// 			}
// 		}
// 	}
// }

