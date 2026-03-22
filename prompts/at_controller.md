# Class AT Command Controller
Class name: AtController

# Member Function

- `Init()`  
  Initialize the device. 
  Sends 'AT+CMGF=0', Use PDU mode.

- `ListSms(SmsStatus status)`  
  Status can be REC_UNREAD=0, REC_READ=1, STO_UNSENT=2, STO_SENT=3, ALL=4.
  Sends "AT+CMGL=<status>.

- `SetOnSend(SendMessageCb sendCb)`  
  Sets the send message callback.

- `SetOnSmsReceived(SmsCb smsCb)`
  Sets the SMS callback. 

- `ReceiveMessage(string message)`
  Called when receive a line of message from the serial port. See Parse Message for details.

# Data Types
- `typedef void(*SendMessageCb)(const string &message)`  
  This function is called when the controller wants to send a line of message.

- `typedef bool(*SmsCb)(const string &sender, const string &body)`
  This function is called when a new message arrives. Returns true if the message is handled and can be deleted.

# State Machine
- Init
- CMGL


# Details

- Parse message.  
  Parse message is called inside receive message. 
  If a line begins with "at+", it is command echo. Ignore it.
  If a line begins with +, it receives a message from the device.
  The formate is like '+CMGL: 4,0,"",,163'. CMGL is the command. It is the response of list message.
  The formate is +CMGL: index,message_status,...
  Parse the index and message_status, ignore other fields. When CMGL is received, it goes to the CMGL state.
  In CMGL state, it will receive a line of heximal numbers. This line is the message body. Parse the message body.
  Decode parsed sender/body to UTF-8 before calling SmsCb.
  Calls SmsCb with sender and message body. If SmsCb returns true, delete the message by send AT+CMGD=index.
  In CMGL state, it may receive another +CMGL line. This is another message.
  When OK is received. List is completed, goes to the Init state.