var udp = require('dgram');
var buffer = require('buffer');

// creating a udp server
var server = udp.createSocket('udp4');

// emits when any error occurs
server.on('error',function(error){
  console.log('Error: ' + error);
  server.close();
});

let tid = 0

// emits on new datagram msg
server.on('message', async function(msg,info) {
  // console.log('!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!')
  console.log(
    'Data received from client : ' + msg.toString('hex'),
    ', addr:', msg.slice(4, 6).toString('hex'),
    ', count:', msg.slice(9, 11).toString('hex')
  );
  console.log('Received %d bytes from %s:%d\n',msg.length, info.address, info.port);

  const msgHex = msg.toString('hex');
  // 64451191 00000000ff31
  tid++
  if (tid > 65535) tid = 0
  const resHex = '64' + '45' + (tid.toString(16).padStart(4, '0')) + msgHex.substr(8, 8)
  console.log('msgHex', msgHex)
  console.log('resHex', resHex)

  const buffer = Buffer.from(resHex, 'hex')
  server.send(buffer,info.port,info.address,function(error){
    if(error){
      // client.close();
    }else{
      console.log('Data sent !!!');
    }
  });
});

//emits when socket is ready and listening for datagram msgs
server.on('listening',function(){
  var address = server.address();
  var port = address.port;
  var family = address.family;
  var ipaddr = address.address;
  console.log('Server is listening at port:', port);
  console.log('Server ip:', ipaddr);
  console.log('Server is IP4/IP6:', family);
});

//emits after the socket is closed using socket.close();
server.on('close',function(){
  console.log('Socket is closed !');
});

server.bind(5683);
