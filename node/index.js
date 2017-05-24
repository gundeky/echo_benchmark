var net = require('net');

// var count = 0;
net.createServer(function (client) {
	client.on('data', function (data) {
		// console.log(++count);
		client.write(data);
	});
}).listen(9000, function () {
	console.info('server listening on 9000');
});

